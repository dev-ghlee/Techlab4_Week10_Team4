#include "EnginePCH.h"
#include "FbxImporter.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Math/Quat.h"
#include "Render/SkeletalMeshData.h"

#include <algorithm>
#include <cmath>
#include <fbxsdk.h>
#include <filesystem>
#include <format>
#include <limits>
#include <system_error>
#include <utility>

// Mesh Node와 해당 Skin, Cluster의 연결 관계.
// 모든 포인터는 SDK 객체를 참조하며 소유하지 않는다.
struct FFbxSkinCluster
{
	// Transform과 재질을 확인할 Mesh의 Node.
	FbxNode* MeshNode = nullptr;

	// 이 Cluster가 속한 Skin Deformer.
	FbxSkin* Skin = nullptr;

	// 연결 본, Control Point Weight, 바인드 정보를 제공한다.
	FbxCluster* Cluster = nullptr;
};

// 최종 Weight를 만들기 전까지 보관하는 본 영향 하나.
struct FFbxBoneInfluence
{
	int32 BoneIndex = -1;
	double Weight = 0.0;
};

// 이번 임포트 작업에서만 사용하는 임시 정보.
struct FImportContext
{
	// 삼각형화가 끝난 Mesh를 소유한 Node 목록.
	// SDK 객체를 참조할 뿐, 소유하거나 직접 해제하지 않는다.
	TArray<FbxNode*> MeshNodes;

	// 각 Mesh Node에 연결된 Skin과 Cluster 정보.
	TArray<FFbxSkinCluster> SkinClusters;

	// 공통 SkeletalMesh 공간의 기준으로 사용할 본 Node.
	FbxNode* RootBoneNode = nullptr;

	// 부모부터 자식 순서로 저장한 SDK Node 목록.
	TArray<FbxNode*> BoneNodes;

	// SDK Node와 엔진 본 인덱스의 대응 관계.
	TMap<FbxNode*, int32> BoneIndexMap;

	// BoneNodes와 같은 순서의 루트 기준 바인드 전역 행렬.
	TArray<FbxMatrix> BoneBindGlobals;

	// Scene 공간에서 루트의 위치·회전을 기준으로 하는 공간으로 변환한다.
	// 크기를 변경하지 않으므로 Scene의 cm 단위와 원본 스케일을 유지한다.
	FbxMatrix SceneToSkeletal;

	// 각 Mesh의 Control Point를 루트 기준 공간으로 변환하는 행렬.
	TMap<FbxNode*, FbxMatrix> MeshToSkeletalMatrices;

	// Mesh Node별 Control Point에 영향을 주는 원본 본 영향 목록.
	TMap<FbxNode*, TArray<TArray<FFbxBoneInfluence>>> ControlPointInfluences;

	// 최대 영향 개수 선정과 정규화가 끝난 Control Point별 스키닝 정보.
	// 이후 정점을 분리할 때 해당 Control Point의 값을 복사한다.
	TMap<FbxNode*, TArray<FSkinWeightInfo>> ControlPointSkinWeights;

	// Mesh Node의 로컬 재질 인덱스에서 전체 MaterialSlots 인덱스로의 대응표.
	// 재질이 없는 Mesh는 기본 슬롯을 가리키는 항목 하나를 둔다.
	TMap<FbxNode*, TArray<uint16>> MeshMaterialIndices;

	// PolygonIndex * 3 + CornerIndex에서 전체 ReferenceVertices 인덱스로의 대응표.
	// 이후 삼각형 인덱스와 Section을 구성할 때 사용한다.
	TMap<FbxNode*, TArray<uint32>> MeshPolygonVertexIndices;
};

bool FFbxImporter::IsWithinFloatRange(double Value)
{
	return std::isfinite(Value) && std::abs(Value) <= std::numeric_limits<float>::max();
}

bool FFbxImporter::ConvertFbxMatrixToTransform(const FbxMatrix& InMatrix, FTransform& OutTransform, FString& OutError)
{
	FbxVector4 Translation; 
	FbxVector4 Shearing; 
	FbxVector4 Scaling;

	FbxQuaternion Rotation;
	double Sign = 1.0;

	// 위치·회전·전단·스케일·행렬식 부호를 SDK에서 추출한다.
	InMatrix.GetElements(Translation, Rotation, Shearing, Scaling, Sign);

	// SDK에서 XYZ 회전각을 구한다. 반환 단위는 도(degree)다.
	const FbxVector4 Euler = Rotation.DecomposeSphericalXYZ();

	// 반사 부호가 Scaling에 이미 포함됐다면 중복 적용하지 않는다.
	const bool bNegativeScale = (Scaling[0] < 0) ^ (Scaling[1] < 0) ^ (Scaling[2] < 0);
	const double ScaleSign = (bNegativeScale == (Sign < 0) ? 1.0 : -1.0);

	FTransform Converted;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double SignedScale = Scaling[Axis] * ScaleSign;

		if (!IsWithinFloatRange(Translation[Axis]) || !IsWithinFloatRange(Euler[Axis])
			|| !IsWithinFloatRange(SignedScale) || std::abs(SignedScale) <= 1e-8)
		{
			OutError = "Bone transform contains unsupported values.";
			return false;
		}

		Converted.Location[Axis] = static_cast<float>(Translation[Axis]);
		Converted.Scale[Axis] = static_cast<float>(SignedScale);
	}

	// SDK의 X·Y·Z 회전을 프로젝트의 Roll·Pitch·Yaw에 대응시킨다.
	Converted.Rotation = FRotator(
		static_cast<float>(Euler[1]),
		static_cast<float>(Euler[2]),
		static_cast<float>(Euler[0]));

	// 엔진에 저장한 회전을 다시 Quaternion으로 만들어 복원한다.
	const FQuat EngineRotation = Converted.Rotation.Quaternion();

	const FbxMatrix Rebuilt(
		FbxVector4(Converted.Location.X, Converted.Location.Y, Converted.Location.Z),
		FbxQuaternion(EngineRotation.X, EngineRotation.Y, EngineRotation.Z, EngineRotation.W),
		FbxVector4(Converted.Scale.X, Converted.Scale.Y, Converted.Scale.Z));

	// 전단 손실이나 회전 변환 오차가 허용 범위를 넘으면 실패한다.
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			const double Expected = InMatrix.Get(Row, Column);
			const double Actual = Rebuilt.Get(Row, Column);
			const double Tolerance = 1e-5 * (1.0 + std::abs(Expected));

			if (!std::isfinite(Expected) || !std::isfinite(Actual)
				|| std::abs(Expected - Actual) > Tolerance)
			{
				OutError = "Bone transform cannot be preserved as TRS.";
				return false;
			}
		}
	}

	OutTransform = Converted;
	return true;
}

bool FFbxImporter::BuildNormalMatrix(const FbxMatrix& MeshToSkeletal, FMatrix& OutNormalMatrix, FString& OutError)
{
	const double Determinant = MeshToSkeletal.Determinant();
	if (!std::isfinite(Determinant) || Determinant == 0.0)
	{
		OutError = "Mesh transform is not invertible.";
		return false;
	}

	const FbxMatrix SkeletalToMesh = MeshToSkeletal.Inverse();
	const FbxVector4 Origin = SkeletalToMesh.MultNormalize(FbxVector4(0, 0, 0, 1));

	FMatrix NormalMatrix = FMatrix::Identity;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		FbxVector4 Point(0, 0, 0, 1);
		Point[Axis] = 1.0;

		// 역행렬로 변환한 축 끝점에서 이동을 제거한다.
		const FbxVector4 InverseAxis = SkeletalToMesh.MultNormalize(Point) - Origin;

		for (int32 Component = 0; Component < 3; ++Component)
		{
			const double Value = InverseAxis[Component];

			if (!IsWithinFloatRange(Value))
			{
				OutError = "Mesh normal matrix exceeds the engine numeric range.";
				return false;
			}

			// 역행렬의 행과 열을 바꿔 역전치 행렬을 구성한다.
			NormalMatrix[Component][Axis] = static_cast<float>(Value);
		}
	}

	OutNormalMatrix = NormalMatrix;
	return true;
}

bool FFbxImporter::ReadMaterialBaseColor(FbxSurfaceMaterial* Material, FVector4& OutBaseColor, FString& OutError)
{
	// Phong은 Lambert를 상속하므로 두 표준 재질을 함께 처리한다.
	FbxSurfaceLambert* Lambert = FbxCast<FbxSurfaceLambert>(Material);
	if (!Lambert)
	{
		OutError = "Only standard Lambert and Phong materials are supported.";
		return false;
	}

	const FbxDouble3 Diffuse = Lambert->Diffuse.Get();
	const double DiffuseFactor = Lambert->DiffuseFactor.Get();

	// 현재 구현은 불투명 재질을 기준으로 Alpha를 1로 둔다.
	FVector4 BaseColor(1, 1, 1, 1);

	for (int32 Channel = 0; Channel < 3; ++Channel)
	{
		// 재질의 Diffuse 강도를 색상에 반영한다.
		const double Value = Diffuse[Channel] * DiffuseFactor;

		if (!IsWithinFloatRange(Value))
		{
			OutError = "Material base color exceeds the engine numeric range.";
			return false;
		}

		BaseColor[Channel] = static_cast<float>(Value);
	}

	// 성공한 경우에만 출력값을 변경한다.
	OutBaseColor = BaseColor;
	return true;
}

bool FFbxImporter::ReadMaterialTexturePath(
	const FbxProperty& Property, const FString& FbxPath, FString& OutTexturePath, FString& OutError)
{
	OutTexturePath.clear();

	// 텍스처 연결이 없는 재질도 허용한다.
	if (!Property.IsValid())
	{
		return true;
	}

	const int32 TextureCount = Property.GetSrcObjectCount<FbxTexture>();
	if (TextureCount == 0)
	{
		return true;
	}

	// 현재 재질 슬롯은 채널마다 파일 텍스처 하나만 저장한다.
	FbxFileTexture* Texture = TextureCount == 1
		? FbxCast<FbxFileTexture>(Property.GetSrcObject<FbxTexture>(0))
		: nullptr;

	if (!Texture)
	{
		OutError = "Only one file texture per material channel is supported.";
		return false;
	}

	// 함께 옮겨진 에셋을 지원하도록 상대 경로를 먼저 확인한다.
	const char* FileNames[] =
	{
		Texture->GetRelativeFileName(),
		Texture->GetFileName()
	};

	for (const char* FileName : FileNames)
	{
		if (!FileName || FileName[0] == '\0')
		{
			continue;
		}

		std::filesystem::path Candidate(FileName);
		if (!Candidate.is_absolute())
		{
			Candidate = std::filesystem::path(FbxPath).parent_path() / Candidate;
		}

		Candidate = Candidate.lexically_normal();

		std::error_code Error;
		if (std::filesystem::is_regular_file(Candidate, Error))
		{
			OutTexturePath = Candidate.generic_string();
			return true;
		}
	}

	OutError = "FBX material texture file was not found.";
	return false;
}

bool FFbxImporter::ValidateMaterialTextureUVSet(
	const FbxProperty& Property, const FbxMesh& Mesh,
	const char* UVSetName, FString& OutError)
{
	if (!Property.IsValid())
	{
		return true;
	}

	const int32 TextureCount = Property.GetSrcObjectCount<FbxTexture>();

	// 텍스처를 사용하지 않는 채널은 검사할 필요가 없다.
	if (TextureCount == 0)
	{
		return true;
	}

	FbxFileTexture* Texture = TextureCount == 1
		? FbxCast<FbxFileTexture>(Property.GetSrcObject<FbxTexture>(0))
		: nullptr;

	if (!Texture)
	{
		OutError = "Only one file texture per material channel is supported.";
		return false;
	}

	// 현재 정점에는 UV 좌표만 저장하므로 다른 투영 방식은 지원하지 않는다.
	if (Texture->GetMappingType() != FbxTexture::eUV)
	{
		OutError = "Only UV-mapped material textures are supported.";
		return false;
	}

	if (!UVSetName || UVSetName[0] == '\0')
	{
		OutError = "A textured mesh requires a valid first UV set.";
		return false;
	}

	const FbxString RequestedUVSet = Texture->UVSet.Get();

	// 이름이 비어 있으면 첫 UV 세트를 사용하는 것으로 처리한다.
	if (RequestedUVSet.IsEmpty())
	{
		return true;
	}

	// SDK 속성의 초기값인 "default"를 기본 세트 별칭으로 허용한다.
	// 실제로 "default"라는 UV 세트가 있으면 해당 이름을 우선한다.
	const bool bDefaultAlias = RequestedUVSet.Compare("default") == 0 && !Mesh.GetElementUV("default");

	if (RequestedUVSet.Compare(UVSetName) != 0 && !bDefaultAlias)
	{
		OutError = "Material texture references a UV set other than the imported first UV set.";
		return false;
	}

	return true;
}

bool FFbxImporter::ReadPolygonMaterialIndex(const FbxMesh& Mesh, int32 PolygonIndex, int32& OutMaterialIndex, FString& OutError)
{
	const FbxGeometryElementMaterial* MaterialElement = Mesh.GetElementMaterial(0);

	// 재질 할당 정보가 없으면 첫 로컬 재질을 사용한다.
	// 재질이 없는 Node는 대응표의 첫 항목이 기본 슬롯을 가리킨다.
	OutMaterialIndex = 0;

	if (!MaterialElement || MaterialElement->GetMappingMode() == FbxLayerElement::eNone)
	{
		return true;
	}

	int32 MappingIndex = 0;

	switch (MaterialElement->GetMappingMode())
	{
	case FbxLayerElement::eAllSame:
		// 모든 Polygon이 IndexArray[0]의 재질을 사용한다.
		MappingIndex = 0;
		break;

	case FbxLayerElement::eByPolygon:
		// Polygon마다 재질 인덱스가 하나씩 있다.
		MappingIndex = PolygonIndex;
		break;

	default:
		OutError = "Unsupported FBX material mapping mode.";
		return false;
	}

	// 재질은 일반 UV처럼 DirectArray를 조회하지 않는다.
	// IndexArray의 값 자체가 Node에 연결된 재질의 인덱스다.
	const FbxLayerElement::EReferenceMode ReferenceMode = MaterialElement->GetReferenceMode();

	if (ReferenceMode != FbxLayerElement::eIndexToDirect
		&& ReferenceMode != FbxLayerElement::eIndex)
	{
		OutError = "Unsupported FBX material reference mode.";
		return false;
	}

	const FbxLayerElementArrayTemplate<int>& MaterialIndices = MaterialElement->GetIndexArray();

	if (MappingIndex < 0 || MappingIndex >= MaterialIndices.GetCount())
	{
		OutError = "FBX polygon material mapping index is out of range.";
		return false;
	}

	OutMaterialIndex = MaterialIndices.GetAt(MappingIndex);

	// -1은 미할당 재질로 받아 기본 슬롯에 연결한다.
	if (OutMaterialIndex < -1)
	{
		OutError = "FBX polygon contains an invalid material index.";
		return false;
	}

	return true;
}

bool FFbxImporter::BuildTangentSpace(FSkeletalMeshData& MeshData, FString& OutError)
{
	auto& Vertices = MeshData.ReferenceVertices;
	const auto& Indices = MeshData.Indices;

	const int32 VertexCount = Vertices.Num();
	const int32 IndexCount = Indices.Num();
	constexpr float UVTolerance = 1e-8f;
	constexpr float VectorToleranceSquared = 1e-12f;

	if (VertexCount == 0 || IndexCount == 0 || IndexCount % 3 != 0)
	{
		OutError = "Tangent generation requires valid triangle data.";
		return false;
	}

	TArray<FVector> TangentSums;
	TArray<FVector> BitangentSums;
	TangentSums.SetNum(VertexCount);
	BitangentSums.SetNum(VertexCount);

	for (int32 Index = 0; Index < IndexCount; Index += 3)
	{
		const uint32 TriangleIndices[3] =
		{
			Indices[Index], Indices[Index + 1], Indices[Index + 2]
		};

		for (uint32 VertexIndex : TriangleIndices)
		{
			if (VertexIndex >= static_cast<uint32>(VertexCount))
			{
				OutError = "Tangent generation found an invalid vertex index.";
				return false;
			}
		}

		const FVertex& VertexA = Vertices[TriangleIndices[0]];
		const FVertex& VertexB = Vertices[TriangleIndices[1]];
		const FVertex& VertexC = Vertices[TriangleIndices[2]];

		const FVector Edge1 = VertexB.Position - VertexA.Position;
		const FVector Edge2 = VertexC.Position - VertexA.Position;
		const FVector2 DeltaUV1 = VertexB.UV - VertexA.UV;
		const FVector2 DeltaUV2 = VertexC.UV - VertexA.UV;

		const float Determinant = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;

		if (!std::isfinite(Determinant))
		{
			OutError = "Tangent generation encountered an invalid UV determinant.";
			return false;
		}

		// UV가 없거나 UV 삼각형의 면적이 거의 0이면 계산을 건너뛴다.
		// 해당 정점에는 아래에서 노멀과 수직인 기본 탄젠트를 만든다.
		if (std::abs(Determinant) <= UVTolerance)
		{
			continue;
		}

		const FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) / Determinant;
		const FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) / Determinant;

		for (uint32 VertexIndex : TriangleIndices)
		{
			TangentSums[VertexIndex] += Tangent;
			BitangentSums[VertexIndex] += Bitangent;
		}
	}

	for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		FVertex& Vertex = Vertices[VertexIndex];
		const FVector Normal = Vertex.Normal.GetSafeNormal(VectorToleranceSquared);

		if (Normal.SizeSquared() <= VectorToleranceSquared)
		{
			OutError = "Tangent generation requires a valid vertex normal.";
			return false;
		}

		// 탄젠트에서 노멀 방향 성분을 제거해 두 축을 수직으로 만든다.
		FVector Tangent = TangentSums[VertexIndex] - Normal * Normal.Dot(TangentSums[VertexIndex]);
		float TangentLengthSquared = Tangent.SizeSquared();

		if (!std::isfinite(TangentLengthSquared))
		{
			OutError = "Generated tangent exceeds the supported numeric range.";
			return false;
		}

		if (TangentLengthSquared <= VectorToleranceSquared)
		{
			// 노멀과 거의 평행하지 않은 축을 선택해 기본 탄젠트를 만든다.
			const FVector Axis = std::abs(Normal.X) < 0.9f ? FVector(1, 0, 0) : FVector(0, 1, 0);
			Tangent = Axis - Normal * Normal.Dot(Axis);
			TangentLengthSquared = Tangent.SizeSquared();
		}

		Tangent = Tangent / std::sqrt(TangentLengthSquared);

		// 셰이더에서 B = cross(N, T) * Tangent.W로 복원할 부호다.
		const float Orientation = Normal.Cross(Tangent).Dot(BitangentSums[VertexIndex]);

		if (!std::isfinite(Orientation))
		{
			OutError = "Generated bitangent orientation is invalid.";
			return false;
		}

		const float Handedness = Orientation < 0.0f ? -1.0f : 1.0f;
		Vertex.Tangent = FVector4(Tangent, Handedness);
	}

	return true;
}

bool FFbxImporter::ValidateSkeletalMeshData(const FSkeletalMeshData& MeshData, FString& OutError)
{
	OutError.clear();

	const int32 VertexCount = MeshData.ReferenceVertices.Num();
	const int32 IndexCount = MeshData.Indices.Num();
	const int32 BoneCount = MeshData.RefSkeleton.RawRefBoneInfo.Num();

	if (VertexCount == 0 || IndexCount == 0 || IndexCount % 3 != 0)
	{
		OutError = "Skeletal mesh requires vertices and triangle indices.";
		return false;
	}

	// 같은 정점 인덱스로 원본 정점과 스키닝 정보를 읽을 수 있어야 한다.
	if (MeshData.SkinWeights.Num() != VertexCount)
	{
		OutError = "Skin weight count does not match the vertex count.";
		return false;
	}

	// 같은 본 인덱스로 본 정보·기준 포즈·역바인드를 읽어야 한다.
	if (BoneCount == 0
		|| MeshData.RefSkeleton.RawRefBonePose.Num() != BoneCount
		|| MeshData.RefBasesInvMatrix.Num() != BoneCount)
	{
		OutError = "Bone information, reference pose and inverse bind counts must match.";
		return false;
	}

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const int32 ParentIndex = MeshData.RefSkeleton.RawRefBoneInfo[BoneIndex].ParentIndex;

		// 현재 규약은 0번 본 하나를 루트로 사용한다.
		// 나머지 본의 부모는 반드시 자신보다 먼저 저장되어야 한다.
		if ((BoneIndex == 0 && ParentIndex != -1)
			|| (BoneIndex > 0 && (ParentIndex < 0 || ParentIndex >= BoneIndex)))
		{
			OutError = "Invalid parent bone index or bone hierarchy order.";
			return false;
		}
	}

	for (uint32 VertexIndex : MeshData.Indices)
	{
		if (VertexIndex >= static_cast<uint32>(VertexCount))
		{
			OutError = "Triangle index is outside the reference vertex array.";
			return false;
		}
	}

	for (const FSkinWeightInfo& SkinWeight : MeshData.SkinWeights)
	{
		for (uint32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
		{
			const int32 BoneIndex = SkinWeight.InfluenceBones[InfluenceIndex];

			// Weight가 0인 미사용 슬롯의 본 인덱스는 검사하지 않는다.
			if (SkinWeight.InfluenceWeights[InfluenceIndex] > 0.0f
				&& (BoneIndex < 0 || BoneIndex >= BoneCount))
			{
				OutError = "Skin weight references an invalid bone index.";
				return false;
			}
		}
	}

	if (MeshData.Sections.IsEmpty() || MeshData.MaterialSlots.IsEmpty())
	{
		OutError = "Skeletal mesh requires render sections and material slots.";
		return false;
	}

	// 앞 Section의 끝과 다음 Section의 시작이 같으면 겹침·누락이 없다.
	uint64 ExpectedBaseIndex = 0;

	for (const FSkelMeshRenderSection& Section : MeshData.Sections)
	{
		// 곱셈과 덧셈에서 uint32 오버플로가 발생하지 않도록 확장한다.
		const uint64 SectionIndexCount = static_cast<uint64>(Section.NumTriangles) * 3;
		const uint64 SectionEnd = static_cast<uint64>(Section.BaseIndex) + SectionIndexCount;

		if (Section.NumTriangles == 0
			|| Section.BaseIndex != ExpectedBaseIndex
			|| SectionEnd > static_cast<uint64>(IndexCount))
		{
			OutError = "Section index ranges are invalid, overlapping or incomplete.";
			return false;
		}

		if (Section.MaterialIndex >= MeshData.MaterialSlots.Num())
		{
			OutError = "Section references an invalid material slot.";
			return false;
		}

		ExpectedBaseIndex = SectionEnd;
	}

	// 마지막 Section까지 모든 삼각형 인덱스를 포함해야 한다.
	if (ExpectedBaseIndex != static_cast<uint64>(IndexCount))
	{
		OutError = "Sections do not cover the entire index array.";
		return false;
	}

	return true;
}

bool FFbxImporter::LoadScene(const FString& Path, FSkeletalMeshData& OutData, FString& OutError)
{
	OutError.clear();

	// 함수가 끝나면 Manager의 Destroy()를 자동으로 호출한다.
	FbxAutoDestroyPtr<FbxManager> Manager(FbxManager::Create());
	if (!Manager)
	{
		OutError = "Failed to create FBX manager.";
		return false;
	}

	// Manager가 소유하므로 Manager 정리 시 함께 해제된다.
	FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager.Get(), IOSROOT);
	FbxScene* Scene = FbxScene::Create(Manager.Get(), "ImportScene");
	FbxImporter* Importer = FbxImporter::Create(Manager.Get(), "Importer");

	if (!IOSettings || !Scene || !Importer)
	{
		OutError = "Failed to create FBX import objects.";
		return false;
	}

	Manager->SetIOSettings(IOSettings);

	// 파일을 열고 형식을 자동 판별한 뒤 내용을 Scene으로 읽는다.
	if (!Importer->Initialize(Path.c_str(), -1, IOSettings)
		|| !Importer->Import(Scene))
	{
		OutError = Importer->GetStatus().GetErrorString();
		return false;
	}

	// 이후 계층 순회를 시작할 최상위 Node를 확인한다.
	if (!Scene->GetRootNode())
	{
		OutError = "FBX scene has no root node.";
		return false;
	}

	// 엔진 축: 오른쪽 +Y, 위쪽 +Z, 전방 +X.
	// SDK 문자열 순서는 오른쪽 / 위쪽 / 화면 안쪽 방향이다.
	FbxAxisSystem EngineAxis;
	if (!FbxAxisSystem::ParseAxisSystem("yzx", EngineAxis))
	{
		OutError = "Failed to define the engine axis system.";
		
		return false;
	}

	// 좌표계가 다를 때 변환하며 좌우 손잡이 변경도 처리한다.
	if (Scene->GetGlobalSettings().GetAxisSystem() != EngineAxis)
	{
		EngineAxis.DeepConvertScene(Scene);
	}

	// 엔진 길이 단위를 센티미터로 통일한다.
	const FbxSystemUnit& EngineUnit = FbxSystemUnit::cm;
	if (Scene->GetGlobalSettings().GetSystemUnit() != EngineUnit)
	{
		// 기본 옵션을 사용해 Cluster의 바인드 정보도 함께 변환한다.
		EngineUnit.ConvertScene(Scene);
	}

	// Scene의 지원 가능한 Geometry를 삼각형화한다.
	// true: 기존 Geometry를 삼각형화한 결과로 교체한다.
	// false: 구형 삼각형화 방식을 사용하지 않는다.
	FbxGeometryConverter GeometryConverter(Manager.Get());

	if (!GeometryConverter.Triangulate(Scene, true, false))
	{
		OutError = "Failed to triangulate FBX scene.";
		return false;
	}

	// 변환 후 노드가 실제로 사용하는 메시를 수집한다.
	// Scene의 전체 Geometry 목록에는 노드에서 사용하지 않는 원본도 있을 수 있다.
	FImportContext Context;
	LoadSceneRecursive(Scene->GetRootNode(), Context);

	if (Context.MeshNodes.IsEmpty())
	{
		OutError = "FBX scene contains no mesh nodes to import.";
		return false;
	}

	// 정점 추출에 사용할 Polygon을 직접 확인한다. 빈 메시 노드는 수집 단계에서 제외한다.
	for (FbxNode* MeshNode : Context.MeshNodes)
	{
		FbxMesh* Mesh = MeshNode->GetMesh();
		for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
		{
			const int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);
			if (PolygonSize != 3)
			{
				OutError = std::format("FBX mesh '{}' polygon {} has {} vertices after triangulation.",
					MeshNode->GetName(), PolygonIndex, PolygonSize);
				return false;
			}
		}
	}

	// 수집한 Mesh Node마다 연결된 Skin을 확인한다.
	for (FbxNode* MeshNode : Context.MeshNodes)
	{
		FbxMesh* Mesh = MeshNode->GetMesh();
		const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

		// Skin이 없으면 반복문을 실행하지 않는다.
		for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
		{
			// eSkin으로 조회했으므로 FbxSkin으로 변환한다.
			FbxSkin* Skin = static_cast<FbxSkin*>(
				Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));

			if (!Skin)
			{
				OutError = "Failed to access FBX skin deformer.";
				return false;
			}

			// Skin에 연결된 각 Cluster를 수집한다.
			for (int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ++ClusterIndex)
			{
				FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);

				// 연결 본이 없는 Cluster는 스키닝에 사용할 수 없다.
				if (!Cluster || !Cluster->GetLink())
				{
					OutError = "FBX cluster has no valid linked bone.";
					return false;
				}

				Context.SkinClusters.Add({ MeshNode, Skin, Cluster });
			}
		}
	}

	// SkeletalMesh를 구성할 연결 정보가 있는지 확인한다.
	if (Context.SkinClusters.IsEmpty())
	{
		OutError = "FBX scene contains no skin clusters.";
		return false;
	}

	for (const FFbxSkinCluster& SkinCluster : Context.SkinClusters)
	{
		// 현재 구현은 Mesh 하나에 Skin Deformer 하나만 지원한다.
		// 복수 Skin의 변형을 단순히 합치면 원본 동작과 달라질 수 있다.
		FbxMesh* Mesh = SkinCluster.MeshNode->GetMesh();
		if (Mesh->GetDeformerCount(FbxDeformer::eSkin) > 1)
		{
			OutError = "Multiple skin deformers on one mesh are not supported.";
			return false;
		}

		// 선형 행렬 블렌딩으로 처리할 수 있는 방식만 허용한다.
		const FbxSkin::EType SkinningType = SkinCluster.Skin->GetSkinningType();
		if (SkinningType != FbxSkin::eLinear
			&& SkinningType != FbxSkin::eRigid)
		{
			OutError = "Only linear and rigid skinning are supported.";
			return false;
		}

		// Additive는 별도의 변형 계산이 필요하므로 허용하지 않는다.
		const FbxCluster::ELinkMode LinkMode = SkinCluster.Cluster->GetLinkMode();
		if (LinkMode != FbxCluster::eNormalize
			&& LinkMode != FbxCluster::eTotalOne)
		{
			OutError = "Only Normalize and TotalOne link modes are supported.";
			return false;
		}

		// 같은 Mesh의 Cluster들은 동일한 링크 모드를 사용해야 한다.
		// 앞선 수집에서 모든 Cluster의 유효성을 확인했으므로 조회할 수 있다.
		FbxCluster* FirstCluster = SkinCluster.Skin->GetCluster(0);
		if (LinkMode != FirstCluster->GetLinkMode())
		{
			OutError = "FBX mesh contains inconsistent cluster link modes.";
			return false;
		}
	}

	FbxNode* SceneRoot = Scene->GetRootNode();
	for (const FFbxSkinCluster& SkinCluster : Context.SkinClusters)
	{
		FbxNode* CandidateRoot = nullptr;
		FbxNode* Node = SkinCluster.Cluster->GetLink();

		// 공통 부모인 Null 노드도 본 계층의 루트로 사용할 수 있다.
		while (Node && Node != SceneRoot)
		{
			const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
			if (Attribute &&
				(Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton ||
				 Attribute->GetAttributeType() == FbxNodeAttribute::eNull))
			{
				CandidateRoot = Node;
			}

			Node = Node->GetParent();
		}

		if (!CandidateRoot)
		{
			OutError = "Failed to find a skeleton root.";
			return false;
		}

		// 하나의 리소스에는 동일한 루트의 스켈레톤만 포함한다.
		if (Context.RootBoneNode && Context.RootBoneNode != CandidateRoot)
		{
			OutError = "Multiple skeleton roots are not supported.";
			return false;
		}

		Context.RootBoneNode = CandidateRoot;
	}

	FbxMatrix RootBindGlobal;
	bool bFoundRootBind = false;

	// 루트 본이 Cluster에 직접 연결되어 있다면 바인드 행렬을 읽는다.
	for (const FFbxSkinCluster& SkinCluster : Context.SkinClusters)
	{
		if (SkinCluster.Cluster->GetLink() == Context.RootBoneNode)
		{
			FbxAMatrix LinkBindGlobal;
			SkinCluster.Cluster->GetTransformLinkMatrix(LinkBindGlobal);

			RootBindGlobal = FbxMatrix(LinkBindGlobal);
			bFoundRootBind = true;
			break;
		}
	}

	// 루트는 정점에 직접 영향을 주지 않아 Cluster가 없을 수도 있다.
	// 이 경우 Scene의 Bind Pose에서 루트의 전역 행렬을 찾는다.
	for (int32 PoseIndex = 0; !bFoundRootBind && PoseIndex < Scene->GetPoseCount(); ++PoseIndex)
	{
		FbxPose* Pose = Scene->GetPose(PoseIndex);
		if (!Pose || !Pose->IsBindPose())
		{
			continue;
		}

		const int32 RootPoseIndex = Pose->Find(Context.RootBoneNode);
		if (RootPoseIndex >= 0)
		{
			RootBindGlobal = Pose->GetMatrix(RootPoseIndex);
			bFoundRootBind = true;
		}
	}

	if (!bFoundRootBind)
	{
		OutError = "Failed to find the root bone bind transform.";
		return false;
	}

	// 원점과 축을 되돌리려면 역행렬이 존재해야 한다.
	const double Determinant = RootBindGlobal.Determinant();
	if (!std::isfinite(Determinant) || Determinant == 0.0)
	{
		OutError = "Root bone bind transform is not invertible.";
		return false;
	}

	FbxVector4 RootTranslation;
	FbxVector4 RootShearing;
	FbxVector4 RootScaling;
	FbxQuaternion RootRotation;
	double RootSign = 1.0;

	// 원본 루트 바인드 행렬은 유지한 채 위치·회전을 추출한다.
	// 공통 좌표계에는 스케일과 전단을 포함하지 않는다.
	RootBindGlobal.GetElements(RootTranslation, RootRotation, RootShearing, RootScaling, RootSign);

	for (int32 Component = 0; Component < 3; ++Component)
	{
		if (!std::isfinite(RootTranslation[Component]))
		{
			OutError = "Root bone bind translation is invalid.";
			return false;
		}
	}

	const double RotationLength = RootRotation.Length();

	if (!std::isfinite(RotationLength) || RotationLength <= 1e-12)
	{
		OutError = "Root bone bind rotation is invalid.";
		return false;
	}

	RootRotation.Normalize();

	// 원점과 축 방향만 정의해 공통 좌표계 변환이 크기를 바꾸지 않도록 한다.
	// 원본 스케일은 이후 메시 정점과 본의 기준 포즈에 남는다.
	const FbxMatrix RootBasis(RootTranslation, RootRotation, FbxVector4(1, 1, 1));
	Context.SceneToSkeletal = RootBasis.Inverse();

	for (FbxNode* MeshNode : Context.MeshNodes)
	{
		FbxAMatrix MeshBindGlobal;
		bool bHasBindMatrix = false;

		// 해당 Mesh의 바인드 당시 전역 배치를 찾는다.
		for (const FFbxSkinCluster& SkinCluster : Context.SkinClusters)
		{
			if (SkinCluster.MeshNode != MeshNode)
			{
				continue;
			}

			FbxAMatrix ClusterMeshBind;
			SkinCluster.Cluster->GetTransformMatrix(ClusterMeshBind);

			// 하나의 Mesh에는 하나의 바인드 배치를 사용한다.
			if (bHasBindMatrix && MeshBindGlobal != ClusterMeshBind)
			{
				OutError = "FBX mesh contains inconsistent bind transforms.";
				return false;
			}

			MeshBindGlobal = ClusterMeshBind;
			bHasBindMatrix = true;
		}

		// Skin 연결이 없는 Mesh는 기본 배치를 사용한다.
		if (!bHasBindMatrix)
		{
			MeshBindGlobal = MeshNode->EvaluateGlobalTransform();
		}

		// Node의 Geometry에만 적용되는 추가 변환을 구성한다.
		FbxAMatrix Geometry;
		Geometry.SetTRS(
			MeshNode->GetGeometricTranslation(FbxNode::eSourcePivot),
			MeshNode->GetGeometricRotation(FbxNode::eSourcePivot),
			MeshNode->GetGeometricScaling(FbxNode::eSourcePivot));

		// SDK 곱셈 규칙에 따라 오른쪽 변환부터 적용된다.
		const FbxMatrix MeshToSkeletal =
			Context.SceneToSkeletal * FbxMatrix(MeshBindGlobal) * FbxMatrix(Geometry);

		Context.MeshToSkeletalMatrices.Add(MeshNode, MeshToSkeletal);
	}

	for (const FFbxSkinCluster& SkinCluster : Context.SkinClusters)
	{
		// 연결 본에서 시작해 아직 등록되지 않은 조상들을 모은다.
		TArray<FbxNode*> BoneChain;
		FbxNode* Node = SkinCluster.Cluster->GetLink();

		while (Node && Node != SceneRoot)
		{
			// 이미 등록된 본은 부모들도 먼저 등록된 상태다.
			if (Context.BoneIndexMap.Contains(Node))
			{
				break;
			}

			BoneChain.Add(Node);

			// 선택한 루트 본 위의 그룹 Node는 본 목록에 포함하지 않는다.
			if (Node == Context.RootBoneNode)
			{
				break;
			}

			// 부모로 올라가며 필요한 조상을 수집한다.
			Node = Node->GetParent();
		}

		// 자식부터 모았으므로 역순으로 등록하면 부모가 먼저 온다.
		for (int32 ChainIndex = BoneChain.Num() - 1; ChainIndex >= 0; --ChainIndex)
		{
			FbxNode* BoneNode = BoneChain[ChainIndex];
			const int32 BoneIndex = Context.BoneNodes.Num();

			Context.BoneNodes.Add(BoneNode);
			Context.BoneIndexMap.Add(BoneNode, BoneIndex);
		}
	}

	// 최종 결과를 조립할 임시 데이터.
	// 모든 추출이 성공한 뒤에만 OutData로 이동한다.
	FSkeletalMeshData ImportedData;

	for (FbxNode* BoneNode : Context.BoneNodes)
	{
		FMeshBoneInfo BoneInfo;
		BoneInfo.Name = FName(BoneNode->GetName());

		FbxNode* ParentNode = BoneNode->GetParent();

		// 기준 루트 본은 부모 인덱스의 기본값 -1을 유지한다.
		if (BoneNode != Context.RootBoneNode && ParentNode)
		{
			const int32* ParentIndex = Context.BoneIndexMap.Find(ParentNode);

			// 필요한 조상은 앞선 목록 구성에서 모두 등록되어 있어야 한다.
			if (!ParentIndex)
			{
				OutError = "Failed to resolve the parent bone index.";
				return false;
			}

			BoneInfo.ParentIndex = *ParentIndex;
		}

		// BoneNodes와 같은 순서로 저장해 본 인덱스를 유지한다.
		ImportedData.RefSkeleton.RawRefBoneInfo.Add(BoneInfo);
	}

	// Cluster에 연결된 본의 Scene 기준 바인드 전역 행렬을 모은다.
	TMap<FbxNode*, FbxMatrix> BoneBindSceneGlobals;

	for (const FFbxSkinCluster& SkinCluster : Context.SkinClusters)
	{
		FbxNode* BoneNode = SkinCluster.Cluster->GetLink();
		FbxAMatrix LinkBindGlobal;
		SkinCluster.Cluster->GetTransformLinkMatrix(LinkBindGlobal);
		const FbxMatrix BindGlobal(LinkBindGlobal);

		const FbxMatrix* Existing = BoneBindSceneGlobals.Find(BoneNode);
		if (Existing && *Existing != BindGlobal)
		{
			OutError = "FBX bone contains inconsistent cluster bind transforms.";
			return false;
		}

		BoneBindSceneGlobals.Add(BoneNode, BindGlobal);
	}

	// Bind Pose에서 Cluster에 직접 연결되지 않은 본의 행렬도 모은다.
	// 기존 행렬과 다르면 동일한 기준 포즈로 처리할 수 없다.
	for (int32 PoseIndex = 0; PoseIndex < Scene->GetPoseCount(); ++PoseIndex)
	{
		FbxPose* Pose = Scene->GetPose(PoseIndex);
		if (!Pose || !Pose->IsBindPose())
		{
			continue;
		}

		for (int32 EntryIndex = 0; EntryIndex < Pose->GetCount(); ++EntryIndex)
		{
			FbxNode* BoneNode = Pose->GetNode(EntryIndex);
			if (!Context.BoneIndexMap.Contains(BoneNode))
			{
				continue;
			}

			const FbxMatrix& BindGlobal = Pose->GetMatrix(EntryIndex);
			const FbxMatrix* Existing = BoneBindSceneGlobals.Find(BoneNode);
			if (Existing && *Existing != BindGlobal)
			{
				OutError = "FBX bind poses contain inconsistent bone transforms.";
				return false;
			}

			BoneBindSceneGlobals.Add(BoneNode, BindGlobal);
		}
	}

	for (int32 BoneIndex = 0; BoneIndex < Context.BoneNodes.Num(); ++BoneIndex)
	{
		FbxNode* BoneNode = Context.BoneNodes[BoneIndex];
		const FbxMatrix* SceneBind = BoneBindSceneGlobals.Find(BoneNode);

		if (!SceneBind)
		{
			OutError = "Failed to find a bone bind transform.";
			return false;
		}

		// Scene 기준의 본 배치를 루트 기준 공통 공간으로 바꾼다.
		const FbxMatrix BoneGlobal = Context.SceneToSkeletal * (*SceneBind);

		// 자식의 로컬 변환과 역바인드 계산에는 역행렬이 필요하다.
		const double BoneDeterminant = BoneGlobal.Determinant();
		if (!std::isfinite(BoneDeterminant) || BoneDeterminant == 0.0)
		{
			OutError = "Bone bind transform is not invertible.";
			return false;
		}

		const int32 ParentIndex = ImportedData.RefSkeleton.RawRefBoneInfo[BoneIndex].ParentIndex;

		// 루트는 공통 공간 기준의 행렬을 그대로 사용한다.
		FbxMatrix LocalBind = BoneGlobal;

		if (ParentIndex >= 0)
		{
			// 부모의 전역 배치를 되돌려 부모 기준 로컬 행렬을 얻는다.
			const FbxMatrix& ParentGlobal = Context.BoneBindGlobals[ParentIndex];
			LocalBind = ParentGlobal.Inverse() * BoneGlobal;
		}

		FTransform RefTransform;
		if (!ConvertFbxMatrixToTransform(LocalBind, RefTransform, OutError))
		{
			return false;
		}

		Context.BoneBindGlobals.Add(BoneGlobal);
		ImportedData.RefSkeleton.RawRefBonePose.Add(RefTransform);
	}

	for (int32 BoneIndex = 0; BoneIndex < Context.BoneBindGlobals.Num(); ++BoneIndex)
	{
		// 본 공간에서 공통 SkeletalMesh 공간으로 가는 바인드 행렬을 뒤집는다.
		// 결과는 공통 SkeletalMesh 공간에서 바인드 당시 본 공간으로 가는 행렬이다.
		const FbxMatrix InverseBind = Context.BoneBindGlobals[BoneIndex].Inverse();
		const FbxVector4 Origin = InverseBind.MultNormalize(FbxVector4(0, 0, 0, 1));

		FMatrix EngineInverseBind = FMatrix::Identity;

		for (int32 Row = 0; Row < 4; ++Row)
		{
			// 앞의 세 행은 단위축 끝점, 마지막 행은 원점을 사용한다.
			FbxVector4 Point(0, 0, 0, 1);
			if (Row < 3)
			{
				Point[Row] = 1.0;
			}

			const FbxVector4 Transformed = InverseBind.MultNormalize(Point);

			for (int32 Column = 0; Column < 3; ++Column)
			{
				// 축 끝점에서 원점을 빼 이동을 제거한다.
				// 마지막 행에는 이동 자체를 저장한다.
				const double Value = Row < 3 ? Transformed[Column] - Origin[Column] : Origin[Column];

				if (!IsWithinFloatRange(Value))
				{
					OutError = "Inverse bind matrix exceeds the engine numeric range.";
					return false;
				}

				EngineInverseBind[Row][Column] = static_cast<float>(Value);
			}
		}

		// 본 정보와 기준 포즈에 대응하는 본 인덱스 순서로 저장한다.
		ImportedData.RefBasesInvMatrix.Add(EngineInverseBind);
	}

	for (const FFbxSkinCluster& SkinCluster : Context.SkinClusters)
	{
		FbxCluster* Cluster = SkinCluster.Cluster;
		const int32* BoneIndex = Context.BoneIndexMap.Find(Cluster->GetLink());

		const int32 InfluenceCount = Cluster->GetControlPointIndicesCount();
		const int* Indices = Cluster->GetControlPointIndices();
		const double* Weights = Cluster->GetControlPointWeights();

		// 영향이 없는 Cluster는 배열 포인터가 nullptr일 수 있다.
		if (!BoneIndex || InfluenceCount < 0
			|| (InfluenceCount > 0 && (!Indices || !Weights)))
		{
			OutError = "Invalid FBX cluster influence data.";
			return false;
		}

		auto& MeshInfluences = Context.ControlPointInfluences.FindOrAdd(SkinCluster.MeshNode);

		// 같은 Mesh의 Cluster들은 같은 크기를 사용한다.
		// SetNum()은 크기가 같으면 기존에 수집한 내용을 유지한다.
		MeshInfluences.SetNum(SkinCluster.MeshNode->GetMesh()->GetControlPointsCount());

		for (int32 EntryIndex = 0; EntryIndex < InfluenceCount; ++EntryIndex)
		{
			const int32 ControlPointIndex = Indices[EntryIndex];
			const double Weight = Weights[EntryIndex];

			if (!MeshInfluences.IsValidIndex(ControlPointIndex)
				|| !std::isfinite(Weight) || Weight < 0.0)
			{
				OutError = "Invalid FBX control point influence.";
				return false;
			}

			// 영향력이 없는 항목은 저장하지 않는다.
			if (Weight > 0.0)
			{
				MeshInfluences[ControlPointIndex].Add({ *BoneIndex, Weight });
			}
		}
	}

	// TotalOne의 원본 Weight 합계에 허용할 오차.
	constexpr double WeightSumTolerance = 1e-4;

	for (auto& MeshEntry : Context.ControlPointInfluences)
	{
		FbxNode* MeshNode = MeshEntry.first;
		auto& MeshInfluences = MeshEntry.second;

		// 수집 대상은 Skin 하나와 유효한 Cluster가 있는 Mesh다.
		// 같은 Mesh의 Cluster들은 동일한 링크 모드로 검증되어 있다.
		FbxSkin* Skin = static_cast<FbxSkin*>(
			MeshNode->GetMesh()->GetDeformer(0, FbxDeformer::eSkin));

		const bool bRigid = Skin->GetSkinningType() == FbxSkin::eRigid;
		const bool bTotalOne = Skin->GetCluster(0)->GetLinkMode() == FbxCluster::eTotalOne;

		for (auto& Influences : MeshInfluences)
		{
			// 양의 영향이 없는 점은 기존 규약대로 비스키닝 상태로 유지한다.
			if (Influences.IsEmpty())
			{
				continue;
			}

			TMap<int32, double> MergedWeights;
			double TotalWeight = 0.0;

			for (const FFbxBoneInfluence& Influence : Influences)
			{
				// 같은 본 인덱스에 해당하는 Weight를 모두 더한다.
				MergedWeights.FindOrAdd(Influence.BoneIndex) += Influence.Weight;
				TotalWeight += Influence.Weight;
			}

			// 개별 Weight가 유효해도 합산 과정에서 범위를 넘을 수 있다.
			if (!std::isfinite(TotalWeight) || TotalWeight <= 0.0)
			{
				OutError = "Invalid control point weight sum.";
				return false;
			}

			// Rigid는 하나의 Control Point에 하나의 본만 영향을 줘야 한다.
			if (bRigid && MergedWeights.Num() != 1)
			{
				OutError = "Rigid skinning requires one bone per control point.";
				return false;
			}

			// 영향 개수 제한이나 정규화 전에 원본 합계를 검사한다.
			if (bTotalOne && std::abs(TotalWeight - 1.0) > WeightSumTolerance)
			{
				OutError = "TotalOne control point weights must sum to one.";
				return false;
			}

			// 원본 목록을 중복 본이 합산된 목록으로 교체한다.
			Influences.Reset();

			for (const auto& BoneEntry : MergedWeights)
			{
				const int32 BoneIndex = BoneEntry.first;
				const double Weight = BoneEntry.second;
				Influences.Add({ BoneIndex, Weight });
			}
		}
	}

	for (auto& MeshEntry : Context.ControlPointInfluences)
	{
		FbxNode* MeshNode = MeshEntry.first;
		auto& MeshInfluences = MeshEntry.second;
		auto& MeshSkinWeights = Context.ControlPointSkinWeights.FindOrAdd(MeshNode);
		MeshSkinWeights.SetNum(MeshInfluences.Num());

		for (int32 ControlPointIndex = 0; ControlPointIndex < MeshInfluences.Num(); ++ControlPointIndex)
		{
			auto& Influences = MeshInfluences[ControlPointIndex];
			FSkinWeightInfo& SkinWeight = MeshSkinWeights[ControlPointIndex];

			// 미사용 슬롯과 영향이 없는 점은 모두 0으로 유지한다.
			SkinWeight = {};
			if (Influences.IsEmpty())
			{
				continue;
			}

			// 영향력이 같으면 본 인덱스가 작은 항목을 먼저 선택한다.
			// Map 순회 순서와 관계없이 결과를 일정하게 만든다.
			std::sort(Influences.begin(), Influences.end(),
				[](const FFbxBoneInfluence& Left, const FFbxBoneInfluence& Right)
				{
					if (Left.Weight != Right.Weight)
					{
						return Left.Weight > Right.Weight;
					}
					return Left.BoneIndex < Right.BoneIndex;
				});

			const int32 SelectedCount = std::min(Influences.Num(), static_cast<int32>(MaxBoneInfluences));
			double SelectedWeightSum = 0.0;

			for (int32 InfluenceIndex = 0; InfluenceIndex < SelectedCount; ++InfluenceIndex)
			{
				SelectedWeightSum += Influences[InfluenceIndex].Weight;
			}

			if (!std::isfinite(SelectedWeightSum) || SelectedWeightSum <= 0.0)
			{
				OutError = "Invalid selected control point weight sum.";
				return false;
			}

			for (int32 InfluenceIndex = 0; InfluenceIndex < SelectedCount; ++InfluenceIndex)
			{
				const FFbxBoneInfluence& Influence = Influences[InfluenceIndex];
				const double NormalizedWeight = Influence.Weight / SelectedWeightSum;

				SkinWeight.InfluenceBones[InfluenceIndex] = Influence.BoneIndex;
				SkinWeight.InfluenceWeights[InfluenceIndex] = static_cast<float>(NormalizedWeight);
			}
		}
	}

	// SDK 재질 객체가 같으면 기존 슬롯을 재사용한다.
	TMap<FbxSurfaceMaterial*, uint16> MaterialIndexMap;

	// 재질이 없는 Mesh와 빈 재질 항목에 사용할 기본 슬롯.
	FStaticMaterialSlot DefaultSlot;
	DefaultSlot.Name = "DefaultMaterial";
	const uint16 DefaultMaterialIndex = static_cast<uint16>(ImportedData.MaterialSlots.Add(DefaultSlot));

	for (FbxNode* MeshNode : Context.MeshNodes)
	{
		const int32 MaterialCount = MeshNode->GetMaterialCount();
		auto& MaterialIndices = Context.MeshMaterialIndices.FindOrAdd(MeshNode);

		// 모든 항목을 기본 슬롯 인덱스로 채운다.
		MaterialIndices.Init(DefaultMaterialIndex, std::max(MaterialCount, 1));

		for (int32 LocalMaterialIndex = 0; LocalMaterialIndex < MaterialCount; ++LocalMaterialIndex)
		{
			FbxSurfaceMaterial* Material = MeshNode->GetMaterial(LocalMaterialIndex);
			if (!Material)
			{
				continue;
			}

			const uint16* ExistingIndex = MaterialIndexMap.Find(Material);
			if (ExistingIndex)
			{
				MaterialIndices[LocalMaterialIndex] = *ExistingIndex;
				continue;
			}

			// Section.MaterialIndex의 uint16 범위를 넘지 않도록 확인한다.
			if (ImportedData.MaterialSlots.Num() > std::numeric_limits<uint16>::max())
			{
				OutError = "Skeletal mesh contains too many material slots.";
				return false;
			}

			FStaticMaterialSlot Slot;
			Slot.Name = Material->GetName();

			const FbxProperty Diffuse = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
			const FbxProperty Normal = Material->FindProperty(FbxSurfaceMaterial::sNormalMap);

			if (!ReadMaterialBaseColor(Material, Slot.BaseColor, OutError)
				|| !ReadMaterialTexturePath(Diffuse, Path, Slot.DiffuseTexturePath, OutError)
				|| !ReadMaterialTexturePath(Normal, Path, Slot.NormalTexturePath, OutError))
			{
				return false;
			}

			const uint16 SlotIndex = static_cast<uint16>(ImportedData.MaterialSlots.Add(Slot));

			MaterialIndexMap.Add(Material, SlotIndex);
			MaterialIndices[LocalMaterialIndex] = SlotIndex;
		}
	}

	for (FbxNode* MeshNode : Context.MeshNodes)
	{
		FbxMesh* Mesh = MeshNode->GetMesh();
		const FbxMatrix* MeshToSkeletal = Context.MeshToSkeletalMatrices.Find(MeshNode);
		auto& PolygonVertexIndices = Context.MeshPolygonVertexIndices.FindOrAdd(MeshNode);

		if (!Mesh || !MeshToSkeletal)
		{
			OutError = "Failed to resolve mesh import data.";
			return false;
		}

		const int32 PolygonCount = Mesh->GetPolygonCount();
		if (PolygonCount == 0)
		{
			continue;
		}

		const int32 ControlPointCount = Mesh->GetControlPointsCount();
		const FbxVector4* ControlPoints = Mesh->GetControlPoints();

		if (ControlPointCount <= 0 || !ControlPoints)
		{
			OutError = "FBX mesh has no valid control points.";
			return false;
		}

		// 앞선 삼각형화에 의해 Polygon마다 꼭짓점이 세 개다.
		// TArray::Num()이 int32이므로 최종 정점 개수도 범위를 확인한다.
		const int64 AddedVertexCount = static_cast<int64>(PolygonCount) * 3;
		const int64 TotalVertexCount = ImportedData.ReferenceVertices.Num() + AddedVertexCount;

		if (TotalVertexCount > std::numeric_limits<int32>::max())
		{
			OutError = "Skeletal mesh contains too many vertices.";
			return false;
		}

		FMatrix NormalMatrix;
		if (!BuildNormalMatrix(*MeshToSkeletal, NormalMatrix, OutError))
		{
			return false;
		}

		// 원본 노멀이 없으면 SDK에서 Polygon Vertex 노멀을 생성한다.
		if (Mesh->GetElementNormalCount() == 0 && !Mesh->GenerateNormals(false, false, false))
		{
			OutError = "Failed to generate mesh normals.";
			return false;
		}

		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		const char* UVSetName = UVSetNames.GetCount() > 0 ? UVSetNames.GetStringAt(0) : nullptr;

		// 공유 재질이라도 Mesh마다 첫 UV 세트가 다를 수 있으므로 개별 검사한다.
		for (int32 LocalMaterialIndex = 0; LocalMaterialIndex < MeshNode->GetMaterialCount(); ++LocalMaterialIndex)
		{
			FbxSurfaceMaterial* Material = MeshNode->GetMaterial(LocalMaterialIndex);

			// 빈 재질 항목은 앞서 구성한 기본 슬롯을 사용한다.
			if (!Material)
			{
				continue;
			}

			const FbxProperty Diffuse = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
			const FbxProperty Normal = Material->FindProperty(FbxSurfaceMaterial::sNormalMap);

			if (!ValidateMaterialTextureUVSet(Diffuse, *Mesh, UVSetName, OutError)
				|| !ValidateMaterialTextureUVSet(Normal, *Mesh, UVSetName, OutError))
			{
				return false;
			}
		}

		const TArray<FSkinWeightInfo>* MeshSkinWeights = Context.ControlPointSkinWeights.Find(MeshNode);

		if (MeshSkinWeights && MeshSkinWeights->Num() != ControlPointCount)
		{
			OutError = "Control point skin weight count does not match the mesh.";
			return false;
		}

		PolygonVertexIndices.Reserve(static_cast<uint32>(AddedVertexCount));

		for (int32 PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const int32 ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);

				if (ControlPointIndex < 0 || ControlPointIndex >= ControlPointCount)
				{
					OutError = "Polygon contains an invalid control point index.";
					return false;
				}

				// 위치는 Control Point에서 읽고 공통 SkeletalMesh 공간으로 변환한다.
				const FbxVector4& ControlPoint = ControlPoints[ControlPointIndex];
				const FbxVector4 Position = MeshToSkeletal->MultNormalize(
					FbxVector4(ControlPoint[0], ControlPoint[1], ControlPoint[2], 1));

				// 노멀은 현재 Polygon Vertex 기준으로 읽는다.
				FbxVector4 SourceNormal;
				if (!Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, SourceNormal))
				{
					OutError = "Failed to read a polygon vertex normal.";
					return false;
				}

				FVertex Vertex;
				FVector LocalNormal;

				for (int32 Component = 0; Component < 3; ++Component)
				{
					if (!IsWithinFloatRange(Position[Component])
						|| !IsWithinFloatRange(SourceNormal[Component]))
					{
						OutError = "Mesh vertex data exceeds the engine numeric range.";
						return false;
					}

					Vertex.Position[Component] = static_cast<float>(Position[Component]);
					LocalNormal[Component] = static_cast<float>(SourceNormal[Component]);
				}

				// 노멀은 이동을 제외한 역전치 변환을 적용한다.
				const FVector TransformedNormal = NormalMatrix.TransformVector(LocalNormal);
				const double NormalLength = std::hypot(
					static_cast<double>(TransformedNormal.X),
					static_cast<double>(TransformedNormal.Y),
					static_cast<double>(TransformedNormal.Z));

				if (!std::isfinite(NormalLength) || NormalLength <= 1e-8)
				{
					OutError = "Mesh vertex has an invalid transformed normal.";
					return false;
				}

				for (int32 Component = 0; Component < 3; ++Component)
				{
					Vertex.Normal[Component] = static_cast<float>(TransformedNormal[Component] / NormalLength);
				}

				if (UVSetName)
				{
					FbxVector2 SourceUV;
					bool bUnmapped = false;

					if (!Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetName, SourceUV, bUnmapped) || bUnmapped)
					{
						OutError = "Failed to read a polygon vertex UV.";
						return false;
					}

					// 기존 OBJ 임포터와 동일하게 V 방향을 뒤집는다.
					const double U = SourceUV[0];
					const double V = 1.0 - SourceUV[1];

					if (!IsWithinFloatRange(U) || !IsWithinFloatRange(V))
					{
						OutError = "Mesh UV exceeds the engine numeric range.";
						return false;
					}

					Vertex.UV = FVector2(static_cast<float>(U), static_cast<float>(V));
				}

				// UV가 없으면 기본값 (0, 0), 정점 색상은 기본값 흰색을 사용한다.
				// 탄젠트는 최종 삼각형 인덱스 구성 후 계산한다.
				FSkinWeightInfo SkinWeight;

				if (MeshSkinWeights)
				{
					SkinWeight = (*MeshSkinWeights)[ControlPointIndex];
				}

				// Polygon Vertex마다 별도 생성해 UV와 노멀 경계를 보존한다.
				// 위치와 Weight는 같은 Control Point에서 읽는다.
				const uint32 VertexIndex = ImportedData.ReferenceVertices.Add(Vertex);
				ImportedData.SkinWeights.Add(SkinWeight);
				PolygonVertexIndices.Add(VertexIndex);
			}
		}
	}

	// 전체 재질 슬롯마다 삼각형 인덱스를 모을 배열을 준비한다.
	TArray<TArray<uint32>> IndicesByMaterial;
	IndicesByMaterial.SetNum(ImportedData.MaterialSlots.Num());

	for (FbxNode* MeshNode : Context.MeshNodes)
	{
		FbxMesh* Mesh = MeshNode->GetMesh();
		const TArray<uint32>* PolygonVertexIndices = Context.MeshPolygonVertexIndices.Find(MeshNode);
		const TArray<uint16>* MaterialIndices = Context.MeshMaterialIndices.Find(MeshNode);
		const FbxMatrix* MeshToSkeletal = Context.MeshToSkeletalMatrices.Find(MeshNode);

		if (!Mesh || !PolygonVertexIndices || !MaterialIndices || !MeshToSkeletal)
		{
			OutError = "Failed to resolve mesh index construction data.";
			return false;
		}

		const int32 PolygonCount = Mesh->GetPolygonCount();
		const int64 ExpectedVertexCount = static_cast<int64>(PolygonCount) * 3;

		if (PolygonVertexIndices->Num() != ExpectedVertexCount)
		{
			OutError = "Polygon vertex index count does not match the mesh.";
			return false;
		}

		// SDK 변환 후의 Polygon 순서를 기준으로 삼는다.
		// 공통 공간으로 옮기는 추가 변환이 반사이면 순서를 뒤집는다.
		const bool bReverseWinding = MeshToSkeletal->Determinant() < 0.0;

		for (int32 PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
		{
			int32 LocalMaterialIndex = 0;

			if (!ReadPolygonMaterialIndex(*Mesh, PolygonIndex, LocalMaterialIndex, OutError))
			{
				return false;
			}

			// 재질 슬롯 구성에서 만든 기본 슬롯을 미할당 재질에 사용한다.
			uint16 MaterialSlotIndex = DefaultMaterialIndex;

			if (LocalMaterialIndex >= 0)
			{
				if (!MaterialIndices->IsValidIndex(LocalMaterialIndex))
				{
					OutError = "Polygon material index is outside the mesh material table.";
					return false;
				}

				MaterialSlotIndex = (*MaterialIndices)[LocalMaterialIndex];
			}

			if (!IndicesByMaterial.IsValidIndex(MaterialSlotIndex))
			{
				OutError = "Polygon material slot index is out of range.";
				return false;
			}

			uint32 TriangleIndices[3];

			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const int32 PolygonVertexIndex = PolygonIndex * 3 + CornerIndex;
				TriangleIndices[CornerIndex] = (*PolygonVertexIndices)[PolygonVertexIndex];

				if (TriangleIndices[CornerIndex] >= static_cast<uint32>(ImportedData.ReferenceVertices.Num()))
				{
					OutError = "Triangle references an invalid skeletal mesh vertex.";
					return false;
				}
			}

			auto& MaterialTriangleIndices = IndicesByMaterial[MaterialSlotIndex];

			MaterialTriangleIndices.Add(TriangleIndices[0]);
			MaterialTriangleIndices.Add(TriangleIndices[bReverseWinding ? 2 : 1]);
			MaterialTriangleIndices.Add(TriangleIndices[bReverseWinding ? 1 : 2]);
		}
	}

	// 정점이 Polygon Vertex별로 분리되어 있으므로 최종 인덱스 수도 같다.
	ImportedData.Indices.Reserve(static_cast<uint32>(ImportedData.ReferenceVertices.Num()));

	for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < IndicesByMaterial.Num(); ++MaterialSlotIndex)
	{
		const TArray<uint32>& MaterialTriangleIndices = IndicesByMaterial[MaterialSlotIndex];

		// 실제 삼각형이 없는 재질은 Section을 만들지 않는다.
		if (MaterialTriangleIndices.IsEmpty())
		{
			continue;
		}

		FSkelMeshRenderSection Section;
		Section.BaseIndex = static_cast<uint32>(ImportedData.Indices.Num());
		Section.NumTriangles = static_cast<uint32>(MaterialTriangleIndices.Num() / 3);
		Section.MaterialIndex = static_cast<uint16>(MaterialSlotIndex);

		// 같은 재질의 인덱스를 하나의 연속 구간으로 저장한다.
		ImportedData.Indices.Append(MaterialTriangleIndices);
		ImportedData.Sections.Add(Section);
	}

	if (!BuildTangentSpace(ImportedData, OutError))
	{
		return false;
	}

	// 탄젠트 생성이 성공했으므로 정점 배열은 비어 있지 않다.
	// 첫 정점으로 시작해야 원점이 불필요하게 Bounds에 포함되지 않는다.
	FBox ReferenceBounds;
	ReferenceBounds.Min = ImportedData.ReferenceVertices[0].Position;
	ReferenceBounds.Max = ReferenceBounds.Min;

	for (const FVertex& Vertex : ImportedData.ReferenceVertices)
	{
		for (int32 Component = 0; Component < 3; ++Component)
		{
			ReferenceBounds.Min[Component] = std::min(ReferenceBounds.Min[Component], Vertex.Position[Component]);
			ReferenceBounds.Max[Component] = std::max(ReferenceBounds.Max[Component], Vertex.Position[Component]);
		}
	}

	ImportedData.ReferenceBounds = ReferenceBounds;

	if (!ValidateSkeletalMeshData(ImportedData, OutError))
	{
		return false;
	}

	// 모든 추출과 검사가 성공했을 때만 호출자의 결과를 교체한다.
	// 이동 대입으로 정점·인덱스 등의 큰 배열을 복사하지 않고 전달한다.
	OutData = std::move(ImportedData);

	return true;
}

void FFbxImporter::LoadSceneRecursive(FbxNode* Node, FImportContext& Context)
{
	if (!Node)
	{
		return;
	}

	// 빈 메시 노드는 렌더링 대상이 아니다. Transform과 재질을 읽기 위해 Node를 보관한다.
	if (FbxMesh* Mesh = Node->GetMesh(); Mesh && Mesh->GetPolygonCount() > 0)
	{
		Context.MeshNodes.Add(Node);
	}

	// 현재 Node에 Mesh가 없어도 자식 탐색은 계속한다.
	for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
	{
		LoadSceneRecursive(Node->GetChild(ChildIndex), Context);
	}
}

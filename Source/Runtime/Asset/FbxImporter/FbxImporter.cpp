#include "EnginePCH.h"
#include "FbxImporter.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Math/Quat.h"
#include "Render/SkeletalMeshData.h"

#include <cmath>
#include <fbxsdk.h>
#include <limits>

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

	// Scene 전역 공간을 루트 본의 바인드 로컬 공간으로 변환한다.
	FbxMatrix SceneToSkeletal;

	// 각 Mesh의 Control Point를 루트 기준 공간으로 변환하는 행렬.
	TMap<FbxNode*, FbxMatrix> MeshToSkeletalMatrices;
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

	// 교체가 끝난 Scene에서 Mesh를 다시 조회한다.
	const int32 MeshCount = Scene->GetSrcObjectCount<FbxMesh>();
	if (MeshCount == 0)
	{
		OutError = "FBX scene contains no meshes.";
		return false;
	}

	// 각 Mesh의 Polygon이 모두 삼각형인지 확인한다.
	for (int32 MeshIndex = 0; MeshIndex < MeshCount; ++MeshIndex)
	{
		FbxMesh* Mesh = Scene->GetSrcObject<FbxMesh>(MeshIndex);

		if (!Mesh || !Mesh->IsTriangleMesh())
		{
			OutError = "FBX mesh triangulation verification failed.";
			return false;
		}
	}

	// 하나의 Context를 재귀 호출 전체에서 공유한다.
	FImportContext Context;
	LoadSceneRecursive(Scene->GetRootNode(), Context);

	if (Context.MeshNodes.IsEmpty())
	{
		OutError = "FBX scene contains no mesh nodes to import.";
		return false;
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

		// 연결 본의 조상 중 가장 위에 있는 Skeleton Node를 찾는다.
		while (Node && Node != SceneRoot)
		{
			if (Node->GetSkeleton())
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
	for (int32 PoseIndex = 0;
		 !bFoundRootBind && PoseIndex < Scene->GetPoseCount();
		 ++PoseIndex)
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

	Context.SceneToSkeletal = RootBindGlobal.Inverse();

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

	// 이후 여기에 역바인드 행렬과 메시 등의 추출을 추가한다.
	// 완성된 임시 데이터를 OutData에 이동한 뒤 true를 반환한다.
	OutError = "Skeletal mesh data extraction is not implemented yet.";

	return false;
}

void FFbxImporter::LoadSceneRecursive(FbxNode* Node, FImportContext& Context)
{
	if (!Node)
	{
		return;
	}

	// 이후 Transform과 재질 정보도 사용하므로 Mesh 대신 Node를 저장한다.
	if (Node->GetMesh())
	{
		Context.MeshNodes.Add(Node);
	}

	// 현재 Node에 Mesh가 없어도 자식 탐색은 계속한다.
	for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
	{
		LoadSceneRecursive(Node->GetChild(ChildIndex), Context);
	}
}

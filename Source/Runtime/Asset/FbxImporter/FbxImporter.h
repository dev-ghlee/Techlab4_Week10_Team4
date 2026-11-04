#pragma once

#include "Core/EngineString.h"
#include "Core/Types.h"
#include "Containers/Array.h"

struct FSkeletalMeshData;
struct FStaticMeshData;
struct FTransform;
struct FVector4;
struct FMatrix;

// SDK 전체를 헤더에 Include하지 않도록 전방 선언한다.
namespace fbxsdk
{
	class FbxManager;
	class FbxScene;
	class FbxNode;
	class FbxMatrix;
	class FbxMesh;
	class FbxSurfaceMaterial;
	class FbxProperty;
}

enum class EFbxMeshType
{
	StaticMesh,
	SkeletalMesh
};

class FFbxImporter
{
public:
	// 성공 시 완성된 엔진용 데이터를 OutData에 저장하고 true를 반환한다.
	// 실패 시 OutError에 원인을 기록하며 기존 OutData는 유지한다.
	static bool LoadScene(const FString& Path, FSkeletalMeshData& OutData, FString& OutError);

	// FBX의 Mesh Node들을 하나의 StaticMesh 데이터로 구성한다.
	static bool LoadStaticMesh(const FString& Path, FStaticMeshData& OutData, FString& OutError);

	// 메시의 Skin 연결 여부를 기준으로 임포트할 타입을 판별한다.
	static bool DetectMeshType(const FString& Path, EFbxMeshType& OutType, FString& OutError);

private:
	// FBX 파일을 읽고 Manager가 소유하는 Scene을 생성한다.
	static bool LoadFbxScene(fbxsdk::FbxManager& Manager, const FString& Path, fbxsdk::FbxScene*& OutScene, FString& OutError);

	// 엔진 좌표계와 cm 단위로 변환하고 Geometry를 삼각형화한다.
	static bool ConvertAndTriangulateScene(fbxsdk::FbxManager& Manager, fbxsdk::FbxScene& Scene, FString& OutError);

	// NaN과 무한대를 제외하고 float의 값 범위에 들어가는지 확인한다.
	static bool IsWithinFloatRange(double Value);

	// SDK 행렬을 엔진 Transform으로 변환하고 원본 변환의 보존 여부를 확인한다.
	static bool ConvertFbxMatrixToTransform(const fbxsdk::FbxMatrix& InMatrix, FTransform& OutTransform, FString& OutError);

	// 비균일 스케일을 포함한 메시 변환에 사용할 노멀 행렬을 만든다.
	static bool BuildNormalMatrix(const fbxsdk::FbxMatrix& MeshToSkeletal, FMatrix& OutNormalMatrix, FString& OutError);

	// 표준 Lambert와 Phong 재질의 Diffuse를 엔진 BaseColor로 읽는다.
	static bool ReadMaterialBaseColor(fbxsdk::FbxSurfaceMaterial* Material, FVector4& OutBaseColor, FString& OutError);

	// 연결된 파일 텍스처를 찾아 FBX 파일 기준으로 경로를 해석한다.
	// 파일 누락을 허용하면 경고 후 빈 경로를 반환해 기본 텍스처를 사용한다.
	static bool ReadMaterialTexturePath(const fbxsdk::FbxProperty& Property, const FString& FbxPath, FString& OutTexturePath, FString& OutError, bool bAllowMissingTexture = false);

	// 텍스처가 해당 Mesh에서 임포트할 첫 UV 세트를 사용하는지 검사한다.
	static bool ValidateMaterialTextureUVSet(
		const fbxsdk::FbxProperty& Property, const fbxsdk::FbxMesh& Mesh,
		const char* UVSetName, FString& OutError);

	// Polygon에 연결된 Mesh Node 로컬 재질 인덱스를 읽는다.
	static bool ReadPolygonMaterialIndex(const fbxsdk::FbxMesh& Mesh, int32 PolygonIndex, int32& OutMaterialIndex, FString& OutError);

	// 최종 정점과 삼각형 인덱스로 탄젠트와 Bitangent 방향 부호를 계산한다.
	static bool BuildTangentSpace(FSkeletalMeshData& MeshData, FString& OutError);

	// 조립된 임포트 결과의 배열 대응과 참조 관계를 검사한다.
	static bool ValidateSkeletalMeshData(const FSkeletalMeshData& MeshData, FString& OutError);

	// 노드 계층을 재귀적으로 순회하며 비어 있지 않은 Mesh Node를 수집한다.
	static void CollectMeshNodesRecursive(fbxsdk::FbxNode* Node, TArray<fbxsdk::FbxNode*>& OutMeshNodes);
};

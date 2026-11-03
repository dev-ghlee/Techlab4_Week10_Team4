#pragma once

#include "Core/EngineString.h"
#include "Core/Types.h"

struct FSkeletalMeshData;
struct FTransform;
struct FVector4;
struct FMatrix;

// Node 목록과 본 인덱스 대응표 등의 임시 정보. 정의는 cpp에 둔다.
struct FImportContext;

// SDK 전체를 헤더에 Include하지 않도록 전방 선언한다.
namespace fbxsdk
{
	class FbxNode;
	class FbxMatrix;
	class FbxMesh;
	class FbxSurfaceMaterial;
	class FbxProperty;
}

class FFbxImporter
{
public:
	// 성공 시 완성된 엔진용 데이터를 OutData에 저장하고 true를 반환한다.
	// 실패 시 OutError에 원인을 기록하며 기존 OutData는 유지한다.
	static bool LoadScene(const FString& Path, FSkeletalMeshData& OutData, FString& OutError);

private:
	// NaN과 무한대를 제외하고 float의 값 범위에 들어가는지 확인한다.
	static bool IsWithinFloatRange(double Value);

	// SDK 행렬을 엔진 Transform으로 변환하고 원본 변환의 보존 여부를 확인한다.
	static bool ConvertFbxMatrixToTransform(const fbxsdk::FbxMatrix& InMatrix, FTransform& OutTransform, FString& OutError);

	// 비균일 스케일을 포함한 메시 변환에 사용할 노멀 행렬을 만든다.
	static bool BuildNormalMatrix(const fbxsdk::FbxMatrix& MeshToSkeletal, FMatrix& OutNormalMatrix, FString& OutError);

	// 표준 Lambert와 Phong 재질의 Diffuse를 엔진 BaseColor로 읽는다.
	static bool ReadMaterialBaseColor(fbxsdk::FbxSurfaceMaterial* Material, FVector4& OutBaseColor, FString& OutError);

	// 연결된 파일 텍스처를 찾아 FBX 파일 기준으로 경로를 해석한다.
	static bool ReadMaterialTexturePath(const fbxsdk::FbxProperty& Property, const FString& FbxPath, FString& OutTexturePath, FString& OutError);

	// Polygon에 연결된 Mesh Node 로컬 재질 인덱스를 읽는다.
	static bool ReadPolygonMaterialIndex(const fbxsdk::FbxMesh& Mesh, int32 PolygonIndex, int32& OutMaterialIndex, FString& OutError);

	// 최종 정점과 삼각형 인덱스로 탄젠트와 Bitangent 방향 부호를 계산한다.
	static bool BuildTangentSpace(FSkeletalMeshData& MeshData, FString& OutError);

	// 조립된 임포트 결과의 배열 대응과 참조 관계를 검사한다.
	static bool ValidateSkeletalMeshData(const FSkeletalMeshData& MeshData, FString& OutError);

	// 현재 Node에서 필요한 대상을 수집하고 자식마다 재귀 호출한다.
	static void LoadSceneRecursive(fbxsdk::FbxNode* Node, FImportContext& Context);
};

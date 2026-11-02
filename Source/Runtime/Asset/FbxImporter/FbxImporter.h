#pragma once

#include "Core/EngineString.h"

struct FSkeletalMeshData;
struct FTransform;

// Node 목록과 본 인덱스 대응표 등의 임시 정보. 정의는 cpp에 둔다.
struct FImportContext;

// SDK 전체를 헤더에 Include하지 않도록 전방 선언한다.
namespace fbxsdk
{
	class FbxNode;
	class FbxMatrix;
}

class FFbxImporter
{
public:
	// 성공 시 완성된 엔진용 데이터를 OutData에 저장하고 true를 반환한다.
	// 실패 시 OutError에 원인을 기록하며 기존 OutData는 유지한다.
	// 데이터 추출 단계까지 구현되기 전에는 미구현 오류를 반환한다.
	static bool LoadScene(const FString& Path, FSkeletalMeshData& OutData, FString& OutError);

private:
	// NaN과 무한대를 제외하고 float의 값 범위에 들어가는지 확인한다.
	static bool IsWithinFloatRange(double Value);

	// SDK 행렬을 엔진 Transform으로 변환하고 원본 변환의 보존 여부를 확인한다.
	static bool ConvertFbxMatrixToTransform(const fbxsdk::FbxMatrix& InMatrix, FTransform& OutTransform, FString& OutError);

	// 현재 Node에서 필요한 대상을 수집하고 자식마다 재귀 호출한다.
	static void LoadSceneRecursive(fbxsdk::FbxNode* Node, FImportContext& Context);
};

#pragma once

#include "Core/EngineString.h"

struct FSkeletalMeshData;

// SDK 전체를 헤더에 Include하지 않도록 전방 선언한다.
namespace fbxsdk
{
	class FbxNode;
}

class FFbxImporter
{
public:
	// 성공 시 완성된 엔진용 데이터를 OutData에 저장하고 true를 반환한다.
	// 실패 시 OutError에 원인을 기록하며 기존 OutData는 유지한다.
	// 함수 본문은 FBX 로딩 및 추출 단계에서 구현한다.
	static bool LoadScene(const FString& Path, FSkeletalMeshData& OutData, FString& OutError);

private:
	// Node 목록과 본 인덱스 대응표 등의 임시 정보. 정의는 cpp에 둔다.
	struct FImportContext;

	// 현재 Node에서 필요한 대상을 수집하고 자식마다 재귀 호출한다.
	static void LoadSceneRecursive(fbxsdk::FbxNode* Node, FImportContext& Context);
};

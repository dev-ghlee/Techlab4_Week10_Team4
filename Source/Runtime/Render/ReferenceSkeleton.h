#pragma once

#include "Containers/Array.h"
#include "Core/NameTypes.h"
#include "Core/Types.h"
#include "Math/Transform.h"

// 본 하나의 이름과 부모 관계. 기준 포즈는 별도 배열에 보관한다.
struct FMeshBoneInfo
{
	// Viewer 표시와 이름 검색에 사용한다.
	// 임포트 중 본 연결은 이름이 아닌 SDK Node 대응표로 처리한다.
	FName Name;

	// RawRefBoneInfo 배열의 부모 본 인덱스.
	// 루트는 -1이며, 부모는 자식보다 먼저 저장한다.
	int32 ParentIndex = -1;
};

// 원본 에셋의 본 계층과 기준 포즈.
// 현재 구현에서는 바인드 포즈를 기준 포즈로 사용한다.
struct FReferenceSkeleton
{
	// 부모부터 자식 순서로 저장한 본 정보.
	TArray<FMeshBoneInfo> RawRefBoneInfo;

	// 같은 인덱스의 본에 대응하는 부모 기준 로컬 Transform.
	// 루트는 공통 SkeletalMesh 로컬 공간을 기준으로 한다.
	// 루트의 바인드 스케일을 포함하므로 항등 Transform을 보장하지 않는다.
	// 현재 포즈를 초기화할 때는 루트를 포함해 저장된 기준 포즈를 복사한다.
	// RawRefBoneInfo와 개수 및 순서가 반드시 같아야 한다.
	TArray<FTransform> RawRefBonePose;
};

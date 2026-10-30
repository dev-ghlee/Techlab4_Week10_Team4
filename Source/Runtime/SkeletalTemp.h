#pragma once

#include "Math/Transform.h"

// 부모 인덱스 < 자식 인덱스, 즉 루트 == 0. 루트의 부모 == -1

struct FMeshBoneInfo
{
	FName Name;
	int32 ParentIndex;
};

struct FReferenceSkeleton
{
	TArray<FMeshBoneInfo> RefBoneInfo;
	TArray<FTransform> RefBonePoses;
};

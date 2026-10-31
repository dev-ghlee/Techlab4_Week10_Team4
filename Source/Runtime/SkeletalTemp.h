#pragma once

#include "Math/Transform.h"

// 부모 인덱스 < 자식 인덱스, 즉 루트 == 0. 루트의 부모 == -1

struct FMeshBoneInfo
{
	FName Name;
	int32 ParentIndex = -1;
};

struct FReferenceSkeleton
{
	TArray<FMeshBoneInfo> RefBoneInfo;
	TArray<FTransform> RefBonePoses;
};

struct FSkeletalPose
{
	const FReferenceSkeleton* Skeleton = nullptr;
	TArray<FTransform> LocalPose;
	TArray<FMatrix> ComponentSpacePose;

	void Reset()
	{
		if (Skeleton)
		{
			LocalPose = Skeleton->RefBonePoses;
		}
	}

	void UpdateComponentSpace()
	{
		if (!Skeleton)
		{
			return;
		}

		if (ComponentSpacePose.Num() != LocalPose.Num())
		{
			ComponentSpacePose.Reset();
			ComponentSpacePose.SetNum(LocalPose.Num());
		}

		for (int32 BoneIndex = 0; BoneIndex < LocalPose.Num(); ++BoneIndex)
		{
			const int32 ParentIndex = Skeleton->RefBoneInfo[BoneIndex].ParentIndex;
			ComponentSpacePose[BoneIndex] = LocalPose[BoneIndex].GetLocalMatrix() *
				(ParentIndex >= 0 ? ComponentSpacePose[ParentIndex] : FMatrix::Identity);
		}
	}

	FQuat GetComponentRotation(int32 BoneIndex) const
	{
		FQuat Rotation = LocalPose[BoneIndex].GetOrientation();
		for (int32 Parent = Skeleton->RefBoneInfo[BoneIndex].ParentIndex; Parent >= 0; Parent = Skeleton->RefBoneInfo[Parent].ParentIndex)
		{
			Rotation = LocalPose[Parent].GetOrientation() * Rotation;
		}
		return Rotation;
	}
};

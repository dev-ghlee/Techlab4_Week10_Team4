#pragma once

#include "Render/ReferenceSkeleton.h"

struct FSkeletalPose
{
	const FReferenceSkeleton* Skeleton = nullptr;
	TArray<FTransform> LocalPose;
	TArray<FMatrix> ComponentSpacePose;

	void Reset()
	{
		if (Skeleton)
		{
			LocalPose = Skeleton->RawRefBonePose;
		}
		// 같은 크기의 다른 Skeleton이나 기준 자세로 돌아가도 전체를 다시 계산한다.
		CachedLocalPose.Reset();
	}

	void UpdateComponentSpace()
	{
		if (!Skeleton)
		{
			return;
		}

		const bool bRebuildAll = ComponentSpacePose.Num() != LocalPose.Num()
			|| CachedLocalPose.Num() != LocalPose.Num();
		if (bRebuildAll)
		{
			ComponentSpacePose.SetNum(LocalPose.Num());
			CachedLocalPose.SetNum(LocalPose.Num());
			DirtyComponentSpace.SetNum(LocalPose.Num());
		}

		for (int32 BoneIndex = 0; BoneIndex < LocalPose.Num(); ++BoneIndex)
		{
			const int32 ParentIndex = Skeleton->RawRefBoneInfo[BoneIndex].ParentIndex;
			DirtyComponentSpace[BoneIndex] = bRebuildAll
				|| LocalPose[BoneIndex] != CachedLocalPose[BoneIndex]
				|| (ParentIndex >= 0 && DirtyComponentSpace[ParentIndex]);
			if (!DirtyComponentSpace[BoneIndex]) continue;
			ComponentSpacePose[BoneIndex] = LocalPose[BoneIndex].GetLocalMatrix() *
				(ParentIndex >= 0 ? ComponentSpacePose[ParentIndex] : FMatrix::Identity);
			CachedLocalPose[BoneIndex] = LocalPose[BoneIndex];
		}
	}

	FQuat GetComponentRotation(int32 BoneIndex) const
	{
		FQuat Rotation = LocalPose[BoneIndex].GetOrientation();
		for (int32 Parent = Skeleton->RawRefBoneInfo[BoneIndex].ParentIndex; Parent >= 0; Parent = Skeleton->RawRefBoneInfo[Parent].ParentIndex)
		{
			Rotation = LocalPose[Parent].GetOrientation() * Rotation;
		}
		return Rotation;
	}

private:
	TArray<FTransform> CachedLocalPose;
	TArray<uint8> DirtyComponentSpace;
};

inline TArray<float> GetJointRadii(const FSkeletalPose& Pose)
{
	constexpr float JointRadiusRatio = 0.07f;
	constexpr float DefaultJointRadius = 0.05f;

	TArray<float> Radii;
	Radii.SetNum(Pose.LocalPose.Num());
	for (float& Radius : Radii)
	{
		Radius = FLT_MAX;
	}

	for (int32 Index = 0; Index < Pose.LocalPose.Num(); ++Index)
	{
		const int32 ParentIndex = Pose.Skeleton->RawRefBoneInfo[Index].ParentIndex;
		if (ParentIndex < 0)
		{
			continue;
		}

		const FVector Delta = Pose.ComponentSpacePose[Index].GetOrigin() - Pose.ComponentSpacePose[ParentIndex].GetOrigin();
		const float Radius = Delta.Length() * JointRadiusRatio;
		if (Radius < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		Radii[Index] = std::min(Radii[Index], Radius);
		Radii[ParentIndex] = std::min(Radii[ParentIndex], Radius);
	}

	for (float& Radius : Radii)
	{
		if (Radius == FLT_MAX)
		{
			Radius = DefaultJointRadius;
		}
	}

	return Radii;
}


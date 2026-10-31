#include "EnginePCH.h"
#include "BoneGizmoTarget.h"

#include "SkeletalTemp.h"

FVector FBoneGizmoTarget::GetPivotLocation() const
{
	return GetLocation();
}

FVector FBoneGizmoTarget::GetLocation() const
{
	return Pose->ComponentSpacePose[BoneIndex].GetOrigin();
}

void FBoneGizmoTarget::SetLocation(const FVector& InLocation)
{
	const int32 ParentIndex = GetParentIndex();
	Pose->LocalPose[BoneIndex].Location = ParentIndex >= 0
		? Pose->ComponentSpacePose[ParentIndex].Inverse().TransformPosition(InLocation)
		: InLocation;
}

FRotator FBoneGizmoTarget::GetRotation() const
{
	return Pose->GetComponentRotation(BoneIndex).ToFRotator();
}

void FBoneGizmoTarget::SetRotation(const FRotator& InRotation)
{
	const int32 ParentIndex = GetParentIndex();
	Pose->LocalPose[BoneIndex].Rotation = ParentIndex >= 0
		? (Pose->GetComponentRotation(ParentIndex).Inverse() * InRotation.Quaternion()).ToFRotator()
		: InRotation;
}

FVector FBoneGizmoTarget::GetScale() const
{
	return Pose->LocalPose[BoneIndex].Scale;
}

void FBoneGizmoTarget::SetScale(const FVector& InScale)
{
	Pose->LocalPose[BoneIndex].Scale = InScale;
}

int32 FBoneGizmoTarget::GetParentIndex() const
{
	return Pose->Skeleton->RefBoneInfo[BoneIndex].ParentIndex;
}

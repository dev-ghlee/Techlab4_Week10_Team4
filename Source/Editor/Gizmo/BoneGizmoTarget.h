#pragma once

#include "GizmoTarget.h"

struct FSkeletalPose;

class FBoneGizmoTarget : public IGizmoTarget
{
public:
	void SetBone(FSkeletalPose* InPose, int32 InBoneIndex)
	{
		Pose = InPose;
		BoneIndex = InBoneIndex;
	}

	virtual FVector GetPivotLocation() const override;

	virtual FVector GetLocation() const override;
	virtual void SetLocation(const FVector& InLocation) override;

	virtual FRotator GetRotation() const override;
	virtual void SetRotation(const FRotator& InRotation) override;

	virtual FVector GetScale() const override;
	virtual void SetScale(const FVector& InScale) override;

private:
	int32 GetParentIndex() const;

	FSkeletalPose* Pose = nullptr;
	int32 BoneIndex = -1;
};

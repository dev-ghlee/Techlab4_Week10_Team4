#pragma once

struct FTransform;
struct FReferenceSkeleton;
struct FSkeletalMeshData;

bool LoadPoseFile(FSkeletalMeshData& Data, const FString& PosePath, FString& OutError);

bool SavePoseFile(const FReferenceSkeleton& Skeleton, const TArray<FTransform>& Pose, const FString& Path, FString& OutError);

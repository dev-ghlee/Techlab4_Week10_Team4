#include "EnginePCH.h"
#include "SkeletalPoseFile.h"

#include "Render/SkeletalMeshData.h"

namespace
{
	bool ReadFloat3(const json& Value, float& A, float& B, float& C)
	{
		if (!Value.is_array() || Value.size() != 3)
		{
			return false;
		}

		for (const json& Element : Value)
		{
			if (!Element.is_number())
			{
				return false;
			}
		}

		A = Value[0].get<float>();
		B = Value[1].get<float>();
		C = Value[2].get<float>();
		return std::isfinite(A) && std::isfinite(B) && std::isfinite(C);
	}

	bool ReadTransform(const json& Bone, FTransform& OutTransform)
	{
		if (!Bone.is_object())
		{
			return false;
		}
		FTransform Result = OutTransform;
		if (Bone.contains("Location") && !ReadFloat3(Bone["Location"], Result.Location.X, Result.Location.Y, Result.Location.Z))
		{
			return false;
		}
		OutTransform = Result;
		if (Bone.contains("Rotation") && !ReadFloat3(Bone["Rotation"], Result.Rotation.Pitch, Result.Rotation.Yaw, Result.Rotation.Roll))
		{
			return false;
		}
		OutTransform = Result;
		if (Bone.contains("Scale") && !ReadFloat3(Bone["Scale"], Result.Scale.X, Result.Scale.Y, Result.Scale.Z))
		{
			return false;
		}
		OutTransform = Result;
		return true;
	}
}

bool LoadPoseFile(FSkeletalMeshData& Data, const FString& PosePath, FString& OutError)
{
	std::ifstream In(PosePath);
	if (!In)
	{
		OutError = "Failed to open pose file: " + PosePath;
		return false;
	}

	const json File = json::parse(In, nullptr, false);
	if (File.is_discarded() || !File.is_object() || !File.contains("Bones") || !File["Bones"].is_object())
	{
		OutError = "Invalid pose file format: " + PosePath;
		return false;
	}

	const FReferenceSkeleton& Skeleton = Data.RefSkeleton;
	TMap<FString, int32> BoneNameToIndex;
	for (int32 Index = 0; Index < Skeleton.RawRefBoneInfo.Num(); ++Index)
	{
		BoneNameToIndex.Add(Skeleton.RawRefBoneInfo[Index].Name.ToString(), Index);
	}

	TArray<FTransform> NewPose = Skeleton.RawRefBonePose;
	for (const auto& [BoneName, BoneValue] : File["Bones"].items())
	{
		const int32* Index = BoneNameToIndex.Find(BoneName);
		if (!Index)
		{
			HTR_LOG(Warning, "Pose file '{}' contains unknown bone '{}'", PosePath, BoneName);
			continue;
		}
		if (!ReadTransform(BoneValue, NewPose[*Index]))
		{
			HTR_LOG(Warning, "Pose file '{}' contains invalid transform for bone '{}'", PosePath, BoneName);
			continue;
		}
	}

	Data.RefSkeleton.RawRefBonePose = std::move(NewPose);
	return true;
}

bool SavePoseFile(const FReferenceSkeleton& Skeleton, const TArray<FTransform>& Pose, const FString& Path, FString& OutError)
{
	if (Pose.Num() != Skeleton.RawRefBoneInfo.Num())
	{
		OutError = "Pose array size does not match skeleton bone count";
		return false;
	}

	json Bones = json::object();
	for (int32 Index = 0; Index < Pose.Num(); ++Index)
	{
		const FTransform& T = Pose[Index];
		Bones[Skeleton.RawRefBoneInfo[Index].Name.ToString()] = {
			{ "Location", { T.Location.X, T.Location.Y, T.Location.Z } },
			{ "Rotation", { T.Rotation.Pitch, T.Rotation.Yaw, T.Rotation.Roll } },
			{ "Scale", { T.Scale.X, T.Scale.Y, T.Scale.Z } }
		};
	}

	json File;
	File["Bones"] = std::move(Bones);

	// 임시 파일에 쓰다가 성공하면 교체
	const FString TempPath = Path + ".tmp";
	{
		std::ofstream Out(TempPath, std::ios::trunc);
		if (!Out || !(Out << File.dump(2)))
		{
			OutError = "Failed to open temporary file for writing: " + TempPath;
			return false;
		}
	}

	std::error_code ErrorCode;
	std::filesystem::rename(TempPath, Path, ErrorCode);
	if (ErrorCode)
	{
		OutError = "Cannot replace pose file: " + ErrorCode.message();
		return false;
	}

	return true;
}

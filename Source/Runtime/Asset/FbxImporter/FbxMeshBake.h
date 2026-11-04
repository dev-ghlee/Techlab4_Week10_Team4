#pragma once

#include "Core/EngineString.h"
#include "Core/Types.h"
#include "Render/StaticMeshData.h"
#include "Render/SkeletalMeshData.h"

#include <iosfwd>

// 파일에 기록하는 타입 값은 명시적으로 고정한다.
enum class EFbxBakeMeshType : uint32
{
	StaticMesh = 0,
	SkeletalMesh = 1
};

struct FFbxBakeHeader
{
	char Magic[4] = { 'F', 'B', 'X', 'B' };
	// 저장 필드나 정점 등의 바이너리 구조가 바뀌면 올린다.
	uint32 Version = 1;
	EFbxBakeMeshType MeshType = EFbxBakeMeshType::StaticMesh;
	int64 SourceWriteTime = 0;
	uint64 SourceFileSize = 0;
	uint64 PayloadSize = 0;
};

// 성공한 타입에 해당하는 포인터 하나만 채운다.
struct FFbxBakeResult
{
	EFbxBakeMeshType MeshType = EFbxBakeMeshType::StaticMesh;
	TUniquePtr<FStaticMeshData> StaticMeshData;
	TUniquePtr<FSkeletalMeshData> SkeletalMeshData;
};

class FFbxMeshBake
{
public:
	static FString GetBakePath(const FString& FbxPath);
	// 실패 시 기존 OutResult를 유지하며 호출자는 FBX 임포트로 진행한다.
	static bool ReadBaked(const FString& FbxPath, FFbxBakeResult& OutResult);
	static bool WriteBaked(const FString& FbxPath, const FStaticMeshData& Data);
	static bool WriteBaked(const FString& FbxPath, const FSkeletalMeshData& Data);

private:
	static constexpr int32 MaxStaticArrayCount = 1'000'000;
	static constexpr int32 MaxSkeletalArrayCount = 1'000'000;
	static constexpr uint32 MaxMaterialSlotCount = 65'536;
	static constexpr int32 MaxBoneCount = 4096;
	static constexpr int32 MaxStringLength = 4096;

	static bool ReadSourceFileInfo(const FString& FbxPath, int64& OutWriteTime, uint64& OutFileSize);
	template<typename T>
	static bool WritePod(std::ofstream& Out, const T& Value);
	template<typename T>
	static bool ReadPod(std::ifstream& In, T& Value);
	template<typename T>
	static bool WriteArray(std::ofstream& Out, const TArray<T>& Array);
	template<typename T>
	static bool ReadArray(std::ifstream& In, TArray<T>& Array, int32 MaxCount);
	static bool WriteString(std::ofstream& Out, const FString& String);
	static bool ReadString(std::ifstream& In, FString& OutString, int32 MaxLength = MaxStringLength);
	static bool WriteHeader(std::ofstream& Out, const FFbxBakeHeader& Header);
	static bool ReadHeader(std::ifstream& In, FFbxBakeHeader& OutHeader);
	static bool WriteMaterialSlot(std::ofstream& Out, const FMeshMaterialSlot& Slot);
	static bool ReadMaterialSlot(std::ifstream& In, FMeshMaterialSlot& OutSlot);
	static bool WriteStaticMeshData(std::ofstream& Out, const FStaticMeshData& Data);
	static bool ReadStaticMeshData(std::ifstream& In, FStaticMeshData& OutData);
	static bool ReadPayload(std::ifstream& In, EFbxBakeMeshType MeshType, FFbxBakeResult& OutResult);
	static bool WriteBone(std::ofstream& Out, const FMeshBoneInfo& Bone, const FTransform& Pose);
	static bool ReadBone(std::ifstream& In, FMeshBoneInfo& OutBone, FTransform& OutPose);
	static bool IsValidBoneParent(int32 BoneIndex, int32 ParentIndex);
	static bool WriteReferenceSkeleton(std::ofstream& Out, const FReferenceSkeleton& Skeleton);
	static bool ReadReferenceSkeleton(std::ifstream& In, FReferenceSkeleton& OutSkeleton);
	static bool WriteSkeletalMeshData(std::ofstream& Out, const FSkeletalMeshData& Data);
	static bool ReadSkeletalMeshData(std::ifstream& In, FSkeletalMeshData& OutData);
	static bool ReadInverseBindMatrices(std::ifstream& In, TArray<FMatrix>& OutMatrices, int32 BoneCount);
};

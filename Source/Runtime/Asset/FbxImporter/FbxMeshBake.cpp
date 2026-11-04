#include "EnginePCH.h"
#include "FbxMeshBake.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>
#include <type_traits>
#include <utility>

template<typename T>
bool FFbxMeshBake::WritePod(std::ofstream& Out, const T& Value)
{
	static_assert(std::is_trivially_copyable_v<T>);
	Out.write(reinterpret_cast<const char*>(&Value), sizeof(T));
	return static_cast<bool>(Out);
}

template<typename T>
bool FFbxMeshBake::ReadPod(std::ifstream& In, T& Value)
{
	static_assert(std::is_trivially_copyable_v<T>);
	In.read(reinterpret_cast<char*>(&Value), sizeof(T));
	return static_cast<bool>(In);
}

template<typename T>
bool FFbxMeshBake::WriteArray(std::ofstream& Out, const TArray<T>& Array)
{
	static_assert(std::is_trivially_copyable_v<T>);
	const uint32 Count = static_cast<uint32>(Array.Num());
	const uint64 ByteCount = sizeof(T) * static_cast<uint64>(Count);
	if (ByteCount > std::numeric_limits<std::streamsize>::max() || !WritePod(Out, Count))
	{
		return false;
	}
	if (Count > 0)
	{
		Out.write(reinterpret_cast<const char*>(Array.GetData()), static_cast<std::streamsize>(ByteCount));
	}
	return static_cast<bool>(Out);
}

template<typename T>
bool FFbxMeshBake::ReadArray(std::ifstream& In, TArray<T>& Array, int32 MaxCount)
{
	static_assert(std::is_trivially_copyable_v<T>);
	uint32 Count = 0;
	if (!ReadPod(In, Count) || MaxCount < 0 || Count > static_cast<uint32>(MaxCount))
	{
		return false;
	}
	const uint64 ByteCount = sizeof(T) * static_cast<uint64>(Count);
	if (ByteCount > std::numeric_limits<std::streamsize>::max())
	{
		return false;
	}
	Array.SetNum(static_cast<int32>(Count));
	if (Count > 0)
	{
		In.read(reinterpret_cast<char*>(Array.GetData()), static_cast<std::streamsize>(ByteCount));
	}
	return static_cast<bool>(In);
}

FString FFbxMeshBake::GetBakePath(const FString& FbxPath)
{
	return FbxPath + ".bin";
}

bool FFbxMeshBake::ReadSourceFileInfo(const FString& FbxPath, int64& OutWriteTime, uint64& OutFileSize)
{
	const std::filesystem::path SourcePath = std::filesystem::u8path(FbxPath);
	std::error_code Error;
	const auto WriteTime = std::filesystem::last_write_time(SourcePath, Error);
	if (Error)
	{
		return false;
	}
	const auto FileSize = std::filesystem::file_size(SourcePath, Error);
	if (Error)
	{
		return false;
	}
	OutWriteTime = static_cast<int64>(WriteTime.time_since_epoch().count());
	OutFileSize = static_cast<uint64>(FileSize);
	return true;
}

bool FFbxMeshBake::WriteString(std::ofstream& Out, const FString& String)
{
	// 읽기와 같은 상한을 적용하여 저장한 문자열이 복원 시 거부되지 않게 한다.
	const uint64 Length = static_cast<uint64>(String.size());
	if (Length > MaxStringLength || !WritePod(Out, static_cast<uint32>(Length)))
	{
		return false;
	}
	Out.write(String.data(), static_cast<std::streamsize>(Length));
	return static_cast<bool>(Out);
}

bool FFbxMeshBake::ReadString(std::ifstream& In, FString& OutString, int32 MaxLength)
{
	uint32 Length = 0;
	if (!ReadPod(In, Length) || MaxLength < 0 || Length > static_cast<uint32>(MaxLength))
	{
		return false;
	}
	OutString.resize(Length);
	In.read(OutString.data(), static_cast<std::streamsize>(Length));
	return static_cast<bool>(In);
}

bool FFbxMeshBake::WriteHeader(std::ofstream& Out, const FFbxBakeHeader& Header)
{
	// 필드별로 기록하므로 구조체 패딩은 파일에 포함하지 않는다 (총 36바이트).
	return WritePod(Out, Header.Magic)
		&& WritePod(Out, Header.Version)
		&& WritePod(Out, Header.MeshType)
		&& WritePod(Out, Header.SourceWriteTime)
		&& WritePod(Out, Header.SourceFileSize)
		&& WritePod(Out, Header.PayloadSize);
}

bool FFbxMeshBake::ReadHeader(std::ifstream& In, FFbxBakeHeader& OutHeader)
{
	FFbxBakeHeader Header;
	const FFbxBakeHeader Expected;
	if (!ReadPod(In, Header.Magic) || !ReadPod(In, Header.Version) ||
		!ReadPod(In, Header.MeshType) || !ReadPod(In, Header.SourceWriteTime) ||
		!ReadPod(In, Header.SourceFileSize) || !ReadPod(In, Header.PayloadSize))
	{
		return false;
	}
	if (std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 || Header.Version != Expected.Version)
	{
		return false;
	}
	if (Header.MeshType != EFbxBakeMeshType::StaticMesh && Header.MeshType != EFbxBakeMeshType::SkeletalMesh)
	{
		return false;
	}
	OutHeader = Header;
	return true;
}

bool FFbxMeshBake::WriteMaterialSlot(std::ofstream& Out, const FMeshMaterialSlot& Slot)
{
	return WriteString(Out, Slot.Name)
		&& WritePod(Out, Slot.BaseColor)
		&& WriteString(Out, Slot.DiffuseTexturePath)
		&& WriteString(Out, Slot.NormalTexturePath);
}

bool FFbxMeshBake::ReadMaterialSlot(std::ifstream& In, FMeshMaterialSlot& OutSlot)
{
	FMeshMaterialSlot Slot;
	if (!ReadString(In, Slot.Name) || !ReadPod(In, Slot.BaseColor) ||
		!ReadString(In, Slot.DiffuseTexturePath) || !ReadString(In, Slot.NormalTexturePath))
	{
		return false;
	}
	OutSlot = std::move(Slot);
	return true;
}

bool FFbxMeshBake::WriteStaticMeshData(std::ofstream& Out, const FStaticMeshData& Data)
{
	FString Error;
	if (!Data.Validate(Error))
	{
		return false;
	}
	if (!WriteArray(Out, Data.Vertices) || !WriteArray(Out, Data.Indices) || !WriteArray(Out, Data.Sections))
	{
		return false;
	}
	if (!WritePod(Out, static_cast<uint32>(Data.MaterialSlots.Num())))
	{
		return false;
	}
	for (const FMeshMaterialSlot& Slot : Data.MaterialSlots)
	{
		if (!WriteMaterialSlot(Out, Slot))
		{
			return false;
		}
	}
	return WritePod(Out, Data.AABB);
}

bool FFbxMeshBake::ReadStaticMeshData(std::ifstream& In, FStaticMeshData& OutData)
{
	FStaticMeshData Data;
	if (!ReadArray(In, Data.Vertices, MaxStaticArrayCount) ||
		!ReadArray(In, Data.Indices, MaxStaticArrayCount) ||
		!ReadArray(In, Data.Sections, MaxStaticArrayCount))
	{
		return false;
	}
	uint32 MaterialSlotCount = 0;
	if (!ReadPod(In, MaterialSlotCount) || MaterialSlotCount > MaxMaterialSlotCount)
	{
		return false;
	}
	for (uint32 SlotIndex = 0; SlotIndex < MaterialSlotCount; ++SlotIndex)
	{
		FMeshMaterialSlot Slot;
		if (!ReadMaterialSlot(In, Slot))
		{
			return false;
		}
		Data.MaterialSlots.Add(std::move(Slot));
	}
	if (!ReadPod(In, Data.AABB))
	{
		return false;
	}
	FString Error;
	if (!Data.Validate(Error))
	{
		return false;
	}
	OutData = std::move(Data);
	return true;
}

bool FFbxMeshBake::WriteBone(std::ofstream& Out, const FMeshBoneInfo& Bone, const FTransform& Pose)
{
	// 실행마다 달라질 수 있는 FName의 내부 ID 대신 문자열을 저장한다.
	return WriteString(Out, Bone.Name.ToString())
		&& WritePod(Out, Bone.ParentIndex)
		&& WritePod(Out, Pose.Location)
		&& WritePod(Out, Pose.Rotation)
		&& WritePod(Out, Pose.Scale);
}

bool FFbxMeshBake::ReadBone(std::ifstream& In, FMeshBoneInfo& OutBone, FTransform& OutPose)
{
	FString BoneName;
	FMeshBoneInfo Bone;
	FTransform Pose;
	if (!ReadString(In, BoneName) || !ReadPod(In, Bone.ParentIndex) ||
		!ReadPod(In, Pose.Location) || !ReadPod(In, Pose.Rotation) || !ReadPod(In, Pose.Scale))
	{
		return false;
	}
	Bone.Name = FName(BoneName);
	OutBone = std::move(Bone);
	OutPose = Pose;
	return true;
}

bool FFbxMeshBake::IsValidBoneParent(int32 BoneIndex, int32 ParentIndex)
{
	return (BoneIndex == 0 && ParentIndex == -1)
		|| (BoneIndex > 0 && ParentIndex >= 0 && ParentIndex < BoneIndex);
}

bool FFbxMeshBake::WriteReferenceSkeleton(std::ofstream& Out, const FReferenceSkeleton& Skeleton)
{
	const int32 BoneCount = Skeleton.RawRefBoneInfo.Num();
	if (BoneCount <= 0 || BoneCount > MaxBoneCount || Skeleton.RawRefBonePose.Num() != BoneCount)
	{
		return false;
	}
	if (!WritePod(Out, static_cast<uint32>(BoneCount)))
	{
		return false;
	}
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const FMeshBoneInfo& Bone = Skeleton.RawRefBoneInfo[BoneIndex];
		if (!IsValidBoneParent(BoneIndex, Bone.ParentIndex) || !WriteBone(Out, Bone, Skeleton.RawRefBonePose[BoneIndex]))
		{
			return false;
		}
	}
	return true;
}

bool FFbxMeshBake::ReadReferenceSkeleton(std::ifstream& In, FReferenceSkeleton& OutSkeleton)
{
	uint32 StoredBoneCount = 0;
	if (!ReadPod(In, StoredBoneCount) || StoredBoneCount == 0 || StoredBoneCount > static_cast<uint32>(MaxBoneCount))
	{
		return false;
	}
	const int32 BoneCount = static_cast<int32>(StoredBoneCount);
	FReferenceSkeleton Skeleton;
	Skeleton.RawRefBoneInfo.SetNum(BoneCount);
	Skeleton.RawRefBonePose.SetNum(BoneCount);
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		FMeshBoneInfo& Bone = Skeleton.RawRefBoneInfo[BoneIndex];
		if (!ReadBone(In, Bone, Skeleton.RawRefBonePose[BoneIndex]) || !IsValidBoneParent(BoneIndex, Bone.ParentIndex))
		{
			return false;
		}
	}
	OutSkeleton = std::move(Skeleton);
	return true;
}

bool FFbxMeshBake::ReadInverseBindMatrices(std::ifstream& In, TArray<FMatrix>& OutMatrices, int32 BoneCount)
{
	if (BoneCount <= 0 || BoneCount > MaxBoneCount)
	{
		return false;
	}
	uint32 MatrixCount = 0;
	if (!ReadPod(In, MatrixCount) || MatrixCount != static_cast<uint32>(BoneCount))
	{
		return false;
	}
	TArray<FMatrix> Matrices;
	Matrices.SetNum(BoneCount);
	for (FMatrix& Matrix : Matrices)
	{
		// FMatrix에는 사용자 정의 대입이 있으므로 내부 float 배열만 읽는다.
		if (!ReadPod(In, Matrix.M))
		{
			return false;
		}
	}
	OutMatrices = std::move(Matrices);
	return true;
}

bool FFbxMeshBake::WriteSkeletalMeshData(std::ofstream& Out, const FSkeletalMeshData& Data)
{
	const int32 BoneCount = Data.RefSkeleton.RawRefBoneInfo.Num();
	const uint32 MaterialSlotCount = static_cast<uint32>(Data.MaterialSlots.Num());
	if (Data.RefBasesInvMatrix.Num() != BoneCount || Data.SkinWeights.Num() != Data.ReferenceVertices.Num() ||
		MaterialSlotCount > MaxMaterialSlotCount)
	{
		return false;
	}
	if (!WriteReferenceSkeleton(Out, Data.RefSkeleton) || !WritePod(Out, static_cast<uint32>(BoneCount)))
	{
		return false;
	}
	for (const FMatrix& Matrix : Data.RefBasesInvMatrix)
	{
		if (!WritePod(Out, Matrix.M))
		{
			return false;
		}
	}
	if (!WriteArray(Out, Data.ReferenceVertices) || !WriteArray(Out, Data.Indices) ||
		!WriteArray(Out, Data.SkinWeights) || !WriteArray(Out, Data.Sections) || !WritePod(Out, MaterialSlotCount))
	{
		return false;
	}
	for (const FMeshMaterialSlot& Slot : Data.MaterialSlots)
	{
		if (!WriteMaterialSlot(Out, Slot))
		{
			return false;
		}
	}
	return WritePod(Out, Data.ReferenceBounds);
}

bool FFbxMeshBake::ReadSkeletalMeshData(std::ifstream& In, FSkeletalMeshData& OutData)
{
	FSkeletalMeshData Data;
	if (!ReadReferenceSkeleton(In, Data.RefSkeleton) ||
		!ReadInverseBindMatrices(In, Data.RefBasesInvMatrix, Data.RefSkeleton.RawRefBoneInfo.Num()))
	{
		return false;
	}
	if (!ReadArray(In, Data.ReferenceVertices, MaxSkeletalArrayCount) ||
		!ReadArray(In, Data.Indices, MaxSkeletalArrayCount) ||
		!ReadArray(In, Data.SkinWeights, MaxSkeletalArrayCount) ||
		!ReadArray(In, Data.Sections, MaxSkeletalArrayCount))
	{
		return false;
	}
	const int32 VertexCount = Data.ReferenceVertices.Num();
	const int32 IndexCount = Data.Indices.Num();
	if (VertexCount == 0 || IndexCount == 0 || IndexCount % 3 != 0 ||
		Data.SkinWeights.Num() != VertexCount || Data.Sections.IsEmpty())
	{
		return false;
	}
	uint32 MaterialSlotCount = 0;
	if (!ReadPod(In, MaterialSlotCount) || MaterialSlotCount == 0 || MaterialSlotCount > MaxMaterialSlotCount)
	{
		return false;
	}
	Data.MaterialSlots.SetNum(static_cast<int32>(MaterialSlotCount));
	for (FMeshMaterialSlot& Slot : Data.MaterialSlots)
	{
		if (!ReadMaterialSlot(In, Slot))
		{
			return false;
		}
	}
	if (!ReadPod(In, Data.ReferenceBounds))
	{
		return false;
	}

	// 별도 Validate 함수 없이 GPU 생성에 필요한 참조 범위만 검사한다.
	for (uint32 VertexIndex : Data.Indices)
	{
		if (VertexIndex >= static_cast<uint32>(VertexCount))
		{
			return false;
		}
	}
	const int32 BoneCount = Data.RefSkeleton.RawRefBoneInfo.Num();
	for (const FSkinWeightInfo& SkinWeight : Data.SkinWeights)
	{
		for (uint32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
		{
			const int32 BoneIndex = SkinWeight.InfluenceBones[InfluenceIndex];
			if (SkinWeight.InfluenceWeights[InfluenceIndex] != 0.0f && (BoneIndex < 0 || BoneIndex >= BoneCount))
			{
				return false;
			}
		}
	}
	uint64 ExpectedBaseIndex = 0;
	for (const FSkelMeshRenderSection& Section : Data.Sections)
	{
		const uint64 SectionEnd = static_cast<uint64>(Section.BaseIndex) + static_cast<uint64>(Section.NumTriangles) * 3;
		if (Section.NumTriangles == 0 || Section.BaseIndex != ExpectedBaseIndex ||
			SectionEnd > static_cast<uint64>(IndexCount) || Section.MaterialIndex >= MaterialSlotCount)
		{
			return false;
		}
		ExpectedBaseIndex = SectionEnd;
	}
	if (ExpectedBaseIndex != static_cast<uint64>(IndexCount))
	{
		return false;
	}
	OutData = std::move(Data);
	return true;
}

bool FFbxMeshBake::ReadPayload(std::ifstream& In, EFbxBakeMeshType MeshType, FFbxBakeResult& OutResult)
{
	FFbxBakeResult Result;
	Result.MeshType = MeshType;
	switch (MeshType)
	{
	case EFbxBakeMeshType::StaticMesh:
		Result.StaticMeshData = MakeUnique<FStaticMeshData>();
		if (!ReadStaticMeshData(In, *Result.StaticMeshData))
		{
			return false;
		}
		break;
	case EFbxBakeMeshType::SkeletalMesh:
		Result.SkeletalMeshData = MakeUnique<FSkeletalMeshData>();
		if (!ReadSkeletalMeshData(In, *Result.SkeletalMeshData))
		{
			return false;
		}
		break;
	default:
		return false;
	}
	OutResult = std::move(Result);
	return true;
}

bool FFbxMeshBake::ReadBaked(const FString& FbxPath, FFbxBakeResult& OutResult)
{
	try
	{
		std::ifstream In(std::filesystem::u8path(GetBakePath(FbxPath)), std::ios::binary | std::ios::ate);
		if (!In)
		{
			return false;
		}
		const std::streampos FileEnd = In.tellg();
		In.seekg(0, std::ios::beg);
		FFbxBakeHeader Header;
		if (!ReadHeader(In, Header))
		{
			return false;
		}
		int64 SourceWriteTime = 0;
		uint64 SourceFileSize = 0;
		if (!ReadSourceFileInfo(FbxPath, SourceWriteTime, SourceFileSize) ||
			Header.SourceWriteTime != SourceWriteTime || Header.SourceFileSize != SourceFileSize)
		{
			return false;
		}
		const std::streampos PayloadStart = In.tellg();
		if (PayloadStart < std::streampos(0) || FileEnd < PayloadStart ||
			static_cast<uint64>(FileEnd - PayloadStart) != Header.PayloadSize)
		{
			return false;
		}
		FFbxBakeResult Result;
		if (!ReadPayload(In, Header.MeshType, Result) || In.tellg() != FileEnd)
		{
			return false;
		}
		OutResult = std::move(Result);
		HTR_LOG(Info, "[FBX Bake] Restored: {}", FbxPath);
		return true;
	}
	catch (const std::exception&)
	{
		// 할당 및 경로 변환 실패도 캐시 미사용으로 처리한다.
		return false;
	}
}

bool FFbxMeshBake::WriteBaked(const FString& FbxPath, const FStaticMeshData& Data)
{
	try
	{
		if (Data.Vertices.Num() > MaxStaticArrayCount || Data.Indices.Num() > MaxStaticArrayCount ||
			Data.Sections.Num() > MaxStaticArrayCount || Data.MaterialSlots.Num() > static_cast<int32>(MaxMaterialSlotCount))
		{
			return false;
		}
		FFbxBakeHeader Header;
		Header.MeshType = EFbxBakeMeshType::StaticMesh;
		if (!ReadSourceFileInfo(FbxPath, Header.SourceWriteTime, Header.SourceFileSize))
		{
			return false;
		}
		std::ofstream Out(std::filesystem::u8path(GetBakePath(FbxPath)), std::ios::binary | std::ios::trunc);
		if (!Out || !WriteHeader(Out, Header))
		{
			return false;
		}
		const std::streampos PayloadStart = Out.tellp();
		if (PayloadStart < std::streampos(0) || !WriteStaticMeshData(Out, Data))
		{
			return false;
		}
		const std::streampos PayloadEnd = Out.tellp();
		if (PayloadEnd < PayloadStart)
		{
			return false;
		}
		Header.PayloadSize = static_cast<uint64>(PayloadEnd - PayloadStart);
		Out.seekp(0, std::ios::beg);
		if (!Out || !WriteHeader(Out, Header))
		{
			return false;
		}
		Out.close();
		if (!Out)
		{
			return false;
		}
		HTR_LOG(Info, "[FBX Bake] Saved: {}", FbxPath);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

bool FFbxMeshBake::WriteBaked(const FString& FbxPath, const FSkeletalMeshData& Data)
{
	try
	{
		if (Data.ReferenceVertices.Num() > MaxSkeletalArrayCount || Data.Indices.Num() > MaxSkeletalArrayCount ||
			Data.SkinWeights.Num() > MaxSkeletalArrayCount || Data.Sections.Num() > MaxSkeletalArrayCount ||
			Data.MaterialSlots.Num() > static_cast<int32>(MaxMaterialSlotCount))
		{
			return false;
		}
		FFbxBakeHeader Header;
		Header.MeshType = EFbxBakeMeshType::SkeletalMesh;
		if (!ReadSourceFileInfo(FbxPath, Header.SourceWriteTime, Header.SourceFileSize))
		{
			return false;
		}
		std::ofstream Out(std::filesystem::u8path(GetBakePath(FbxPath)), std::ios::binary | std::ios::trunc);
		if (!Out || !WriteHeader(Out, Header))
		{
			return false;
		}
		const std::streampos PayloadStart = Out.tellp();
		if (PayloadStart < std::streampos(0) || !WriteSkeletalMeshData(Out, Data))
		{
			return false;
		}
		const std::streampos PayloadEnd = Out.tellp();
		if (PayloadEnd < PayloadStart)
		{
			return false;
		}
		Header.PayloadSize = static_cast<uint64>(PayloadEnd - PayloadStart);
		Out.seekp(0, std::ios::beg);
		if (!Out || !WriteHeader(Out, Header))
		{
			return false;
		}
		Out.close();
		if (!Out)
		{
			return false;
		}
		HTR_LOG(Info, "[FBX Bake] Saved: {}", FbxPath);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

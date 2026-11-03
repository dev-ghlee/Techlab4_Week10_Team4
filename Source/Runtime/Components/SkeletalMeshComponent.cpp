#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	if (SkeletalMesh == InMesh) return;
	// 리소스가 들어오는 지점에서 한 번 검사하고, 본 계산 루프에서는 반복하지 않는다.
	if (InMesh)
	{
		FString Error;
		if (!InMesh->GetMeshData().Validate(Error))
		{
			HTR_LOG(Error, "SetSkeletalMesh: '{}' is invalid ({})", InMesh->GetPath(), Error);
			return;
		}
	}

	SkeletalMesh = InMesh;
	InitializePose();
	// 메시가 바뀌면 재질 슬롯도 달라지므로 이전 덮어쓰기를 비우고 렌더 상태를 갱신한다.
	ClearOverrideMaterials();
	OnTransformDirty();
}

const FSkeletalMeshData* USkeletalMeshComponent::GetSkinnedMeshData() const
{
	return SkeletalMesh ? &SkeletalMesh->GetMeshData() : nullptr;
}

int32 USkeletalMeshComponent::GetNumMaterials() const
{
	return SkeletalMesh ? SkeletalMesh->GetMeshData().MaterialSlots.Num() : 0;
}

FString USkeletalMeshComponent::GetMaterialSlotName(int32 SlotIndex) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetMeshData().MaterialSlots.IsValidIndex(SlotIndex))
		return FString();
	return SkeletalMesh->GetMeshData().MaterialSlots[SlotIndex].Name;
}

UMaterial* USkeletalMeshComponent::GetDefaultMaterial(int32 SlotIndex) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetMeshData().MaterialSlots.IsValidIndex(SlotIndex))
		return nullptr;
	return SkeletalMesh->GetMaterial(static_cast<uint32>(SlotIndex));
}

FBox USkeletalMeshComponent::CalcLocalBounds() const
{
	// CPU 스키닝의 변형 Bounds를 연결하기 전에는 기준 포즈의 Bounds를 사용한다.
	return SkeletalMesh ? SkeletalMesh->GetMeshData().ReferenceBounds : FBox{};
}

void USkeletalMeshComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);
	if (bIsLoading)
	{
		// 리플렉션 로드는 Setter를 거치지 않아 현재 자세를 별도로 초기화한다.
		InitializePose();
		MarkRenderStateDirty();
		OnTransformDirty();
	}
}

void USkeletalMeshComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);
	if (Record.GetUnderlyingArchive().IsLoading())
	{
		InitializePose();
		MarkRenderStateDirty();
		OnTransformDirty();
	}
}

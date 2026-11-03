#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	if (SkeletalMesh == InMesh) return;
	// 원본 검증은 Importer가 마쳤다. 공유 IB가 없는 미초기화 리소스만 거부한다.
	if (InMesh && !InMesh->GetIndexBuffer())
	{
		HTR_LOG(Error, "SetSkeletalMesh: '{}' is not initialized", InMesh->GetPath());
		return;
	}

	MeshObject.reset();
	SkeletalMesh = InMesh;
	InitializePose();
	// 메시가 바뀌면 재질 슬롯도 달라지므로 이전 덮어쓰기를 비우고 렌더 상태를 갱신한다.
	ClearOverrideMaterials();
	OnTransformDirty();
}

bool USkeletalMeshComponent::UpdateSkinnedMesh()
{
	if (!SkeletalMesh) return false;
	if (!bPoseDirty && MeshObject) return true;
	if (!MeshObject)
		MeshObject = MakeUnique<FSkeletalMeshObject>(SkeletalMesh->GetMeshData(), SkeletalMesh->GetIndexBuffer());
	if (!MeshObject->UpdateDynamicData(GetBoneComponentMatrices())) return false;
	if (!MeshObject->UploadSkinnedVertices()) return false;
	// 업로드까지 성공한 자세만 반영 완료로 처리한다.
	bPoseDirty = false;
	// 본만 움직여도 변형 Bounds와 그림자 캐시를 갱신해야 한다.
	OnTransformDirty();
	return true;
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
	if (MeshObject) return MeshObject->GetLocalBounds();
	return SkeletalMesh ? SkeletalMesh->GetMeshData().ReferenceBounds : FBox{};
}

void USkeletalMeshComponent::DestroyRenderState()
{
	// Proxy를 먼저 제거한 뒤 그것이 참조할 수 있는 버퍼를 해제한다.
	Super::DestroyRenderState();
	MeshObject.reset();
	MarkPoseDirty();
}

void USkeletalMeshComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);
	if (bIsLoading)
	{
		// 리플렉션 로드는 Setter를 거치지 않아 현재 자세를 별도로 초기화한다.
		MeshObject.reset();
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
		MeshObject.reset();
		InitializePose();
		MarkRenderStateDirty();
		OnTransformDirty();
	}
}

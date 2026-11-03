#pragma once

#include "Components/SkinnedMeshComponent.h"
#include "Render/SkeletalMesh.h"
#include "SkeletalMeshComponent.generated.h"

UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
	GENERATED_BODY()

public:
	// 메시 교체 시 기준 포즈로 초기화한다. nullptr이면 리소스와 본 자세를 비운다.
	void SetSkeletalMesh(USkeletalMesh* InMesh);
	USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

	int32 GetNumMaterials() const override;
	FString GetMaterialSlotName(int32 SlotIndex) const override;
	UMaterial* GetDefaultMaterial(int32 SlotIndex) const override;
	FBox CalcLocalBounds() const override;

	using Super::Serialize;
	void Serialize(json& Handle, bool bIsLoading) override;
	void Serialize(FStructuredArchive::FRecord Record) override;

protected:
	const FSkeletalMeshData* GetSkinnedMeshData() const override;

private:
	// 리소스 변경은 자세 초기화를 위해 SetSkeletalMesh를 거친다.
	UPROPERTY(VisibleAnywhere)
	USkeletalMesh* SkeletalMesh = nullptr;
};

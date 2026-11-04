#pragma once

#include "Components/SkinnedMeshComponent.h"
#include "Render/SkeletalMesh.h"
#include "Render/SkeletalMeshObject.h"
#include "SkeletalMeshComponent.generated.h"

UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
	GENERATED_BODY()

public:
	// 메시 교체 시 기준 포즈로 초기화한다. nullptr이면 리소스와 본 자세를 비운다.
	void SetSkeletalMesh(USkeletalMesh* InMesh);
	USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }
	const FSkeletalMeshObject* GetMeshObject() const { return MeshObject.get(); }

	// 컬링 전 렌더 준비 단계에서 호출한다. 자세가 같으면 기존 버퍼를 재사용한다.
	bool UpdateSkinnedMesh();
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	int32 GetNumMaterials() const override;
	FString GetMaterialSlotName(int32 SlotIndex) const override;
	UMaterial* GetDefaultMaterial(int32 SlotIndex) const override;
	FBox CalcLocalBounds() const override;

	using Super::Serialize;
	void Serialize(json& Handle, bool bIsLoading) override;
	void Serialize(FStructuredArchive::FRecord Record) override;

protected:
	const FSkeletalMeshData* GetSkinnedMeshData() const override;
	void DestroyRenderState() override;

private:
	// 리소스 변경은 자세 초기화를 위해 SetSkeletalMesh를 거친다.
	UPROPERTY(EditAnywhere)
	USkeletalMesh* SkeletalMesh = nullptr;
	TUniquePtr<FSkeletalMeshObject> MeshObject;
};

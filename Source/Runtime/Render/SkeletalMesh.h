#pragma once

#include "Asset/RenderAsset.h"
#include "Render/SkeletalMeshData.h"
#include "SkeletalMesh.generated.h"

class UMaterial;

UCLASS()
class USkeletalMesh : public URenderAsset
{
	GENERATED_BODY()

public:
	// 임포트 후 공유하는 원본. 현재 본 자세는 컴포넌트가 보관한다.
	FSkeletalMeshData MeshData;
	// MeshData.MaterialSlots와 같은 슬롯 순서로 보관한다.
	TArray<UMaterial*> Materials;

	const FSkeletalMeshData& GetMeshData() const { return MeshData; }
	UMaterial* GetMaterial(uint32 SlotIndex) const;
};

#pragma once

#include "Asset/RenderAsset.h"
#include "Render/Buffer.h"
#include "Render/SkeletalMeshData.h"
#include "SkeletalMesh.generated.h"

UCLASS()
class USkeletalMesh : public URenderAsset
{
	GENERATED_BODY()

public:
	USkeletalMesh() = default;
	~USkeletalMesh() override = default;

	// 등록 전에 한 번 호출한다.
	// 데이터, 재질, 버퍼의 대응을 확인한 뒤 내부로 이동한다.
	// 실패하거나 이미 초기화된 경우 false와 오류를 반환한다.
	bool Initialize(FSkeletalMeshData&& InMeshData, TArray<FSkeletalMaterial>&& InMaterials, TUniquePtr<FIndexBuffer> InIndexBuffer, FString& OutError);

	// B가 원본 정점, Weight, 역바인드 등을 읽는 인터페이스.
	const FSkeletalMeshData& GetMeshData() const
	{
		return MeshData;
	}

	// B의 포즈 초기화와 C의 본 트리 표시에 사용한다.
	const FReferenceSkeleton& GetRefSkeleton() const
	{
		return MeshData.RefSkeleton;
	}

	// 슬롯의 실제 재질을 반환한다. 없으면 기본 재질을 반환한다.
	UMaterial* GetMaterial(uint32 SlotIndex) const;

	// 렌더링에 사용하는 공유 인덱스 버퍼.
	FIndexBuffer* GetIndexBuffer() const
	{
		return IndexBuffer.get();
	}

private:
	// 임포트 완료 후 공유하는 원본 데이터.
	FSkeletalMeshData MeshData;

	// Section.MaterialIndex로 참조하는 실제 재질 슬롯.
	TArray<FSkeletalMaterial> Materials;

	// 이 리소스가 소유하는 공유 인덱스 버퍼.
	TUniquePtr<FIndexBuffer> IndexBuffer;
};

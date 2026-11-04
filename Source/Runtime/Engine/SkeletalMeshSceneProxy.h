#pragma once

#include "Engine/PrimitiveSceneProxy.h"

class USkeletalMeshComponent;

// 현재 엔진은 메인 스레드에서 버퍼를 갱신하고, 이후 수집/렌더 단계에서는 캐시만 읽는다.
class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent);

	void UpdateDynamicData() override;
	void UpdateRenderState() override;
	FBox GetLocalBounds() const override { return LocalBounds; }
	FMeshGeometry GetGeometry(uint32 = 0) const override { return Geometry; }
	const FMeshRenderState* GetRenderState() const override;
	void GatherMeshElements(FRenderQueue& Queue, const FViewContext& View) const override;

private:
	USkeletalMeshComponent* SkeletalMeshComponent;
	FMeshRenderState MeshRenderState;
	FMeshGeometry Geometry;
	FBox LocalBounds{};
};

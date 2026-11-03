#pragma once

#include "Math/Frustum.h"
#include "Render/MeshRenderData.h"
#include "Render/MeshGeometry.h"

class FScene;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterial;
class FRenderQueue;
struct FViewContext;

class FPrimitiveSceneProxy
{
public:
	explicit FPrimitiveSceneProxy(UPrimitiveComponent* InComponent);
	
	virtual ~FPrimitiveSceneProxy();
	// 컬링 전에 메인 스레드에서 실행한다. 정적 프리미티브는 갱신할 동적 데이터가 없다.
	virtual void UpdateDynamicData() {}
	void UpdateTransform();
	virtual void UpdateRenderState();
	virtual FBox GetLocalBounds() const;
	virtual FMeshGeometry GetGeometry(uint32 LODIndex = 0) const;
	virtual void GatherMeshElements(FRenderQueue& Queue, const FViewContext& View) const;
	void OnMeshRenderDataChanged();
	void OnMeshDestroyed();

	UPrimitiveComponent* GetComponent() const { return Component; }
	const FAABB& GetBounds() const { return Bounds; }
	const FMatrix& GetLocalToWorld() const { return LocalToWorld; }
	const FMatrix& GetWorldToLocal() const { return WorldToLocal; }
	const FLODSphere& GetLODSphere() const { return LODSphere; }
	UStaticMesh* GetMesh() const { return Mesh; }
	bool IsVisible() const { return bVisible; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }
	uint8 GetLODCount() const
	{
		const FMeshRenderState* State = GetRenderState();
		return State ? State->LODCount : 1;
	}
	virtual const FMeshRenderState* GetRenderState() const { return RenderState.get(); }
	const float* GetLODThresholdsSq() const { return GetRenderState()->LODThresholdSq; }
	const FCachedMeshLOD& GetLOD(uint32 Index) const { return GetRenderState()->LODs[Index]; }
	const FCachedMeshSection& GetSection(uint32 Index) const { return GetRenderState()->Sections[Index]; }
	FScene* GetScene() const { return Scene; }

	void SetCastShadow(bool NewValue);
	void SetReceiveShadow(bool NewValue);

	bool bCastsShadow = true;
	bool bReceiveShadow = true;
private:
	friend class FScene;
	FScene* Scene = nullptr;
	UPrimitiveComponent* Component = nullptr;
	UStaticMesh* Mesh = nullptr;
	TSharedPtr<const FMeshRenderState> RenderState;
	FMatrix LocalToWorld;
	FMatrix WorldToLocal;
	FAABB Bounds;
	FLODSphere LODSphere;
	int32 PackedIndex = INDEX_NONE;
	bool bQueuedForUpdate = false;
	bool bRenderStateQueued = false;
	bool bVisible = true;
	bool bHiddenInGame = false;
};

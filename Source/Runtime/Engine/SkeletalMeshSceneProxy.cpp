#include "EnginePCH.h"
#include "Engine/SkeletalMeshSceneProxy.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Scene.h"
#include "Render/RenderPacket.h"

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent), SkeletalMeshComponent(InComponent),
	  LocalBounds(InComponent->CalcLocalBounds())
{
}

// 컬링 전에 스키닝을 갱신하고, 이번 렌더링에 사용할 버퍼와 메시 경계를 보관한다.
void FSkeletalMeshSceneProxy::UpdateDynamicData()
{
	const bool bWasReady = Geometry.IsValid();
	const bool bReady = SkeletalMeshComponent->UpdateSkinnedMesh();
	const FSkeletalMeshObject* MeshObject = SkeletalMeshComponent->GetMeshObject();
	Geometry = bReady && MeshObject ? MeshObject->GetGeometry() : FMeshGeometry{};
	LocalBounds = SkeletalMeshComponent->CalcLocalBounds();
	// 버퍼의 유효 상태가 바뀌면 기존 그림자 캐시도 무효화한다.
	if (bWasReady != Geometry.IsValid()) GetScene()->MarkDirty(this);
}

// 메시나 재질 설정이 바뀌면 Section별 인덱스 범위와 적용할 재질을 다시 준비한다.
void FSkeletalMeshSceneProxy::UpdateRenderState()
{
	// 가시성 및 Cast/Receive Shadow는 기존 Primitive의 갱신 경로를 사용한다.
	FPrimitiveSceneProxy::UpdateRenderState();
	MeshRenderState.Sections.Reset();
	MeshRenderState.LODs[0] = {};
	LocalBounds = SkeletalMeshComponent->CalcLocalBounds();
	const USkeletalMesh* Mesh = SkeletalMeshComponent->GetSkeletalMesh();
	if (!Mesh) return;

	const auto& Sections = Mesh->GetMeshData().Sections;
	MeshRenderState.Sections.Reserve(Sections.Num());
	for (const FSkelMeshRenderSection& Section : Sections)
	{
		// Importer의 삼각형 개수를 렌더러의 인덱스 개수로 변환한다.
		MeshRenderState.Sections.Add({ SkeletalMeshComponent->GetMaterial(Section.MaterialIndex),
			Section.BaseIndex, Section.NumTriangles * 3 });
	}
	// 현재 CPU 스키닝 리소스는 LOD 0 하나를 사용한다.
	MeshRenderState.LODs[0] = { 0, static_cast<uint32>(Sections.Num()) };
}

const FMeshRenderState* FSkeletalMeshSceneProxy::GetRenderState() const
{
	return MeshRenderState.Sections.IsEmpty() ? nullptr : &MeshRenderState;
}

// 미리 준비한 버퍼와 재질을 읽어 Section마다 렌더 패킷을 만든다.
// 같은 스키닝 결과를 기존 화면·깊이·그림자 렌더링 경로에서 사용한다.
void FSkeletalMeshSceneProxy::GatherMeshElements(FRenderQueue& Queue, const FViewContext& View) const
{
	if (!IsShown(View.bGameView) || !Geometry.IsValid()) return;
	for (const FCachedMeshSection& Section : MeshRenderState.Sections)
	{
		if (!Section.Material) continue;
		FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
		Packet.Proxy = this;
		Packet.Geometry = Geometry;
		Packet.Material = Section.Material;
		Packet.StartIndex = Section.StartIndex;
		Packet.IndexCount = Section.IndexCount;
	}
}

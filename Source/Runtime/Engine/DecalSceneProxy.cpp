#include "EnginePCH.h"
#include "DecalSceneProxy.h"
#include "Render/Material.h"
#include "Math/Color.h"

FDecalSceneProxy::FDecalSceneProxy(const UDecalComponent* InComponent)
{
	Component = InComponent;

	UpdateTransform();
	UpdateRenderState();
}

void FDecalSceneProxy::UpdateTransform()
{
	if(!Component)
	{
		return;
	}

	FVector ScaledExtent = Component->DecalSize * Component->GetWorldScale3D();

	DecalToWorld = Component->GetWorldMatrix();
	WorldToDecal = DecalToWorld.Inverse();

	WorldBounds = Component->CalcLocalBounds().GetWorldAABB(DecalToWorld);
	WorldOBB.Center = FVector(DecalToWorld.M[3][0], DecalToWorld.M[3][1], DecalToWorld.M[3][2]);
	for(uint32 i = 0; i < 3; i++)
	{
		WorldOBB.Axis[i] = FVector(DecalToWorld.M[i][0], DecalToWorld.M[i][1], DecalToWorld.M[i][2]).Normalized();
	}
	WorldOBB.Extent = ScaledExtent;

	FMatrix DecalView = GetViewMatrix(Component->GetWorldRotation().Quaternion(), Component->GetWorldLocation());

	float Width = ScaledExtent.Y * 2.0f;
	float Height = ScaledExtent.Z * 2.0f;
	float AspectRatio = Height > 0.0f ? Width / Height : 1.0f;	
	float Near = -ScaledExtent.X;
	float Far = ScaledExtent.X;

	FMatrix DecalProjection = Ortho(Width, AspectRatio, Near, Far);

	DecalViewProjection = DecalView * DecalProjection;
}

void FDecalSceneProxy::UpdateRenderState()
{
	if(!Component)
	{
		return;
	}

	DecalMaterial = Component->GetDecalMaterial();
	DecalColor = Component->DecalColor;
	SortOrder = Component->SortOrder;
	FadeScreenSize = Component->FadeScreenSize > 0.0f ? 1.0f / Component->FadeScreenSize : 0.0f;
	InvFadeDuration = Component->FadeDuration > 0.0f  ? 1.0f / Component->FadeDuration : 0.0f;
	InvFadeInDuration = 1.0f / Component->FadeInDuration;
	DrawInGame = Component->IsVisible() && !Component->IsHiddenInGame();
}

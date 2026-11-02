#pragma once

#include "Math/EngineMath.h"
#include "Math/Vector.h"
#include "Math/Box.h"

#include "Render/Buffer.h"

#include "Components/DecalComponent.h"

enum class EDecalRenderMode : uint8
{
	ProjectedTextureMapping,
	ScreenSpace
};

class UMaterial;

class FDecalSceneProxy
{
public:
	FDecalSceneProxy(const UDecalComponent* InComponent);

	virtual ~FDecalSceneProxy() = default;

	void UpdateTransform();
	void UpdateRenderState();

	FMatrix DecalViewProjection;
	FWorldBox WorldOBB;
	FBox WorldBounds;

	FMatrix DecalToWorld;
	FMatrix WorldToDecal;

	const UDecalComponent* Component = nullptr;

	UMaterial* DecalMaterial = nullptr;
	int32 SortOrder = 0;
	float FadeScreenSize = 0.0f;
	float InvFadeDuration = -1.0f;
	float InvFadeInDuration = 1.0f;
	FColor DecalColor;
	bool DrawInGame = true;	
};

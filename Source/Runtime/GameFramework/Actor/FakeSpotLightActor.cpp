#include "EnginePCH.h"
#include "FakeSpotLightActor.h"
#include "Components/DecalComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Asset/AssetManager.h"
#include "Render/Material.h"
#include "Engine/DecalSceneProxy.h"
#include <algorithm>

AFakeSpotLightActor::AFakeSpotLightActor()
{
	// 1. Decal Component (바닥/벽면에 투영되는 조명 스팟)
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("DecalComponent");
	SetRootComponent(DecalComponent);
	DecalComponent->SetDecalSize(FVector(MaxDistance, SpotRadius, SpotRadius));
	DecalComponent->SetDecalColor(LightColor);
	DecalComponent->bIsEmissive = true;

	UMaterial* BaseDecalMat = UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
	if (BaseDecalMat)
	{
		UMaterial* DecalMat = UMaterial::CreateInstance(BaseDecalMat);
		UTexture2D* SpotTex = UAssetManager::Get().LoadTexture("Assets/Icons/FakeSpot.png");
		if (SpotTex)
		{
			DecalMat->Textures[0] = SpotTex;
		}
		DecalComponent->SetDecalMaterial(DecalMat);
	}

	// 2. Billboard Component (광원 위치의 빛나는 플레어/글로우)
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("BillboardComponent");
	BillboardComponent->SetupAttachment(RootComponent);
	BillboardComponent->SetEditorIcon(true);
	BillboardComponent->SetIconSize(FlareSize);
	BillboardComponent->bCastShadow = false;
	BillboardComponent->bReceiveShadow = false;
	BillboardComponent->SetHiddenInGame(false);

	UMaterial* BaseBB = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	UTexture2D* SpotIcon = UAssetManager::Get().LoadTexture("Assets/Icons/FakeSpotIcon.png");
	if (BaseBB && SpotIcon)
	{
		UMaterial* BBMaterial = UMaterial::CreateInstance(BaseBB);
		BBMaterial->Textures[0] = SpotIcon;
		BillboardComponent->SetMaterial(0, BBMaterial);
	}

	// 3. Arrow Component (에디터 전용 방향 가이드)
	ArrowComponent = CreateDefaultSubobject<UStaticMeshComponent>("ArrowComponent");
	ArrowComponent->SetupAttachment(RootComponent);
	ArrowComponent->SetEditorIcon(true);
	ArrowComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Arrow"));
	ArrowComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("EditorArrowMaterial"));
	ArrowComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	ArrowComponent->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.5f));
	ArrowComponent->SetHiddenInDetails(true);
	ArrowComponent->SetCastShadow(false);
	ArrowComponent->SetReceiveShadow(false);
	ArrowComponent->SetHiddenInGame(true);
	ArrowComponent->bReceivesDecals = false;
}

void AFakeSpotLightActor::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	UpdateParameters();
}

void AFakeSpotLightActor::UpdateParameters()
{
	if (DecalComponent)
	{
		FColor FinalColor = LightColor;
		float ScaledAlpha = (static_cast<float>(LightColor.A) / 255.0f) * Intensity * 255.0f;
		FinalColor.A = static_cast<uint8>(std::clamp(ScaledAlpha, 0.0f, 255.0f));
		DecalComponent->SetDecalColor(FinalColor);
		DecalComponent->SetDecalSize(FVector(MaxDistance, SpotRadius, SpotRadius));
		DecalComponent->SceneProxy->UpdateTransform();
	}

	if (BillboardComponent)
	{
		BillboardComponent->SetIconSize(FlareSize);
	}
}

void AFakeSpotLightActor::SetLightColor(const FColor& InColor)
{
	LightColor = InColor;
	UpdateParameters();
}

void AFakeSpotLightActor::SetIntensity(float InIntensity)
{
	Intensity = InIntensity;
	UpdateParameters();
}

void AFakeSpotLightActor::SetSpotRadius(float InRadius)
{
	SpotRadius = InRadius;
	UpdateParameters();
}

void AFakeSpotLightActor::SetMaxDistance(float InDistance)
{
	MaxDistance = InDistance;
	UpdateParameters();
}

void AFakeSpotLightActor::SetFlareSize(float InSize)
{
	FlareSize = InSize;
	UpdateParameters();
}

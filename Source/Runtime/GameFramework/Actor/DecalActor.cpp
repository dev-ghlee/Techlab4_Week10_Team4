#include "EnginePCH.h"
#include "DecalActor.h"
#include "Components/DecalComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Asset/AssetManager.h"

ADecalActor::ADecalActor()
{
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("DecalComponent");
	SetRootComponent(DecalComponent);	

	SpriteComponent = CreateDefaultSubobject<UBillboardComponent>("SpriteComponent");
	SpriteComponent->SetupAttachment(RootComponent);
	SpriteComponent->SetEditorIcon(true);
	SpriteComponent->SetHiddenInDetails(true);
	SpriteComponent->SetHiddenInGame(true);
	UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	UTexture2D* Icon = UAssetManager::Get().LoadTexture("Assets/Icons/Decal.png");
	if(Base && Icon)
	{
		UMaterial* IconMat = UMaterial::CreateInstance(Base);
		IconMat->Textures[0] = Icon;
		SpriteComponent->SetMaterial(0, IconMat);
	}

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

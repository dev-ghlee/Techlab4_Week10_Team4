#include "EnginePCH.h"
#include "DecalActor.h"
#include "Components/DecalComponent.h"
#include "Components/BillboardComponent.h"

ADecalActor::ADecalActor()
{
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("DecalComponent");
	SetRootComponent(DecalComponent);

	SpriteComponent = CreateDefaultSubobject<UBillboardComponent>("SpriteComponent");
	SpriteComponent->SetupAttachment(RootComponent);
}

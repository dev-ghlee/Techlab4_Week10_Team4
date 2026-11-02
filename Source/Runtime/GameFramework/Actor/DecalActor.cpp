#include "EnginePCH.h"
#include "DecalActor.h"
#include "Components/DecalComponent.h"

ADecalActor::ADecalActor()
{
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("DecalComponent");
	SetRootComponent(DecalComponent);
}

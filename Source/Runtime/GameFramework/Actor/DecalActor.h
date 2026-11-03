#pragma once

#include "GameFrameWork/Actor.h"
#include "DecalActor.generated.h"

class UDecalComponent;
class UBillboardComponent;


UCLASS()
class ADecalActor : public AActor
{
	GENERATED_BODY()

public:
	ADecalActor();

	UDecalComponent* GetDecal() const { return DecalComponent; }

private:
	UDecalComponent* DecalComponent;
	UBillboardComponent* SpriteComponent;
};
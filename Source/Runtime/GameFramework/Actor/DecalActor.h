#pragma once

#include "GameFrameWork/Actor.h"
#include "DecalActor.generated.h"

class UDecalComponent;
class UBillboardComponent;
class UStaticMeshComponent;

UCLASS()
class ADecalActor : public AActor
{
	GENERATED_BODY()

public:
	ADecalActor();

	UDecalComponent* GetDecal() const { return DecalComponent; }

	virtual void RegisterAllComponents() override;

private:
	UDecalComponent* DecalComponent;
	UBillboardComponent* SpriteComponent;
	UStaticMeshComponent* ArrowComponent;
};
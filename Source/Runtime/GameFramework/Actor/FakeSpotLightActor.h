#pragma once

#include "GameFramework/Actor.h"
#include "FakeSpotLightActor.generated.h"

class UDecalComponent;
class UBillboardComponent;
class UStaticMeshComponent;

UCLASS()
class AFakeSpotLightActor : public AActor
{
	GENERATED_BODY()

public:
	AFakeSpotLightActor();

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	UDecalComponent* GetDecalComponent() const { return DecalComponent; }
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }

	void SetLightColor(const FColor& InColor);
	void SetIntensity(float InIntensity);
	void SetSpotRadius(float InRadius);
	void SetMaxDistance(float InDistance);
	void SetFlareSize(float InSize);

	UPROPERTY(EditAnywhere)
	FColor LightColor = FColor(255, 240, 200, 255);

	UPROPERTY(EditAnywhere)
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere)
	float SpotRadius = 2.0f;

	UPROPERTY(EditAnywhere)
	float MaxDistance = 10.0f;

	UPROPERTY(EditAnywhere)
	float FlareSize = 1.5f;

private:
	void UpdateParameters();

	UDecalComponent* DecalComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	UStaticMeshComponent* ArrowComponent = nullptr;
};

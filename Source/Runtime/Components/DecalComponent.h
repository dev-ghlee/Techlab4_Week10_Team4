#pragma once

#include "SceneComponent.h"
#include "DecalComponent.generated.h"

class UMaterial;
class FDecalSceneProxy;
class FLineBatcher;
struct FLinearColor;
class FLineBatcher;

void DrawDebugDecal(UWorld* World, FLineBatcher* LineBatcher, const AActor* SelectedActor);

UCLASS()
class UDecalComponent : public USceneComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	UMaterial* DecalMaterial;

public:
	UPROPERTY(EditAnywhere)
	int32 SortOrder;

	UPROPERTY(EditAnywhere)
	float FadeScreenSize;

	UPROPERTY(EditAnywhere)
	float FadeStartDelay;

	UPROPERTY(EditAnywhere)
	float FadeDuration;

	UPROPERTY(EditAnywhere)
	float FadeInDuration;

	UPROPERTY(EditAnywhere)
	float FadeInStartDelay;

	UPROPERTY(EditAnywhere)
	bool bDestroyOwnerAfterFade;

	UPROPERTY(EditAnywhere)
	FVector DecalSize;

	UPROPERTY(EditAnywhere)
	FColor DecalColor;

	UPROPERTY(EditAnywhere)
	bool bIsEmissive = false;

	UDecalComponent();

	FDecalSceneProxy* SceneProxy = nullptr;

	virtual void CreateRenderState() override;
	virtual void DestroyRenderState() override;

	virtual void OnTransformDirty() override;
	virtual void MarkRenderStateDirty() override;

	// Getter-Setter
	UMaterial* GetDecalMaterial() const;
	void SetDecalMaterial(UMaterial* InMaterial);

	int32 GetSortOrder() const;
	void SetSortOrder(const int32 InSortOrder);

	float GetFadeScreenSize() const;
	void SetFadeScreenSize(const float InScreenSize);

	float GetFadeStartDelay() const;
	float GetFadeDuration() const;
	float GetFadeInDuration() const;
	bool GetDestroyOwnerAfterFade() const;
	void SetFadeOut(float StartDelay, float Duration, bool DestroOwnerAfterFade = true);
	void SetFadeIn(float StartDelay, float Duration);

	FVector GetDecalSize() const;
	void SetDecalSize(FVector InSize);

	FColor GetDecalColor() const;
	void SetDecalColor(FColor InColor);	

	// Interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	virtual FBox CalcLocalBounds() const override;

	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	// Decal Component functions
	void SetLifeSpan(const float LifeSpan);

	void DrawDebug(FLineBatcher* LineBatcher, const FVector4& Color) const;

protected:
	virtual bool ShouldCreateRenderState() const override { return true; }
	virtual void LifeSpanCallback();

private:
	// TODO : Change these to TimerHandle
	float ElapsedTime = 0.0f;
	float LifeSpanTime = 0.0f;
	bool bHasStartedSpanTime = false;

	void HandleLifeSpan(float DeltaTime);
	/////////////////////////////////////

};
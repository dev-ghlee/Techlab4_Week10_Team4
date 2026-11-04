#include "EnginePCH.h"
#include "DecalComponent.h"
#include "Math/Color.h"
#include "GameFrameWork/Actor.h"
#include "Render/Material.h"
#include "Engine/World.h"
#include "Engine/Scene.h"
#include "Render/LineBatcher.h"
#include "UObject/UObjectIterator.h"

UDecalComponent::UDecalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

    FadeScreenSize = 0.01f;
    FadeStartDelay = 0.0f;
    FadeDuration = 0.0f;
    bDestroyOwnerAfterFade = true;
    DecalSize = FVector(1.0f, 1.0f, 1.0f);
    DecalColor = FColor(255, 255, 255, 255);
    bIsEmissive = false;
    SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
}

void UDecalComponent::CreateRenderState()
{
    Super::CreateRenderState();
    GetWorld()->GetScene().AddDecal(this);
}

void UDecalComponent::DestroyRenderState()
{
    GetWorld()->GetScene().RemoveDecal(this);
    Super::DestroyRenderState();
}

void UDecalComponent::OnTransformDirty()
{
    if(SceneProxy)
    {
        SceneProxy->UpdateTransform();
    }
}

void UDecalComponent::MarkRenderStateDirty()
{
    if(SceneProxy)
    {
        SceneProxy->UpdateRenderState();
    }
}

UMaterial* UDecalComponent::GetDecalMaterial() const
{
    return DecalMaterial;
}

void UDecalComponent::SetDecalMaterial(UMaterial* InMaterial)
{
    DecalMaterial = InMaterial;
    MarkRenderStateDirty();
}

int32 UDecalComponent::GetSortOrder() const
{
    return SortOrder;
}

void UDecalComponent::SetSortOrder(const int32 InSortOrder)
{
    SortOrder = InSortOrder;
    MarkRenderStateDirty();
}

float UDecalComponent::GetFadeScreenSize() const
{
    return FadeScreenSize;
}

void UDecalComponent::SetFadeScreenSize(const float InScreenSize)
{
    FadeScreenSize = InScreenSize;
    MarkRenderStateDirty();
}

float UDecalComponent::GetFadeStartDelay() const
{
    return FadeStartDelay;
}

float UDecalComponent::GetFadeDuration() const
{
    return FadeDuration;
}

float UDecalComponent::GetFadeInDuration() const
{
    return FadeInDuration;
}

bool UDecalComponent::GetDestroyOwnerAfterFade() const
{
    return bDestroyOwnerAfterFade;
}

void UDecalComponent::SetFadeOut(float StartDelay, float Duration, bool DestroOwnerAfterFade)
{
    FadeStartDelay = StartDelay;
    FadeDuration = Duration;
    bDestroyOwnerAfterFade = DestroOwnerAfterFade;

    SetLifeSpan(FadeStartDelay + FadeDuration);
}

void UDecalComponent::SetFadeIn(float StartDelay, float Duration)
{
    FadeInStartDelay = StartDelay;
    FadeInDuration = Duration;
}

FVector UDecalComponent::GetDecalSize() const
{
    return DecalSize;
}

void UDecalComponent::SetDecalSize(FVector InSize)
{
    DecalSize = InSize;
    MarkRenderStateDirty();
}

FColor UDecalComponent::GetDecalColor() const
{
	SceneProxy->UpdateRenderState();
    return DecalColor;
}

void UDecalComponent::SetDecalColor(FColor InColor)
{
    DecalColor = InColor;
    MarkRenderStateDirty();
}

void UDecalComponent::BeginPlay()
{
    Super::BeginPlay();

    float SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if(SceneProxy)
    {
        SceneProxy->InitializeFadingParameters(SpawnTime, FadeDuration, FadeStartDelay, FadeInDuration, FadeInStartDelay);
        SceneProxy->UpdateRenderState();
        SceneProxy->UpdateTransform();
    }

    SetLifeSpan(FadeStartDelay + FadeDuration);
}

void UDecalComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    HandleLifeSpan(DeltaTime);
}

void UDecalComponent::PostEditChangeProperty(const FProperty& Property)
{
    Super::PostEditChangeProperty(Property);

	if(Property.Name == "DecalMaterial" || Property.Name == "DecalColor" || Property.Name == "bIsEmissive" || Property.Name == "SortOrder" || Property.Name == "FadeScreenSize" || Property.Name == "FadeStartDelay" || Property.Name == "FadeDuration" || Property.Name == "FadeInDuration" || Property.Name == "FadeInStartDelay" || Property.Name == "bDestroyOwnerAfterFade")
	{
		SceneProxy->UpdateRenderState();
	}
	if(Property.Name == "DecalSize")
	{
		SceneProxy->UpdateTransform();
	}
}

FBox UDecalComponent::CalcLocalBounds() const
{
    return FBox(-DecalSize, DecalSize);
}

void UDecalComponent::Serialize(FStructuredArchive::FRecord Record)
{
    Super::Serialize(Record);

    if(Record.GetUnderlyingArchive().IsLoading())
    {
        MarkRenderStateDirty();
        OnTransformDirty();
    }
}

void UDecalComponent::SetLifeSpan(const float LifeSpan)
{
    if(LifeSpan > 0.0f)
    {
        ElapsedTime = 0.0f;
        LifeSpanTime = LifeSpan;
        bHasStartedSpanTime = true;
    }
    else
    {
        ElapsedTime = 0.0f;
        LifeSpanTime = 0.0f;
        bHasStartedSpanTime = false;
    }
}

void UDecalComponent::DrawDebug(FLineBatcher* LineBatcher, const FVector4& Color) const
{
    LineBatcher->AddOrientedBox(CalcLocalBounds(), GetWorldMatrix(), Color);
}

void UDecalComponent::LifeSpanCallback()
{
    if(bDestroyOwnerAfterFade && (FadeDuration > 0.0f || FadeStartDelay > 0.0f))
    {
        if(AActor* Owner = GetOwner())
        {
            //Owner->DestroyComponent(this);
            Owner->Destroy();
        }
    }
}

void UDecalComponent::HandleLifeSpan(float DeltaTime)
{
    if(!bHasStartedSpanTime)
    {
        return;
    }

    ElapsedTime += DeltaTime;

    if(ElapsedTime >= LifeSpanTime)
    {
        ElapsedTime = 0.0f;
        LifeSpanTime = 0.0f;
        bHasStartedSpanTime = false;

        LifeSpanCallback();
    }
}

void DrawDebugDecal(UWorld* World, FLineBatcher* LineBatcher, const AActor* SelectedActor)
{
    if(!World || !LineBatcher || !SelectedActor)
    {
        return;
    }

	for(TObjectIterator<UDecalComponent> It; It; ++It)
	{
		UDecalComponent* Decal = *It;
		if(!Decal || !Decal->IsRegistered() || Decal->GetOwner() != SelectedActor)
		{
			continue;
		}
		FColor Color = FColor(1.0f, 1.0f, 0.0f, 1.0f);
		Decal->DrawDebug(LineBatcher, Color.ToVector4());
	}
}

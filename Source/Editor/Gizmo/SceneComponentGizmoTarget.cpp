#include "EnginePCH.h"
#include "SceneComponentGizmoTarget.h"

#include "Components/LocalLightComponent.h"
#include "GameFramework/Actor/LightActorBase.h"

FVector FSceneComponentGizmoTarget::GetPivotLocation() const
{
	if (Target)
	{
		return Target->GetWorldLocation();
	}
	return FVector::ZeroVector;
}

FVector FSceneComponentGizmoTarget::GetLocation() const
{
	if (Target)
	{
		return Target->GetRelativeLocation();
	}
	return FVector::ZeroVector;
}

void FSceneComponentGizmoTarget::SetLocation(const FVector& InLocation)
{
	if (Target)
	{
		Target->SetRelativeLocation(InLocation);
	}
}

FRotator FSceneComponentGizmoTarget::GetRotation() const
{
	if (Target)
	{
		return Target->GetRelativeRotation();
	}
	return FRotator::Identity;
}

void FSceneComponentGizmoTarget::SetRotation(const FRotator& InRotation)
{
	if (Target)
	{
		Target->SetRelativeRotation(InRotation);
	}
}

FVector FSceneComponentGizmoTarget::GetScale() const
{
	if (Target)
	{
		return Target->GetRelativeScale3D();
	}
	return FVector::OneVector;
}

void FSceneComponentGizmoTarget::SetScale(const FVector& InScale)
{
	if (Target)
	{
		Target->SetRelativeScale3D(InScale);
	}
}

void FSceneComponentGizmoTarget::OnBeginDrag()
{
	if (ULocalLightComponent* Light = GetTargetLight())
	{
		DragStartRadius = Light->GetAttenuationRadius();
	}
}

bool FSceneComponentGizmoTarget::ApplyScaleFactor(float Factor)
{
	ULocalLightComponent* Light = GetTargetLight();
	if (!Light)
	{
		return false;
	}
	Light->SetAttenuationRadius(DragStartRadius * Factor);
	return true;
}

ULocalLightComponent* FSceneComponentGizmoTarget::GetTargetLight() const
{
	if (!Target)
	{
		return nullptr;
	}
	ALightActorBase* LightActor = Cast<ALightActorBase>(Target->GetOwner());
	return LightActor ? Cast<ULocalLightComponent>(LightActor->GetLightComponent()) : nullptr;
}

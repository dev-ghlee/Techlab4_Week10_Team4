#pragma once

#include "GizmoTarget.h"
#include "Components/SceneComponent.h"

class ULocalLightComponent;

class FSceneComponentGizmoTarget : public IGizmoTarget
{
public:
	void SetTarget(USceneComponent* InTarget) { Target = InTarget; }
	USceneComponent* GetTarget() const { return Target; }

	virtual FVector GetPivotLocation() const override;

	virtual FVector GetLocation() const override;
	virtual void SetLocation(const FVector& InLocation) override;

	virtual FRotator GetRotation() const override;
	virtual void SetRotation(const FRotator& InRotation) override;

	virtual FVector GetScale() const override;
	virtual void SetScale(const FVector& InScale) override;

	virtual void OnBeginDrag() override;
	virtual bool ApplyScaleFactor(float Factor) override;

private:
	ULocalLightComponent* GetTargetLight() const;

	USceneComponent* Target = nullptr;

	float DragStartRadius = 0.0f;
};

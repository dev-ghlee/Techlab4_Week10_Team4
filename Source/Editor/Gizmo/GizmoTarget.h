#pragma once

class IGizmoTarget
{
public:
	virtual ~IGizmoTarget() = default;
	virtual FVector GetPivotLocation() const = 0;

	virtual FVector GetLocation() const = 0;
	virtual void SetLocation(const FVector& InLocation) = 0;

	virtual FRotator GetRotation() const = 0;
	virtual void SetRotation(const FRotator& InRotation) = 0;

	virtual FVector GetScale() const = 0;
	virtual void SetScale(const FVector& InScale) = 0;

	virtual void OnBeginDrag() {}
	virtual bool ApplyScaleFactor(float Factor) { return false; }
};

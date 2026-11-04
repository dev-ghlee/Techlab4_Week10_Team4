#pragma once

#include "Components/PrimitiveComponent.h"
#include "Engine/PrimitiveSceneProxy.h"

class FOutline
{
public:
	void SetTarget(UPrimitiveComponent* InTarget) { Target = InTarget; }
	UPrimitiveComponent* GetTarget() const { return Target; }
	UStaticMesh* GetMesh() const { return Target ? Target->GetRenderMesh() : nullptr; }
	FMeshGeometry GetGeometry() const
	{
		// 스켈레탈 메시는 현재 변형된 버퍼를 사용한다.
		if (Target && Target->GetSceneProxy())
		{
			const FMeshGeometry Geometry = Target->GetSceneProxy()->GetGeometry();
			if (Geometry.IsValid()) return Geometry;
		}
		UStaticMesh* Mesh = GetMesh();
		return Mesh ? Mesh->GetGeometry() : FMeshGeometry{};
	}
	FMatrix GetWorldMatrix() const { return Target->GetWorldMatrix(); }
	const FVector& GetTargetScale() const { return Target->GetRelativeScale3D(); }
private:
	UPrimitiveComponent* Target = nullptr;

};

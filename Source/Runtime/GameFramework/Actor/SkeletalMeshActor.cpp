#include "EnginePCH.h"
#include "SkeletalMeshActor.h"

ASkeletalMeshActor::ASkeletalMeshActor()
{
	USkeletalMeshComponent* MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
	MeshComponent->SetupAttachment(GetRootComponent());
}

USkeletalMeshComponent* ASkeletalMeshActor::GetSkeletalMeshComponent() const
{
	// 컴포넌트 삭제나 PIE 복제 후에도 현재 액터가 소유한 컴포넌트만 반환한다.
	return Cast<USkeletalMeshComponent>(FindComponentByName("SkeletalMeshComponent"));
}

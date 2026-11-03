#pragma once

#include "Components/MeshComponent.h"
#include "Render/SkeletalMeshData.h"
#include "SkinnedMeshComponent.generated.h"

UCLASS()
class USkinnedMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	int32 GetNumBones() const { return BoneLocalTransforms.Num(); }
	int32 FindBoneIndex(const FString& BoneName) const;
	int32 GetParentBoneIndex(int32 BoneIndex) const;

	// Local은 부모 본 기준, Component는 메시 기준, World는 월드 기준이다.
	FTransform GetBoneLocalTransform(int32 BoneIndex) const;
	FMatrix GetBoneComponentMatrix(int32 BoneIndex) const;
	FMatrix GetBoneWorldMatrix(int32 BoneIndex) const;
	// 바인드 역행렬을 결합하기 전의 Component Space 본 행렬 배열이다.
	// CPU 스키닝에서 사용하며, 반환값은 복사본이 아닌 내부 캐시 참조다.
	const TArray<FMatrix>& GetBoneComponentMatrices() const;

	// 속성 패널은 Local, 월드 기준 Gizmo는 World 편집 API를 사용한다.
	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform);
	void SetBoneWorldTransform(int32 BoneIndex, const FMatrix& NewWorldMatrix);
	void ResetToReferencePose();
	void RefreshBoneTransforms() const;
	bool IsPoseDirty() const { return bPoseDirty; }

protected:
	// 파생 컴포넌트가 가진 공유 리소스를 읽어 본 계산에 사용한다.
	virtual const FSkeletalMeshData* GetSkinnedMeshData() const = 0;
	void InitializePose();
	void MarkPoseDirty();

	// CPU 스키닝 측이 새 자세를 반영한 뒤 해제한다. 계층 캐시 갱신과는 구분한다.
	bool bPoseDirty = false;

private:
	// 공유 리소스의 기준 포즈를 복사한 이 컴포넌트만의 현재 자세.
	TArray<FTransform> BoneLocalTransforms;
	// 조회 시 필요한 경우에만 계층 행렬을 갱신한다.
	mutable TArray<FMatrix> BoneComponentMatrices;
	mutable bool bBoneTransformsDirty = false;
};

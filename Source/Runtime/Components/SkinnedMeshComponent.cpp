#include "EnginePCH.h"
#include "SkinnedMeshComponent.h"

int32 USkinnedMeshComponent::FindBoneIndex(const FString& BoneName) const
{
	const FSkeletalMeshData* Data = GetSkinnedMeshData();
	if (!Data) return INDEX_NONE;
	const FName Name(BoneName);
	const auto& BoneInfos = Data->RefSkeleton.RawRefBoneInfo;
	for (int32 BoneIndex = 0; BoneIndex < BoneInfos.Num(); ++BoneIndex)
		if (BoneInfos[BoneIndex].Name == Name) return BoneIndex;
	return INDEX_NONE;
}

int32 USkinnedMeshComponent::GetParentBoneIndex(int32 BoneIndex) const
{
	return BoneLocalTransforms.IsValidIndex(BoneIndex)
		? GetSkinnedMeshData()->RefSkeleton.RawRefBoneInfo[BoneIndex].ParentIndex : INDEX_NONE;
}

FTransform USkinnedMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
	return BoneLocalTransforms.IsValidIndex(BoneIndex)
		? BoneLocalTransforms[BoneIndex] : FTransform::Identity;
}

FMatrix USkinnedMeshComponent::GetBoneComponentMatrix(int32 BoneIndex) const
{
	if (!BoneLocalTransforms.IsValidIndex(BoneIndex)) return FMatrix::Identity;
	RefreshBoneTransforms();
	// 부모 본의 변환까지 누적했지만 컴포넌트 자체의 World 변환은 포함하지 않는다.
	return BoneComponentMatrices[BoneIndex];
}

FMatrix USkinnedMeshComponent::GetBoneWorldMatrix(int32 BoneIndex) const
{
	if (!BoneLocalTransforms.IsValidIndex(BoneIndex)) return FMatrix::Identity;
	RefreshBoneTransforms();
	// Gizmo 등에 쓰는 본의 월드 행렬이다. 정점을 변형하는 스키닝 행렬과는 다르다.
	return BoneComponentMatrices[BoneIndex] * GetWorldMatrix();
}

const TArray<FMatrix>& USkinnedMeshComponent::GetBoneComponentMatrices() const
{
	RefreshBoneTransforms();
	return BoneComponentMatrices;
}

// 부모 본을 기준으로 편집한 자세를 저장하고, 본 행렬과 스키닝 결과를 다시 계산하도록 표시한다.
void USkinnedMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	if (!BoneLocalTransforms.IsValidIndex(BoneIndex)) return;
	BoneLocalTransforms[BoneIndex] = NewTransform;
	MarkPoseDirty();
}

// Gizmo에서 받은 월드 행렬을 부모 본 기준의 위치·회전·크기로 바꿔 저장한다.
void USkinnedMeshComponent::SetBoneWorldTransform(int32 BoneIndex, const FMatrix& NewWorldMatrix)
{
	if (!BoneLocalTransforms.IsValidIndex(BoneIndex)) return;
	// 부모가 직전에 편집됐을 수 있으므로 최신 계층 행렬로 역변환한다.
	RefreshBoneTransforms();

	// 행벡터 규약: World -> Component -> 부모 본 기준 Local 순서로 되돌린다.
	FMatrix LocalMatrix = NewWorldMatrix * GetWorldMatrix().Inverse();
	const int32 ParentIndex = GetSkinnedMeshData()->RefSkeleton.RawRefBoneInfo[BoneIndex].ParentIndex;
	if (ParentIndex != INDEX_NONE)
		LocalMatrix *= BoneComponentMatrices[ParentIndex].Inverse();

	FVector XAxis(LocalMatrix[0][0], LocalMatrix[0][1], LocalMatrix[0][2]);
	const FVector YAxis(LocalMatrix[1][0], LocalMatrix[1][1], LocalMatrix[1][2]);
	const FVector ZAxis(LocalMatrix[2][0], LocalMatrix[2][1], LocalMatrix[2][2]);
	FTransform LocalTransform;
	LocalTransform.Location = FVector(LocalMatrix[3][0], LocalMatrix[3][1], LocalMatrix[3][2]);
	LocalTransform.Scale = FVector(XAxis.Size(), YAxis.Size(), ZAxis.Size());
	// 축 길이에서 사라진 반사 부호를 X 스케일에 남긴다.
	if (LocalMatrix.Determinant() < 0.0f)
	{
		LocalTransform.Scale.X *= -1.0f;
		XAxis *= -1.0f;
	}

	// 기존 SceneComponent와 같은 TRS 분해 방식이며 Shear는 보존하지 않는다.
	FMatrix Rotation = FMatrix::Identity;
	Rotation.SetAxis(0, XAxis.Normalized());
	Rotation.SetAxis(1, YAxis.Normalized());
	Rotation.SetAxis(2, ZAxis.Normalized());
	LocalTransform.Rotation = MatrixToRotator(Rotation);
	// Pitch가 +/-90도면 Yaw와 Roll을 따로 구할 수 없어 Roll=0인 동등한 회전으로 표현한다.
	const float CosPitchSquared = Rotation[0][0] * Rotation[0][0] + Rotation[0][1] * Rotation[0][1];
	if (CosPitchSquared <= SMALL_NUMBER)
	{
		LocalTransform.Rotation.Yaw = FMath::RadiansToDegrees(std::atan2(-Rotation[1][0], Rotation[1][1]));
		LocalTransform.Rotation.Roll = 0.0f;
	}
	BoneLocalTransforms[BoneIndex] = LocalTransform;
	MarkPoseDirty();
}

// 리소스의 기준 자세를 복사해 이 컴포넌트가 편집할 본 자세를 준비한다.
void USkinnedMeshComponent::InitializePose()
{
	BoneLocalTransforms.Reset();
	BoneComponentMatrices.Reset();
	if (const FSkeletalMeshData* Data = GetSkinnedMeshData())
	{
		// 루트의 바인드 스케일까지 포함한 기준 로컬 포즈를 그대로 복사한다.
		BoneLocalTransforms = Data->RefSkeleton.RawRefBonePose;
		BoneComponentMatrices.SetNum(BoneLocalTransforms.Num());
	}
	MarkPoseDirty();
}

void USkinnedMeshComponent::ResetToReferencePose()
{
	InitializePose();
}

// 본 행렬 갱신과 정점 스키닝은 각각 필요할 때 수행하므로 두 상태를 함께 표시한다.
void USkinnedMeshComponent::MarkPoseDirty()
{
	bBoneTransformsDirty = true;
	bPoseDirty = true;
}

// 부모부터 변환을 누적해, 메시를 기준으로 한 각 본의 현재 행렬을 구한다.
void USkinnedMeshComponent::RefreshBoneTransforms() const
{
	if (!bBoneTransformsDirty) return;
	const FSkeletalMeshData* Data = GetSkinnedMeshData();
	// 리소스의 부모 우선 순서를 이용한다. 부모 수정 시 자식도 함께 갱신된다.
	for (int32 BoneIndex = 0; BoneIndex < BoneLocalTransforms.Num(); ++BoneIndex)
	{
		const FMatrix Local = BoneLocalTransforms[BoneIndex].GetLocalMatrix();
		const int32 ParentIndex = Data->RefSkeleton.RawRefBoneInfo[BoneIndex].ParentIndex;
		// 루트는 Local 그대로, 자식은 Local * 부모 Component 행렬로 누적한다.
		BoneComponentMatrices[BoneIndex] = ParentIndex == INDEX_NONE
			? Local : Local * BoneComponentMatrices[ParentIndex];
	}
	// 계층 조회가 끝나도 CPU 스키닝에 전달할 bPoseDirty는 유지한다.
	bBoneTransformsDirty = false;
}

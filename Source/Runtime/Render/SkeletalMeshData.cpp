#include "EnginePCH.h"
#include "SkeletalMeshData.h"

#include <cmath>
#include <format>
#include <limits>

void FSkeletalMeshData::UpdateReferenceBounds()
{
	ReferenceBounds = {};
	if (ReferenceVertices.IsEmpty()) return;
	// 원점이 Bounds에 섞이지 않도록 첫 정점에서 시작한다.
	ReferenceBounds.Min = ReferenceBounds.Max = ReferenceVertices[0].Position;
	for (const FVertex& Vertex : ReferenceVertices)
	{
		ReferenceBounds.Min = FMath::Min(ReferenceBounds.Min, Vertex.Position);
		ReferenceBounds.Max = FMath::Max(ReferenceBounds.Max, Vertex.Position);
	}
}

bool FSkeletalMeshData::Validate(FString& OutError) const
{
	OutError.clear();
	const auto Fail = [&OutError](FString Error)
	{
		OutError = std::move(Error);
		return false;
	};

	if (ReferenceVertices.IsEmpty() || Indices.IsEmpty() || Indices.Num() % 3 != 0)
		return Fail("skeletal mesh requires vertices and triangle indices");
	if (VertexInfluences.Num() != ReferenceVertices.Num())
		return Fail("vertex influence count does not match final render vertex count");
	if (Bones.IsEmpty() || static_cast<uint32>(Bones.Num()) >
		static_cast<uint32>(std::numeric_limits<uint16>::max()) + 1u)
		return Fail("bone count must be between 1 and 65536 (uint16 bone indices)");

	// 부모부터 계산할 수 있게 본 순서를 확인한다. 자기 참조와 순환도 차단된다.
	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		const FSkeletalBone& Bone = Bones[BoneIndex];
		if ((BoneIndex == 0 && Bone.ParentIndex != INDEX_NONE) ||
			(BoneIndex > 0 && (Bone.ParentIndex < 0 || Bone.ParentIndex >= BoneIndex)))
			return Fail(std::format("bone {} requires a preceding parent; only bone 0 may be root", BoneIndex));
	}

	for (int32 VertexIndex = 0; VertexIndex < ReferenceVertices.Num(); ++VertexIndex)
	{
		const FSkeletalVertexInfluences& Influences = VertexInfluences[VertexIndex];
		float WeightSum = 0.0f;
		for (uint32 InfluenceIndex = 0; InfluenceIndex < MaxSkeletalMeshInfluences; ++InfluenceIndex)
		{
			const float Weight = Influences.Weights[InfluenceIndex];
			if (!std::isfinite(Weight) || Weight < 0.0f)
				return Fail(std::format("vertex {} has a negative or non-finite weight", VertexIndex));
			// 미사용 슬롯은 BoneIndex를 참조하지 않는다.
			if (Weight == 0.0f) continue;
			const uint16 BoneIndex = Influences.BoneIndices[InfluenceIndex];
			if (static_cast<int32>(BoneIndex) >= Bones.Num())
				return Fail(std::format("vertex {} references invalid bone {}", VertexIndex, BoneIndex));
			WeightSum += Weight;
		}
		// 가중합이 원래 크기와 이동량을 유지하려면 Weight 합이 1이어야 한다.
		if (std::abs(WeightSum - 1.0f) > SkeletalMeshWeightTolerance)
			return Fail(std::format("vertex {} weights must sum to 1 (got {})", VertexIndex, WeightSum));
	}

	for (uint32 Index : Indices)
		if (Index >= static_cast<uint32>(ReferenceVertices.Num()))
			return Fail(std::format("triangle index {} exceeds vertex count {}", Index, ReferenceVertices.Num()));

	if (Sections.IsEmpty())
		return Fail("skeletal mesh requires at least one section");
	const uint32 IndexCount = static_cast<uint32>(Indices.Num());
	for (const FMeshSection& Section : Sections)
	{
		// 뺄셈으로 검사하여 StartIndex + IndexCount의 overflow도 피한다.
		if (Section.StartIndex > IndexCount || Section.IndexCount > IndexCount - Section.StartIndex)
			return Fail("section range exceeds the index buffer");
		if (Section.IndexCount == 0 || Section.IndexCount % 3 != 0)
			return Fail("section requires triangle indices");
		if (Section.MaterialSlotIndex >= static_cast<uint32>(MaterialSlots.Num()))
			return Fail("section references an invalid material slot");
	}
	return true;
}

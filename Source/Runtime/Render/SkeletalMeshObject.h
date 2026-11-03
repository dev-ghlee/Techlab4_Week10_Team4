#pragma once

#include "Render/MeshGeometry.h"
#include "Render/SkeletalMeshData.h"

// 컴포넌트별 CPU 스키닝 결과와 동적 VB. 공유 MeshData와 IB는 USkeletalMesh가 소유한다.
// 리소스는 이 객체보다 오래 살아야 한다.
class FSkeletalMeshObject
{
public:
	FSkeletalMeshObject(const FSkeletalMeshData& InMeshData, FIndexBuffer* InIndexBuffer);
	~FSkeletalMeshObject();

	// 본 행렬을 즉시 스키닝 행렬로 변환하므로 컴포넌트의 자세 배열을 보관하지 않는다.
	bool UpdateDynamicData(const TArray<FMatrix>& BoneComponentMatrices);
	bool UploadSkinnedVertices();

	const TArray<FVertex>& GetSkinnedVertices() const { return SkinnedVertices; }
	const FBox& GetLocalBounds() const { return LocalBounds; }
	FMeshGeometry GetGeometry() const;

private:
	bool InitializeResources();
	void BuildSkinningMatrices(const TArray<FMatrix>& BoneComponentMatrices);
	void SkinVerticesCPU();
	void UpdateLocalBounds();

	const FSkeletalMeshData& MeshData;
	TArray<FMatrix> SkinningMatrices;
	TArray<FVertex> SkinnedVertices;
	FBox LocalBounds;
	TUniquePtr<FVertexBuffer> DynamicVertexBuffer;
	FIndexBuffer* IndexBuffer = nullptr;
	bool bVertexBufferDirty = true;
	const uint16 SortID;
	static inline uint16 NextSortID = 0;
};

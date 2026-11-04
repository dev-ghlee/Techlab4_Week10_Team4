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
	// DirtyBones는 자손까지 포함한 마스크. 생략하거나 첫 갱신이면 전체를 계산한다.
	bool UpdateDynamicData(const TArray<FMatrix>& BoneComponentMatrices, const TArray<uint8>* DirtyBones = nullptr);
	bool UploadSkinnedVertices();

	const TArray<FVertex>& GetSkinnedVertices() const { return SkinnedVertices; }
	const FBox& GetLocalBounds() const { return LocalBounds; }
	FMeshGeometry GetGeometry() const;
	int32 GetLastSkinnedVertexCount() const { return LastSkinnedVertexCount; }

private:
	bool InitializeResources();
	void BuildSkinningMatrices(const TArray<FMatrix>& BoneComponentMatrices, const TArray<uint8>* DirtyBones);
	void SkinVerticesCPU(const TArray<uint8>* DirtyBones);
	void SkinVertexCPU(int32 VertexIndex);
	void UpdateLocalBounds();

	const FSkeletalMeshData& MeshData;
	TArray<FMatrix> SkinningMatrices;
	TArray<FVertex> SkinnedVertices;
	// 변경 본에서 정점으로 바로 접근하고, 여러 본에 걸친 정점은 한 번만 계산한다.
	TArray<TArray<int32>> BoneVertices;
	TArray<uint8> DirtyVertices;
	TArray<int32> DirtyVertexIndices;
	int32 LastSkinnedVertexCount = 0;
	bool bSkinningInitialized = false;
	FBox LocalBounds;
	TUniquePtr<FVertexBuffer> DynamicVertexBuffer;
	FIndexBuffer* IndexBuffer = nullptr;
	bool bVertexBufferDirty = true;
	const uint16 SortID;
	static inline uint16 NextSortID = 0;
};

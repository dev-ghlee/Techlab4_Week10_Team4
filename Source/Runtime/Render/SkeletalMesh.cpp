#include "EnginePCH.h"
#include "Render/SkeletalMesh.h"
#include "Asset/AssetManager.h"
#include "Render/Material.h"
#include <utility>

bool USkeletalMesh::Initialize(FSkeletalMeshData&& InMeshData, TArray<FSkeletalMaterial>&& InMaterials, TUniquePtr<FIndexBuffer> InIndexBuffer, FString& OutError)
{
	OutError.clear();

	// 공유 리소스는 등록 전에 한 번만 초기화한다.
	if (IndexBuffer)
	{
		OutError = "Skeletal mesh is already initialized.";
		return false;
	}

	// 상세한 원본 구조 검사는 Importer가 수행한다.
	if (InMeshData.ReferenceVertices.IsEmpty()
		|| InMeshData.Indices.IsEmpty() || InMeshData.Sections.IsEmpty())
	{
		OutError = "Skeletal mesh has no renderable geometry.";
		return false;
	}

	// 래퍼 객체가 존재하더라도 실제 GPU 버퍼 생성은 실패했을 수 있다.
	if (!InIndexBuffer || !InIndexBuffer->GetBuffer())
	{
		OutError = "Skeletal mesh requires a valid GPU index buffer.";
		return false;
	}

	const DXGI_FORMAT IndexFormat = InIndexBuffer->GetFormat();
	if (IndexFormat != DXGI_FORMAT_R16_UINT && IndexFormat != DXGI_FORMAT_R32_UINT)
	{
		OutError = "Unsupported skeletal mesh index buffer format.";
		return false;
	}

	// 기존 생성 함수가 선택한 인덱스 형식에 맞춰 바이트 크기를 검사한다.
	const uint32 IndexStride = IndexFormat == DXGI_FORMAT_R16_UINT ? 2u : 4u;
	const uint64 ExpectedBufferSize = static_cast<uint64>(InMeshData.Indices.Num()) * IndexStride;
	if (InIndexBuffer->GetIndexCount() != static_cast<uint32>(InMeshData.Indices.Num())
		|| InIndexBuffer->GetBufferSize() != ExpectedBufferSize)
	{
		OutError = "Index buffer does not match skeletal mesh index data.";
		return false;
	}

	if (InMaterials.IsEmpty() || InMaterials.Num() != InMeshData.MaterialSlots.Num())
	{
		OutError = "Runtime material count does not match imported material slots.";
		return false;
	}

	for (const FSkeletalMaterial& Material : InMaterials)
	{
		if (!Material.MaterialInterface)
		{
			OutError = "Skeletal mesh contains an invalid runtime material.";
			return false;
		}
	}

	// 모든 검사를 통과한 뒤에만 공유 데이터와 버퍼를 내부로 이동한다.
	MeshData = std::move(InMeshData);
	Materials = std::move(InMaterials);
	IndexBuffer = std::move(InIndexBuffer);
	return true;
}

UMaterial* USkeletalMesh::GetMaterial(uint32 SlotIndex) const
{
	if (SlotIndex < static_cast<uint32>(Materials.Num()))
	{
		UMaterial* Material = Materials[SlotIndex].MaterialInterface;
		if (Material)
		{
			return Material;
		}
	}

	return UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
}

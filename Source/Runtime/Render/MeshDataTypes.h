#pragma once

#include "Core/EngineString.h"
#include "Core/Types.h"
#include "Math/Vector4.h"

// StaticMesh와 SkeletalMesh가 공유하는 삼각형 인덱스 범위.
struct FMeshSection
{
	uint32 StartIndex = 0;
	uint32 IndexCount = 0;
	uint32 MaterialSlotIndex = 0;
};

// GPU 객체가 아닌 임포트 결과. 경로는 UAssetManager::LoadTexture에 전달한다.
struct FMeshMaterialSlot
{
	FString Name;
	FVector4 BaseColor = FVector4(1, 1, 1, 1);
	FString DiffuseTexturePath;
	FString NormalTexturePath{};
};

#pragma once

#include "Containers/Array.h"
#include "Core/NameTypes.h"
#include "Core/Types.h"
#include "Render/ReferenceSkeleton.h"
#include "Math/Box.h"
#include "Math/Matrix.h"
#include "Render/StaticMeshData.h"
#include "Render/Vertex.h"

class UMaterial;

// 현재 구현에서 정점 하나에 허용할 최대 본 영향 개수.
inline constexpr uint32 MaxBoneInfluences = 4;

// 렌더링 정점 하나의 스키닝 정보.
struct FSkinWeightInfo
{
	// 전체 RawRefBoneInfo 배열의 본 인덱스.
	// Weight가 0인 미사용 슬롯의 인덱스는 무시한다.
	int32 InfluenceBones[MaxBoneInfluences] = {};

	// 같은 위치의 InfluenceBones에 대응하는 영향력.
	// 유효한 영향들의 합은 1이며, 영향이 없으면 모두 0이다.
	float InfluenceWeights[MaxBoneInfluences] = {};
};

// 하나의 재질로 그릴 삼각형들의 연속된 인덱스 구간.
struct FSkelMeshRenderSection
{
	// 전체 인덱스 배열에서 이 Section이 시작하는 위치.
	uint32 BaseIndex = 0;

	// 그릴 인덱스 개수는 NumTriangles * 3이다.
	uint32 NumTriangles = 0;

	// USkeletalMesh의 Materials 배열을 참조하는 인덱스.
	uint16 MaterialIndex = 0;
};

// 실제 재질과 슬롯 이름을 연결하는 런타임 재질 슬롯.
struct FSkeletalMaterial
{
	// 현재 렌더러가 사용하는 구체적인 재질 객체를 참조한다.
	UMaterial* MaterialInterface = nullptr;

	// Viewer 표시와 슬롯 식별에 사용하는 이름.
	FName MaterialSlotName;
};

// 임포트가 완료된 SkeletalMesh의 공유 원본 데이터.
// 컴포넌트별 현재 포즈와 변형된 정점은 여기에 저장하지 않는다.
// 임포트 결과의 규약: X-Forward / Y-Right / Z-Up, 길이 단위는 cm.
// 행벡터 방식으로 본 전역 = 로컬 * 부모 전역,
// 스키닝 행렬 = 역바인드 * 현재 본 전역 순서로 계산한다.
struct FSkeletalMeshData
{
	// 본 계층과 기준 로컬 포즈.
	FReferenceSkeleton RefSkeleton;

	// 기준 포즈의 본별 메시 공간 전역 행렬을 뒤집은 행렬.
	// 메시 공간의 정점을 바인드 당시 본 공간으로 변환한다.
	// RawRefBoneInfo와 개수 및 순서가 같아야 한다.
	TArray<FMatrix> RefBasesInvMatrix;

	// 공통 SkeletalMesh 로컬 공간의 기준 포즈 정점.
	TArray<FVertex> ReferenceVertices;

	// 전체 ReferenceVertices 배열을 직접 참조하는 삼각형 인덱스.
	TArray<uint32> Indices;

	// ReferenceVertices와 개수 및 순서가 같은 정점별 본 영향 정보.
	TArray<FSkinWeightInfo> SkinWeights;

	// 재질별로 구분한 인덱스 구간.
	TArray<FSkelMeshRenderSection> Sections;

	// 실제 재질 생성에 필요한 이름, 색상, 텍스처 경로.
	// Section.MaterialIndex와 최종 Materials의 슬롯 순서를 유지한다.
	TArray<FStaticMaterialSlot> MaterialSlots;

	// Importer가 최종 원본 정점으로 계산할 메시 로컬 공간 경계.
	FBox ReferenceBounds{};
};

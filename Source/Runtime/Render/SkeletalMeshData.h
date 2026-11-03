#pragma once

#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Math/Transform.h"
#include "Math/Box.h"
#include "Render/MeshDataTypes.h"
#include "Render/Vertex.h"

inline constexpr uint32 MaxSkeletalMeshInfluences = 4;
inline constexpr float SkeletalMeshWeightTolerance = 1.e-4f;

struct FSkeletalBone
{
	// 리소스 안에서 유일한 이름. 중복 FBX 이름은 임포터에서 경로 등으로 구분한다.
	FString Name;
	int32 ParentIndex = INDEX_NONE;

	// 이 규약의 기준 포즈는 바인드 포즈다. 부모가 없는 0번 본이 유일한 루트다.
	FTransform ReferenceLocalTransform;

	// 공통 메시 로컬 공간 -> 바인드 당시 본 로컬 공간. World 변환은 포함하지 않는다.
	// 스키닝 행렬은 InverseBindMatrix * 현재 본의 Component 행렬로 계산한다.
	FMatrix InverseBindMatrix = FMatrix::Identity;
};

struct FSkeletalVertexInfluences
{
	// 리소스 전체 Bones 배열의 인덱스. Section 내부의 본 인덱스가 아니다.
	uint16 BoneIndices[MaxSkeletalMeshInfluences]{};
	// 양수 Weight만 활성 영향이다. 미사용 슬롯은 Weight 0, 합은 1이어야 한다.
	float Weights[MaxSkeletalMeshInfluences]{};
};

// A(FBX 임포트) -> B(CPU 스키닝) 공용 CPU 데이터. SDK/GPU 객체를 보관하지 않는다.
// 등록 후 공유 원본으로 취급하고 현재 자세/출력 정점은 컴포넌트별로 따로 보관한다.
struct FSkeletalMeshData
{
	// 바인드 포즈의 원본 정점. CPU 스키닝 결과는 별도 배열에 기록한다.
	TArray<FVertex> ReferenceVertices;
	TArray<uint32> Indices;
	TArray<FMeshSection> Sections;
	TArray<FMeshMaterialSlot> MaterialSlots;
	TArray<FSkeletalBone> Bones;
	// ReferenceVertices와 같은 인덱스를 사용한다. UV 경계로 분리된 정점도 각각 가진다.
	TArray<FSkeletalVertexInfluences> VertexInfluences;
	FBox ReferenceBounds{};

	// 최종 정점을 확정한 뒤 임포터에서 호출한다. 빈 데이터면 원점의 빈 Bounds.
	void UpdateReferenceBounds();

	// 배열 크기/인덱스와 스키닝에 필요한 Weight만 검사한다. 원본 속성은 임포터가 준비한다.
	// 보정 없이 실패 이유를 반환한다. 성공 시 OutError는 빈 문자열.
	bool Validate(FString& OutError) const;
};

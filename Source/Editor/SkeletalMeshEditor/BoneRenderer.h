#pragma once

#include "Render/Buffer.h"
#include "Render/PipelineState.h"

struct FSkeletalPose;
class UStaticMesh;
class FShaderProgram;
class FRenderer;

class FBoneRenderer
{
public:
	bool Init(FRenderer* InRenderer);

	void OnRender(const FSkeletalPose& Pose, int32 SelectedBoneIndex, const FMatrix& ViewProjection, const FVector& CameraLocation);
	void DrawBones(const FSkeletalPose& Pose, const TArray<float>& JointRadii, int32 SelectedBoneIndex, const FMatrix& ViewProjection, const FVector& CameraLocation, float Alpha);

	FRenderer* Renderer = nullptr;

	FShaderProgram* Shader = nullptr;
	FPipelineState PipelineState;
	FPipelineState OccludedPipelineState;
	UStaticMesh* BoneSphereMesh = nullptr;
	UStaticMesh* BoneOctahedronMesh = nullptr;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
};

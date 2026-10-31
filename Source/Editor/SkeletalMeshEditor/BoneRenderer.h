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

private:
	FRenderer* Renderer = nullptr;

	FShaderProgram* Shader = nullptr;
	FPipelineState PipelineState;
	UStaticMesh* BoneSphereMesh = nullptr;
	UStaticMesh* BoneOctahedronMesh = nullptr;
	TUniquePtr<FConstantBuffer> ConstantBuffer;

	TArray<float> JointRadii;
};

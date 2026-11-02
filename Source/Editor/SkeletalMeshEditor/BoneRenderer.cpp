#include "EnginePCH.h"
#include "BoneRenderer.h"

#include "SkeletalTemp.h"
#include "Asset/AssetManager.h"

namespace
{
	FVector4 GetDisplayColor(bool Selected)
	{
		if (Selected)
		{
			return { 0.87f, 0.56f, 0.13f, 1.0f };
		}
		return { 0.3f, 0.31f, 0.41f, 1.0f };
	}

	constexpr float JointRadiusRatio = 0.07f;
	constexpr float DefaultJointRadius = 0.05f;

	struct FConstantData
	{
		FMatrix World;
		FMatrix ViewProjection;
		FVector4 Color;
		FVector CameraLocation;
		float Padding;
	};
	static_assert(sizeof(FConstantData) % 16 == 0);
}

bool FBoneRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;

	BoneSphereMesh = UAssetManager::GetAssetByPath<UStaticMesh>("BoneSphere");
	BoneOctahedronMesh = UAssetManager::GetAssetByPath<UStaticMesh>("BoneOctahedron");

	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FConstantData));

	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/BoneShader.hlsl");

	PipelineState.Shader = Shader;
	PipelineState.Topology = EPrimitiveTopology::TriangleList;

	return true;
}

void FBoneRenderer::OnRender(const FSkeletalPose& Pose, int32 SelectedBoneIndex, const FMatrix& ViewProjection, const FVector& CameraLocation)
{
	if (!Shader || !BoneSphereMesh || !BoneOctahedronMesh)
	{
		return;
	}

	RenderCommand::BindPipelineState(PipelineState);

	JointRadii.SetNum(Pose.LocalPose.Num());
	for (float& Radius : JointRadii)
	{
		Radius = FLT_MAX;
	}

	for (int32 Index = 0; Index < Pose.LocalPose.Num(); ++Index)
	{
		const int32 ParentIndex = Pose.Skeleton->RefBoneInfo[Index].ParentIndex;
		if (ParentIndex < 0)
		{
			continue;
		}
		
		const FVector Delta = Pose.ComponentSpacePose[Index].GetOrigin() - Pose.ComponentSpacePose[ParentIndex].GetOrigin();
		const float Radius = Delta.Length() * JointRadiusRatio;
		if (Radius < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		JointRadii[Index] = std::min(JointRadii[Index], Radius);
		JointRadii[ParentIndex] = std::min(JointRadii[ParentIndex], Radius);
	}

	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	// Joint
	RenderCommand::BindMesh(BoneSphereMesh);
	for (int32 Index = 0; Index < Pose.LocalPose.Num(); ++Index)
	{
		const FVector Location = Pose.ComponentSpacePose[Index].GetOrigin();
		const float Radius = JointRadii[Index] < FLT_MAX ? JointRadii[Index] : DefaultJointRadius;

		const FMatrix World{
			{ Radius, 0.0f, 0.0f, 0.0f },
			{ 0.0f, Radius, 0.0f, 0.0f },
			{ 0.0f, 0.0f, Radius, 0.0f },
			{ Location.X, Location.Y, Location.Z, 1.0f }
		};

		const FConstantData Data{
			.World = World,
			.ViewProjection = ViewProjection,
			.Color = GetDisplayColor(Index == SelectedBoneIndex),
			.CameraLocation = CameraLocation,
		};

		RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Data, sizeof(Data));
		RenderCommand::DrawIndexed(BoneSphereMesh->IndexBuffer->GetIndexCount());
	}

	// Bone
	RenderCommand::BindMesh(BoneOctahedronMesh);
	for (int32 Index = 0; Index < Pose.LocalPose.Num(); ++Index)
	{
		const int32 ParentIndex = Pose.Skeleton->RefBoneInfo[Index].ParentIndex;
		if (ParentIndex < 0)
		{
			continue;
		}

		const FMatrix& Parent = Pose.ComponentSpacePose[ParentIndex];
		const FVector ParentLocation = Parent.GetOrigin();
		const FVector Delta = Pose.ComponentSpacePose[Index].GetOrigin() - ParentLocation;
		const float Length = Delta.Length();
		if (Length < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float ParentRadius = JointRadii[ParentIndex] < FLT_MAX ? JointRadii[ParentIndex] : DefaultJointRadius;
		const float ChildRadius = JointRadii[Index] < FLT_MAX ? JointRadii[Index] : DefaultJointRadius;
		const float DrawLength = Length - (ParentRadius + ChildRadius);
		if (DrawLength < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector X = Delta / Length;
		const FVector Start = ParentLocation + X * ParentRadius;

		FVector ParentAxis = FVector{ Parent[1][0], Parent[1][1], Parent[1][2] }.Normalized();
		if (std::abs(ParentAxis | X) > 0.99f)
		{
			ParentAxis = FVector{ Parent[2][0], Parent[2][1], Parent[2][2] }.Normalized();
		}

		const FVector Y = (ParentAxis - X * (ParentAxis | X)).GetSafeNormal();
		const FVector Z = X ^ Y;

		const FMatrix World{
			{ DrawLength * X.X, DrawLength * X.Y, DrawLength * X.Z, 0.0f },
			{ DrawLength * Y.X, DrawLength * Y.Y, DrawLength * Y.Z, 0.0f },
			{ DrawLength * Z.X, DrawLength * Z.Y, DrawLength * Z.Z, 0.0f },
			{ Start.X, Start.Y, Start.Z, 1.0f }
		};

		const FConstantData Data{
			.World = World,
			.ViewProjection = ViewProjection,
			.Color = GetDisplayColor(ParentIndex == SelectedBoneIndex),
			.CameraLocation = CameraLocation,
		};

		RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Data, sizeof(Data));
		RenderCommand::DrawIndexed(BoneOctahedronMesh->IndexBuffer->GetIndexCount());
	}
}

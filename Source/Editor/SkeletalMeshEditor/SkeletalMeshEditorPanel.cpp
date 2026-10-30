#include "EnginePCH.h"
#include "SkeletalMeshEditorPanel.h"

#include "Engine/World.h"

bool FSkeletalMeshEditorPanel::Init()
{
	PreviewWorld = UWorld::CreateWorld(EWorldType::Editor, false, "SkeletalMeshPreview");
	ViewportClient.SetWorld(PreviewWorld);
	ViewportClient.SetViewLocation(FVector{ -4.0f, 0.0f, 2.0f });
	ViewportClient.SetViewRotation(FVector{ 20.0f, 0.0f, 0.0f });

	BuildDummySkeleton();

	return true;
}

void FSkeletalMeshEditorPanel::Tick(float DeltaTime)
{
	UpdateComponentSpacePose();
}

void FSkeletalMeshEditorPanel::OnRender()
{
	if (ImGui::Begin(GetPanelName(), nullptr))
	{
		const ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
		const uint32 Width = static_cast<uint32>(std::max(1.0f, ViewportSize.x));
		const uint32 Height = static_cast<uint32>(std::max(1.0f, ViewportSize.y));
		if (Width != ViewportClient.GetWidth() || Height != ViewportClient.GetHeight())
		{
			ViewportClient.Resize(Width, Height);
		}

		if (ViewportClient.GetColorTarget() && ViewportClient.GetColorTarget()->GetSRV())
		{
			ImGui::Image(ViewportClient.GetColorTarget()->GetSRV(), ImVec2(static_cast<float>(Width), static_cast<float>(Height)));
		}
	}

	ImGui::End();
}

UWorld* FSkeletalMeshEditorPanel::ReleasePreviewWorld()
{
	ViewportClient.SetWorld(nullptr);
	UWorld* World = PreviewWorld;
	PreviewWorld = nullptr;
	return World;
}

void FSkeletalMeshEditorPanel::BuildDummySkeleton()
{
	ReferenceSkeleton = {};
	ComponentSpacePose.Reset();

	ReferenceSkeleton.RefBoneInfo.Emplace("Root", -1);
	ReferenceSkeleton.RefBonePoses.Emplace(FTransform::Identity);

	ReferenceSkeleton.RefBoneInfo.Emplace("Pelvis", 0);
	ReferenceSkeleton.RefBonePoses.Emplace(FRotator::Identity, FVector{ 0.0f, 0.0f, 1.0f });

	ReferenceSkeleton.RefBoneInfo.Emplace("Spine", 1);
	ReferenceSkeleton.RefBonePoses.Emplace(FRotator::Identity, FVector{ 0.0f, 0.0f, 0.5f });

	ReferenceSkeleton.RefBoneInfo.Emplace("Head", 2);
	ReferenceSkeleton.RefBonePoses.Emplace(FRotator::Identity, FVector{ 0.0f, 0.0f, 0.5f });

	ReferenceSkeleton.RefBoneInfo.Emplace("LeftArm", 2);
	ReferenceSkeleton.RefBonePoses.Emplace(FRotator::Identity, FVector{ 0.0f, -0.3f, 0.4f });

	ReferenceSkeleton.RefBoneInfo.Emplace("LeftHand", 4);
	ReferenceSkeleton.RefBonePoses.Emplace(FRotator::Identity, FVector{ 0.0f, -0.5f, 0.0f });

	ReferenceSkeleton.RefBoneInfo.Emplace("RightArm", 2);
	ReferenceSkeleton.RefBonePoses.Emplace(FRotator::Identity, FVector{ 0.0f, 0.3f, 0.4f });

	ReferenceSkeleton.RefBoneInfo.Emplace("RightHand", 6);
	ReferenceSkeleton.RefBonePoses.Emplace(FRotator::Identity, FVector{ 0.0f, 0.5f, 0.0f });

	ResetPose();
}

void FSkeletalMeshEditorPanel::ResetPose()
{
	LocalPose = ReferenceSkeleton.RefBonePoses;
}

void FSkeletalMeshEditorPanel::UpdateComponentSpacePose()
{
	if (ComponentSpacePose.Num() != LocalPose.Num())
	{
		ComponentSpacePose.Reset();
		ComponentSpacePose.SetNum(LocalPose.Num());
	}

	for (int32 BoneIndex = 0; BoneIndex < LocalPose.Num(); ++BoneIndex)
	{
		const int32 ParentIndex = ReferenceSkeleton.RefBoneInfo[BoneIndex].ParentIndex;
		ComponentSpacePose[BoneIndex] = LocalPose[BoneIndex].GetLocalMatrix() *
			(ParentIndex >= 0 ? ComponentSpacePose[ParentIndex] : FMatrix::Identity);
	}
}

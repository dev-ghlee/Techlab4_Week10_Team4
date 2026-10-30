#include "EnginePCH.h"
#include "SkeletalMeshEditorPanel.h"

#include "Camera/ViewportCameraMovement.h"
#include "Editor/Settings/SettingsPanel.h"
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
	bool bPanelOpen = IsOpen();
	if (ImGui::Begin(GetPanelName(), &bPanelOpen))
	{
		SetOpen(bPanelOpen);

		if (ImGui::BeginTable("##SkeletalMeshEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Tree", ImGuiTableColumnFlags_WidthFixed, 250.0f);

			ImGui::TableNextColumn();
			
			const ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
			const uint32 Width = static_cast<uint32>(std::max(1.0f, ViewportSize.x));
			const uint32 Height = static_cast<uint32>(std::max(1.0f, ViewportSize.y));
			if (Width != ViewportClient.GetWidth() || Height != ViewportClient.GetHeight())
			{
				ViewportClient.Resize(Width, Height);
			}

			if (ViewportClient.GetColorTarget() && ViewportClient.GetColorTarget()->GetSRV())
			{
				const ImVec2 Min = ImGui::GetCursorScreenPos();
				const ImVec2 Size(static_cast<float>(Width), static_cast<float>(Height));
				ImGui::InvisibleButton("##PreviewViewport", Size, ImGuiButtonFlags_MouseButtonRight);
				ImGui::GetWindowDrawList()->AddImage(ViewportClient.GetColorTarget()->GetSRV(), Min, ImVec2(Min.x + Size.x, Min.y + Size.y));
				TickCameraInput();
			}

			ImGui::TableNextColumn();

			ImGui::SeparatorText("Skeleton Tree");

			if (ImGui::BeginChild("##SkeletonTree", ImVec2(0, 0), false))
			{
				DrawBoneTreeNode(0);
				
				ImGui::EndChild();
			}

			if (ImGui::IsWindowHovered() &&
				!ImGui::IsAnyItemHovered() &&
				ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				SelectedBoneIndex = -1;
			}

			ImGui::EndTable();
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

void FSkeletalMeshEditorPanel::TickCameraInput()
{
	if (!Settings) return;

	if (ImGui::IsItemActivated())
	{
		ImGui::SetWindowFocus();
	}

	const ImGuiIO& IO = ImGui::GetIO();
	const bool bCaptured = ImGui::IsItemActive();

	FViewportCameraInput Input{
		.bCaptured = bCaptured,
		.bAllowWheel = bCaptured || ImGui::IsItemHovered(),
		.bForward = ImGui::IsKeyDown(ImGuiKey_W),
		.bBackward = ImGui::IsKeyDown(ImGuiKey_S),
		.bRight = ImGui::IsKeyDown(ImGuiKey_D),
		.bLeft = ImGui::IsKeyDown(ImGuiKey_A),
		.bUp = ImGui::IsKeyDown(ImGuiKey_E),
		.bDown = ImGui::IsKeyDown(ImGuiKey_Q),
		.MouseX = static_cast<float>(IO.MouseDelta.x),
		.MouseY = static_cast<float>(IO.MouseDelta.y),
		.Wheel = static_cast<float>(IO.MouseWheel) * 120.0f,
	};

	ViewportClient.ApplyCameraInput(Input, IO.DeltaTime, 2.0f * Settings->CameraSpeed, 0.2f * Settings->MouseSensitivity);
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

	Children.SetNum(ReferenceSkeleton.RefBoneInfo.Num());
	for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.RefBoneInfo.Num(); ++BoneIndex)
	{
		const int32 ParentIndex = ReferenceSkeleton.RefBoneInfo[BoneIndex].ParentIndex;
		if (ParentIndex >= 0)
		{
			Children[ParentIndex].Add(BoneIndex);
		}
	}
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

void FSkeletalMeshEditorPanel::DrawBoneTreeNode(int32 BoneIndex)
{
	if (BoneIndex < 0 || BoneIndex >= ReferenceSkeleton.RefBoneInfo.Num())
		return;

	const auto& RefBoneInfo = ReferenceSkeleton.RefBoneInfo[BoneIndex];

	const bool bIsLeaf = Children[BoneIndex].Num() == 0;

	ImGuiTreeNodeFlags Flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;

	if (bIsLeaf)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	if (BoneIndex == SelectedBoneIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	const FString Name = RefBoneInfo.Name.ToString();
	ImGui::PushID(BoneIndex);
	bool bOpen = ImGui::TreeNodeEx("##Bone", Flags, "%s", Name.c_str());
	ImGui::PopID();

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
	{
		SelectedBoneIndex = BoneIndex;
	}
	
	if (bOpen && !bIsLeaf)
	{
		for (int32 ChildIndex : Children[BoneIndex])
		{
			DrawBoneTreeNode(ChildIndex);
		}

		ImGui::TreePop();
	}
}

void FSkeletalMeshEditorPanel::DrawBones(FLineBatcher& LineBatcher) const
{
	for (int32 BoneIndex = 0; BoneIndex < ComponentSpacePose.Num(); ++BoneIndex)
	{
		const int32 ParentIndex = ReferenceSkeleton.RefBoneInfo[BoneIndex].ParentIndex;
		if (ParentIndex < 0) continue;
		const FVector ParentLocation = ComponentSpacePose[ParentIndex].GetOrigin();
		const FVector4 Color = BoneIndex == SelectedBoneIndex ? FVector4{ 1.0f, 0.0f, 0.0f, 1.0f } : FVector4{ 1.0f, 1.0f, 1.0f, 1.0f };
		LineBatcher.AddLine(ParentLocation, ComponentSpacePose[BoneIndex].GetOrigin(), Color);
	}
}

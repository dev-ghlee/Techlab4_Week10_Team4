#include "EnginePCH.h"
#include "SkeletalMeshEditorPanel.h"

#include "Camera/ViewportCameraMovement.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Engine/World.h"

namespace
{
	constexpr ImU32 AxisColors[3] = {
		IM_COL32(165, 1, 10, 255),
		IM_COL32(13, 90, 6, 255),
		IM_COL32(3, 34, 234, 255)
	};

	bool DrawResetButton(const char* StrId)
	{
		const float Size = ImGui::GetFrameHeight();
		const bool bPressed = ImGui::InvisibleButton(StrId, ImVec2(Size, Size));

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const ImVec2 Center(Min.x + Size * 0.5f, Min.y + Size * 0.5f);

		if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		{
			const ImGuiCol BgColor = ImGui::IsItemActive() ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered;
			DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(BgColor), ImGui::GetStyle().FrameRounding);
		}

		const ImU32 Color = ImGui::GetColorU32(ImGuiCol_Text);
		const float Radius = Size * 0.28f;
		constexpr float Thickness = 1.5f;

		constexpr float AngleStart = -IM_PI * 0.5f;
		constexpr float AngleEnd = IM_PI;
		DrawList->PathArcTo(Center, Radius, AngleStart, AngleEnd);
		DrawList->PathStroke(Color, ImDrawFlags_None, Thickness);

		const ImVec2 Radial(cosf(AngleStart), sinf(AngleStart));
		const ImVec2 Tangent(sinf(AngleStart), -cosf(AngleStart));
		const ImVec2 P(Center.x + Radial.x * Radius, Center.y + Radial.y * Radius);
		const float HalfWidth = Size * 0.16f;
		const float Length = Size * 0.24f;

		DrawList->AddTriangleFilled(
			ImVec2(P.x + Radial.x * HalfWidth, P.y + Radial.y * HalfWidth),
			ImVec2(P.x - Radial.x * HalfWidth, P.y - Radial.y * HalfWidth),
			ImVec2(P.x + Tangent.x * Length, P.y + Tangent.y * Length),
			Color);

		return bPressed;
	}

	bool DrawVector3Controller(const FString& Label, float* Values, const float* ResetValue, float LabelWidth)
	{
		ImGuiTableFlags Flags = ImGuiTableFlags_BordersInnerV;

		bool bVectorChanged = false;

		ImGui::PushID(Label.c_str());
		if (ImGui::BeginTable(Label.c_str(), 3, Flags))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 30.0f);

			ImGui::TableNextColumn();
			ImGui::Text("%s", Label.c_str());

			ImGui::TableNextColumn();
			ImGui::PushMultiItemsWidths(3, ImGui::GetContentRegionAvail().x);

			bool bValueChanged = false;
			for (int i = 0; i < 3; i++)
			{
				bValueChanged |= Values[i] != ResetValue[i];

				ImGui::PushID(i);
				
				if (i > 0)
				{
					ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
				}

				if (ImGui::DragFloat("##v", &Values[i], 0.1f, 0.0f, 0.0f, "%.2f"))
				{
					bVectorChanged = true;
				}

				const ImVec2 Min = ImGui::GetItemRectMin();
				const ImVec2 Max = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRectFilled(Min, ImVec2(Min.x + 2.0f, Max.y), AxisColors[i]);

				ImGui::PopID();
				ImGui::PopItemWidth();
			}

			ImGui::TableNextColumn();
			if (bValueChanged)
			{
				if (DrawResetButton("Reset"))
				{
					for (int i = 0; i < 3; i++)
					{
						Values[i] = ResetValue[i];
					}
					bVectorChanged = true;
				}
			}

			ImGui::EndTable();
		}
		ImGui::PopID();

		return bVectorChanged;
	}

	bool DrawRotatorController(const FString& Label, FRotator& Rotator, const FRotator& ResetValue, float LabelWidth)
	{
		float Values[3] = { Rotator.Roll, Rotator.Pitch, Rotator.Yaw };
		const float ResetValues[3] = { ResetValue.Roll, ResetValue.Pitch, ResetValue.Yaw };

		const bool bChanged = DrawVector3Controller(Label, Values, ResetValues, LabelWidth);
		if (bChanged)
		{
			Rotator.Roll = Values[0];
			Rotator.Pitch = Values[1];
			Rotator.Yaw = Values[2];
		}
		return bChanged;
	}
}

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
	PreviewPose.UpdateComponentSpace();
}

void FSkeletalMeshEditorPanel::OnRender()
{
	bool bPanelOpen = IsOpen();
	bFocused = false;
	if (ImGui::Begin(GetPanelName(), &bPanelOpen))
	{
		SetOpen(bPanelOpen);
		TickGizmoShortcut();

		if (ImGui::BeginTable("##SkeletalMeshEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Tree", ImGuiTableColumnFlags_WidthFixed, 250.0f);

			ImGui::TableNextColumn();

			DrawViewport();

			ImGui::TableNextColumn();

			if (ImGui::BeginChild("##SkeletonTree", ImVec2(0, 300), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
			{
				ImGui::SeparatorText("Skeleton Tree");
				DrawBoneTreeNode(0);

				if (ImGui::IsWindowHovered() &&
					!ImGui::IsAnyItemHovered() &&
					ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					SelectBone(-1);
				}
			}
			ImGui::EndChild();

			if (ImGui::BeginChild("##Details", ImVec2(0, 0), ImGuiChildFlags_Borders))
			{
				ImGui::SeparatorText("Details");
				
				if (SelectedBoneIndex >= 0 && SelectedBoneIndex < ReferenceSkeleton.RefBoneInfo.Num())
				{
					ImGui::Text("Name: %s", ReferenceSkeleton.RefBoneInfo[SelectedBoneIndex].Name.ToString().c_str());

					FVector& Location = PreviewPose.LocalPose[SelectedBoneIndex].Location;
					FRotator& Rotation = PreviewPose.LocalPose[SelectedBoneIndex].Rotation;
					FVector& Scale = PreviewPose.LocalPose[SelectedBoneIndex].Scale;

					DrawVector3Controller("Location", &Location[0], &ReferenceSkeleton.RefBonePoses[SelectedBoneIndex].Location[0], 90.0f);
					DrawRotatorController("Rotation", Rotation, ReferenceSkeleton.RefBonePoses[SelectedBoneIndex].Rotation, 90.0f);
					DrawVector3Controller("Scale", &Scale[0], &ReferenceSkeleton.RefBonePoses[SelectedBoneIndex].Scale[0], 90.0f);
				}
			}
			ImGui::EndChild();

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
	const bool bCaptured = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Right) && !BoneGizmo.IsUsing();

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

void FSkeletalMeshEditorPanel::TickGizmoShortcut()
{
	bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (!bFocused || ImGui::GetIO().WantTextInput || BoneGizmo.IsUsing())
	{
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		const int NextMode = (static_cast<int>(BoneGizmo.GetMode()) + 1) % 3;
		BoneGizmo.SetMode(static_cast<EGizmoMode>(NextMode));
	}
}

void FSkeletalMeshEditorPanel::BuildDummySkeleton()
{
	ReferenceSkeleton = {};
	PreviewPose.LocalPose.Reset();

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

	PreviewPose.Skeleton = &ReferenceSkeleton;
	PreviewPose.Reset();

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

void FSkeletalMeshEditorPanel::DrawViewport()
{
	DrawGizmoToolbar();

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
		ImGui::InvisibleButton("##PreviewViewport", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		ImGui::GetWindowDrawList()->AddImage(ViewportClient.GetColorTarget()->GetSRV(), Min, ImVec2(Min.x + Size.x, Min.y + Size.y));
		TickCameraInput();
		TickBoneGizmo(Min);
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
		SelectBone(BoneIndex);
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

void FSkeletalMeshEditorPanel::SelectBone(int32 BoneIndex)
{
	SelectedBoneIndex = BoneIndex;
	BoneGizmoTarget.SetBone(&PreviewPose, BoneIndex);
	BoneGizmo.SetTarget(BoneIndex >= 0 ? &BoneGizmoTarget : nullptr);
}

void FSkeletalMeshEditorPanel::TickBoneGizmo(const ImVec2& ViewportMin)
{
	const ImGuiIO& IO = ImGui::GetIO();
	const FVector2 LocalMouse(IO.MousePos.x - ViewportMin.x, IO.MousePos.y - ViewportMin.y);
	const bool bMouseDown = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);

	BoneGizmo.Update(
		ViewportClient.DeprojectScreenToWorld(LocalMouse),
		LocalMouse,
		ViewportClient.GetViewProjectionMatrix(),
		static_cast<int32>(ViewportClient.GetWidth()),
		static_cast<int32>(ViewportClient.GetHeight()),
		bMouseDown,
		ViewportClient.GetViewLocation(),
		!ViewportClient.IsPerspective()
	);

	if (ImGui::IsItemHovered() &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!BoneGizmo.IsUsing() &&
		BoneGizmo.GetHoveredAxis() < 0)
	{
		SelectBone(-1);
	}
}

void FSkeletalMeshEditorPanel::DrawGizmoToolbar()
{
	const EGizmoMode Mode = BoneGizmo.GetMode();
	if (ImGui::RadioButton("Location", Mode == EGizmoMode::Location))
	{
		BoneGizmo.SetMode(EGizmoMode::Location);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotation", Mode == EGizmoMode::Rotation))
	{
		BoneGizmo.SetMode(EGizmoMode::Rotation);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", Mode == EGizmoMode::Scale))
	{
		BoneGizmo.SetMode(EGizmoMode::Scale);
	}
	ImGui::SameLine();

	const bool bLocal = BoneGizmo.GetSpace() == EGizmoSpace::Local;
	if (ImGui::Button(bLocal ? "Local" : "World"))
	{
		BoneGizmo.SetSpace(bLocal ? EGizmoSpace::World : EGizmoSpace::Local);
	}
}

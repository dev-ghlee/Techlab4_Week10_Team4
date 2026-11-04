#include "EnginePCH.h"
#include "SkeletalMeshEditorPanel.h"

#include "Camera/ViewportCameraMovement.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Engine/World.h"
#include "Asset/AssetManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Render/ReferenceSkeleton.h"

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
			ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 35.0f);

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
	SetOpen(false);

	PreviewWorld = UWorld::CreateWorld(EWorldType::Editor, false, "SkeletalMeshPreview");
	ViewportClient.SetWorld(PreviewWorld);
	ViewportClient.SetViewLocation(FVector{ -4.0f, 0.0f, 2.0f });
	ViewportClient.SetViewRotation(FVector{ 20.0f, 0.0f, 0.0f });

	if (AActor* PreviewActor = PreviewWorld->SpawnActor<AActor>("SkeletalPreview"))
	{
		PreviewMeshComponent = Cast<USkeletalMeshComponent>(PreviewActor->AddComponent(USkeletalMeshComponent::StaticClass(), "PreviewMesh"));
	}

	// 라이트 액터 대신 컴포넌트만 붙여서 프리뷰에 아이콘/화살표가 보이지 않게 한다
	if (AActor* LightActor = PreviewWorld->SpawnActor<AActor>("PreviewLights"))
	{
		if (auto* Sky = Cast<USkyLightComponent>(LightActor->AddComponent(USkyLightComponent::StaticClass(), "PreviewAmbient")))
		{
			Sky->SetIntensity(0.3f);
		}

		if (auto* Sun = Cast<UDirectionalLightComponent>(LightActor->AddComponent(UDirectionalLightComponent::StaticClass(), "PreviewDirectional")))
		{
			Sun->SetRelativeRotation(FRotator(45.0f, 30.0f, 0.0f));
			Sun->SetIntensity(1.0f);
		}
	}

	return true;
}

void FSkeletalMeshEditorPanel::Tick(float DeltaTime)
{
	PreviewPose.UpdateComponentSpace();
	SyncPoseToComponent();

	if (PreviewWorld)
	{
		PreviewWorld->GetScene().UpdateAllTransforms();
	}
}

void FSkeletalMeshEditorPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2{ 2100.0f, 1200.0f }, ImGuiCond_FirstUseEver);

	bool bPanelOpen = IsOpen();
	bFocused = false;
	if (ImGui::Begin(GetPanelName(), &bPanelOpen))
	{
		SetOpen(bPanelOpen);
		TickShortcut();

		if (ImGui::BeginTable("##SkeletalMeshEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Tree", ImGuiTableColumnFlags_WidthFixed, 150.0f);

			ImGui::TableNextColumn();

			DrawViewport();

			ImGui::TableNextColumn();

			if (ImGui::BeginChild("##SkeletonTree", ImVec2(0, 850), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
			{
				ImGui::SeparatorText("Skeleton Tree");
				ImGui::SameLine();

				if (ImGui::Button("Reset All", ImVec2(150, 0)))
				{
					PreviewPose.LocalPose = ReferenceSkeleton.RawRefBonePose;
				}

				for (int32 Index = 0; Index < ReferenceSkeleton.RawRefBoneInfo.Num(); ++Index)
				{
					if (ReferenceSkeleton.RawRefBoneInfo[Index].ParentIndex < 0)
					{
						DrawBoneTreeNode(Index);
					}
				}

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
				
				if (SelectedBoneIndex >= 0 && SelectedBoneIndex < ReferenceSkeleton.RawRefBoneInfo.Num())
				{
					ImGui::Text("Name: %s", ReferenceSkeleton.RawRefBoneInfo[SelectedBoneIndex].Name.ToString().c_str());

					FVector& Location = PreviewPose.LocalPose[SelectedBoneIndex].Location;
					FRotator& Rotation = PreviewPose.LocalPose[SelectedBoneIndex].Rotation;
					FVector& Scale = PreviewPose.LocalPose[SelectedBoneIndex].Scale;

					DrawVector3Controller("Location", &Location[0], &ReferenceSkeleton.RawRefBonePose[SelectedBoneIndex].Location[0], 90.0f);
					DrawRotatorController("Rotation", Rotation, ReferenceSkeleton.RawRefBonePose[SelectedBoneIndex].Rotation, 90.0f);
					DrawVector3Controller("Scale", &Scale[0], &ReferenceSkeleton.RawRefBonePose[SelectedBoneIndex].Scale[0], 90.0f);
				}
			}
			ImGui::EndChild();

			ImGui::EndTable();
		}
	}

	ImGui::End();

	const bool bAnyItemActive = ImGui::IsAnyItemActive();
	if (bWasAnyItemActive && !bAnyItemActive)
	{
		CommitUndo();
	}
	bWasAnyItemActive = bAnyItemActive;
}

UWorld* FSkeletalMeshEditorPanel::ReleasePreviewWorld()
{
	ViewportClient.SetWorld(nullptr);
	UWorld* World = PreviewWorld;
	PreviewWorld = nullptr;
	return World;
}

bool FSkeletalMeshEditorPanel::OpenAsset(const FString& Path)
{
	if (!LoadMesh(Path))
	{
		HTR_LOG(Error, "Failed to open skeletal mesh asset: {}", Path);
		return false;
	}
	SetOpen(true);
	return true;
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

	ViewportClient.ApplyCameraInput(Input, IO.DeltaTime, CameraSpeed * Settings->CameraSpeed, 0.2f * Settings->MouseSensitivity);
}

void FSkeletalMeshEditorPanel::TickShortcut()
{
	const ImGuiIO& IO = ImGui::GetIO();

	bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (!bFocused || IO.WantTextInput || BoneGizmo.IsUsing() || ImGui::IsAnyItemActive())
	{
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		const int NextMode = (static_cast<int>(BoneGizmo.GetMode()) + 1) % 3;
		BoneGizmo.SetMode(static_cast<EGizmoMode>(NextMode));
	}

	if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
	{
		IO.KeyShift ? Redo() : Undo();
	}
	else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))
	{
		Redo();
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
	if (BoneIndex < 0 || BoneIndex >= ReferenceSkeleton.RawRefBoneInfo.Num())
		return;

	const auto& RefBoneInfo = ReferenceSkeleton.RawRefBoneInfo[BoneIndex];

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

void FSkeletalMeshEditorPanel::SetSkeleton(const FReferenceSkeleton& Skeleton)
{
	ReferenceSkeleton = Skeleton;
	PreviewPose.Skeleton = &ReferenceSkeleton;
	PreviewPose.Reset();

	Children.Reset();
	Children.SetNum(ReferenceSkeleton.RawRefBoneInfo.Num());
	for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.RawRefBoneInfo.Num(); ++BoneIndex)
	{
		const int32 ParentIndex = ReferenceSkeleton.RawRefBoneInfo[BoneIndex].ParentIndex;
		if (ParentIndex >= 0)
		{
			Children[ParentIndex].Add(BoneIndex);
		}
	}
	PreviewPose.UpdateComponentSpace();

	FVector Min{ FLT_MAX, FLT_MAX, FLT_MAX };
	FVector Max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (const auto& Bone : PreviewPose.ComponentSpacePose)
	{
		const FVector Location = Bone.GetOrigin();
		Min.X = std::min(Min.X, Location.X);
		Max.X = std::max(Max.X, Location.X);
		Min.Y = std::min(Min.Y, Location.Y);
		Max.Y = std::max(Max.Y, Location.Y);
		Min.Z = std::min(Min.Z, Location.Z);
		Max.Z = std::max(Max.Z, Location.Z);
	}
	ViewportClient.SetViewRotation(FVector{ 20.0f, 0.0f, 0.0f });
	ViewportClient.FocusBounds({ Min, Max });

	CameraSpeed = std::max(1.0f, (Max - Min).Length() * 0.5f);

	UndoStack.Reset();
	RedoStack.Reset();
	LastSnapshot = PreviewPose.LocalPose;

	SelectBone(-1);
}

bool FSkeletalMeshEditorPanel::LoadMesh(const FString& Path)
{
	FString Error;
	USkeletalMesh* Mesh = UAssetManager::Get().LoadFbxSkeletalMesh(Path, Path, Error);
	if (!Mesh || !PreviewMeshComponent)
	{
		HTR_LOG(Error, "{}", Error);
		return false;
	}

	PreviewMeshComponent->SetSkeletalMesh(Mesh);
	SetSkeleton(Mesh->GetRefSkeleton());

	return true;
}

void FSkeletalMeshEditorPanel::SelectBone(int32 BoneIndex)
{
	SelectedBoneIndex = BoneIndex;
	BoneGizmoTarget.SetBone(&PreviewPose, BoneIndex);
	BoneGizmo.SetTarget(BoneIndex >= 0 ? &BoneGizmoTarget : nullptr);
}

int32 FSkeletalMeshEditorPanel::PickBone(const FVector2& MousePosition)
{
	TArray<float> JointRadii = GetJointRadii(PreviewPose);

	const auto& Pose = PreviewPose.ComponentSpacePose;
	const FMatrix ViewProjection = ViewportClient.GetViewProjectionMatrix();
	const FVector CameraRight = ViewportClient.GetViewRotation().Quaternion().GetRightVector();
	const int32 Width = static_cast<int32>(ViewportClient.GetWidth());
	const int32 Height = static_cast<int32>(ViewportClient.GetHeight());

	const auto GetHitRadius = [&](const FVector& WorldLocation, const FVector2& ScreenLocation, float WorldRadius) -> float
		{
			constexpr float MinHitPixels = 6.0f;
			const FVector2 Edge = WorldToScreen(WorldLocation + CameraRight * WorldRadius, ViewProjection, Width, Height);
			const float HitPixels = (Edge - ScreenLocation).Length();
			return std::max(HitPixels, MinHitPixels);
		};

	TArray<FVector2> ScreenJoints;
	ScreenJoints.SetNum(Pose.Num());
	for (int32 Index = 0; Index < Pose.Num(); ++Index)
	{
		ScreenJoints[Index] = WorldToScreen(Pose[Index].GetOrigin(), ViewProjection, Width, Height);
	}

	// Joint
	float MinDistance = FLT_MAX;
	int32 PickedJoint = -1;
	for (int32 Index = 0; Index < Pose.Num(); ++Index)
	{
		if (ScreenJoints[Index].X == -FLT_MAX)
		{
			continue;
		}
		
		const float Distance = (MousePosition - ScreenJoints[Index]).Length();
		const float HitRadius = GetHitRadius(Pose[Index].GetOrigin(), ScreenJoints[Index], JointRadii[Index]);
		if (Distance <= HitRadius && Distance < MinDistance)
		{
			MinDistance = Distance;
			PickedJoint = Index;
		}
	}
	if (PickedJoint >= 0)
	{
		return PickedJoint;
	}

	// Bone
	MinDistance = FLT_MAX;
	int32 PickedBone = -1;
	for (int32 Index = 0; Index < Pose.Num(); ++Index)
	{
		const int32 ParentIndex = ReferenceSkeleton.RawRefBoneInfo[Index].ParentIndex;
		if (ParentIndex < 0 || ScreenJoints[Index].X == -FLT_MAX || ScreenJoints[ParentIndex].X == -FLT_MAX)
		{
			continue;
		}

		const FVector ParentLocation = Pose[ParentIndex].GetOrigin();
		const FVector ChildLocation = Pose[Index].GetOrigin();
		const FVector Middle = (ParentLocation + ChildLocation) * 0.5f;
		const float WorldRadius = (ChildLocation - ParentLocation).Length() * 0.1f;

		const float Distance = DistanceToSegment(MousePosition, ScreenJoints[ParentIndex], ScreenJoints[Index]);
		const float HitRadius = GetHitRadius(Middle, WorldToScreen(Middle, ViewProjection, Width, Height), WorldRadius);
		if (Distance <= HitRadius && Distance < MinDistance)
		{
			MinDistance = Distance;
			PickedBone = ParentIndex;
		}
	}

	return PickedBone;
}

void FSkeletalMeshEditorPanel::TickBoneGizmo(const ImVec2& ViewportMin)
{
	const ImGuiIO& IO = ImGui::GetIO();
	const FVector2 LocalMouse(IO.MousePos.x - ViewportMin.x, IO.MousePos.y - ViewportMin.y);

	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		bJustPicked = false;
	}

	const bool bMouseDown = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !bJustPicked;

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
		int32 PickedBone = PickBone(LocalMouse);
		if (PickedBone >= 0)
		{
			bJustPicked = true;
		}
		SelectBone(PickedBone);
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
	ImGui::SameLine();
	
	const ImGuiStyle& Style = ImGui::GetStyle();
	const auto GetCheckboxWidth = [&](const char* Label)
		{
			return ImGui::GetFrameHeight() + Style.ItemInnerSpacing.x + ImGui::CalcTextSize(Label, nullptr, true).x;
		};

	const float ButtonsWidth = GetCheckboxWidth("Grid") + GetCheckboxWidth("Mesh") + GetCheckboxWidth("Bones") + Style.ItemSpacing.x * 2.0f;
	const float Offset = ImGui::GetContentRegionAvail().x - ButtonsWidth;
	if (Offset > 0.0f)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Offset);
	}

	ImGui::Checkbox("Grid", &bRenderGrid);
	ImGui::SameLine();
	if (ImGui::Checkbox("Mesh", &bRenderMesh))
	{
		PreviewMeshComponent->SetVisibility(bRenderMesh);
	}
	ImGui::SameLine();
	ImGui::Checkbox("Bones", &bRenderBones);
}

void FSkeletalMeshEditorPanel::CommitUndo()
{
	bool bChanged = false;
	TArray<FBoneChange> Changes;
	for (int32 Index = 0; Index < PreviewPose.LocalPose.Num(); ++Index)
	{
		if (PreviewPose.LocalPose[Index] != LastSnapshot[Index])
		{
			FBoneChange Change;
			Change.BoneIndex = Index;
			Change.Before = LastSnapshot[Index];
			Change.After = PreviewPose.LocalPose[Index];
			Changes.Push(Change);
			LastSnapshot[Index] = PreviewPose.LocalPose[Index];
			bChanged = true;
		}
	}
	
	if (bChanged)
	{
		RedoStack.Reset();
		UndoStack.Push(Changes);
	}
}

void FSkeletalMeshEditorPanel::Undo()
{
	if (UndoStack.Num() > 0)
	{
		const TArray<FBoneChange> Changes = UndoStack.Last();
		UndoStack.Pop();
		
		for (const FBoneChange& Change : Changes)
		{
			PreviewPose.LocalPose[Change.BoneIndex] = Change.Before;
		}
		RedoStack.Push(Changes);

		for (const FBoneChange& Change : Changes)
		{
			LastSnapshot[Change.BoneIndex] = Change.Before;
		}
	}
}

void FSkeletalMeshEditorPanel::Redo()
{
	if (RedoStack.Num() > 0)
	{
		const TArray<FBoneChange> Changes = RedoStack.Last();
		RedoStack.Pop();
		
		for (const FBoneChange& Change : Changes)
		{
			PreviewPose.LocalPose[Change.BoneIndex] = Change.After;
		}
		UndoStack.Push(Changes);

		for (const FBoneChange& Change : Changes)
		{
			LastSnapshot[Change.BoneIndex] = Change.After;
		}
	}
}

void FSkeletalMeshEditorPanel::SyncPoseToComponent()
{
	if (!PreviewMeshComponent || PreviewMeshComponent->GetNumBones() != PreviewPose.LocalPose.Num())
	{
		return;
	}

	for (int32 Index = 0; Index < PreviewPose.LocalPose.Num(); ++Index)
	{
		if (PreviewPose.LocalPose[Index] != PreviewMeshComponent->GetBoneLocalTransform(Index))
		{
			PreviewMeshComponent->SetBoneLocalTransform(Index, PreviewPose.LocalPose[Index]);
		}
	}
}

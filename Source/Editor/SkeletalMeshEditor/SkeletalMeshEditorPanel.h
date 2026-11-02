#pragma once

#include "SkeletalTemp.h"
#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Gizmo/BoneGizmoTarget.h"
#include "Editor/Gizmo/Gizmo.h"
#include "Editor/Viewports/EditorViewportClient.h"

struct FEditorSettings;
class UWorld;

class FSkeletalMeshEditorPanel : public IEditorPanel
{
public:
	virtual bool Init() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnRender() override;
	virtual const char* GetPanelName() const override { return "Skeletal Mesh Editor"; }

	UWorld* ReleasePreviewWorld();
	UWorld* GetPreviewWorld() const { return PreviewWorld; }
	FEditorViewportClient* GetViewportClient() { return &ViewportClient; }
	void SetEditorSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	bool IsFocused() const { return IsOpen() && bFocused; }

	const FGizmo& GetBoneGizmo() const { return BoneGizmo; }

	const FSkeletalPose& GetPreviewPose() const { return PreviewPose; }
	int32 GetSelectedBoneIndex() const { return SelectedBoneIndex; }

private:
	void TickCameraInput();
	void TickGizmoShortcut();

	void BuildDummySkeleton();

	void DrawViewport();
	void DrawBoneTreeNode(int32 BoneIndex);

	void SelectBone(int32 BoneIndex);
	int32 PickBone(const FVector2& MousePosition);
	void TickBoneGizmo(const ImVec2& ViewportMin);
	void DrawGizmoToolbar();

	UWorld* PreviewWorld = nullptr;
	FEditorViewportClient ViewportClient;
	const FEditorSettings* Settings = nullptr;
	bool bFocused = false;
	bool bJustPicked = false;

	FGizmo BoneGizmo;
	FBoneGizmoTarget BoneGizmoTarget;

	FReferenceSkeleton ReferenceSkeleton;
	FSkeletalPose PreviewPose;

	TArray<TArray<int32>> Children;
	int32 SelectedBoneIndex = -1;
};

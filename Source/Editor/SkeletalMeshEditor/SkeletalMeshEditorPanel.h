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

	void DrawBones(FLineBatcher& LineBatcher) const;

	UWorld* ReleasePreviewWorld();
	UWorld* GetPreviewWorld() const { return PreviewWorld; }
	FEditorViewportClient* GetViewportClient() { return &ViewportClient; }
	void SetEditorSettings(const FEditorSettings* InSettings) { Settings = InSettings; }

	const FGizmo& GetBoneGizmo() const { return BoneGizmo; }

private:
	void TickCameraInput();

	void BuildDummySkeleton();

	void DrawViewport();
	void DrawBoneTreeNode(int32 BoneIndex);

	void SelectBone(int32 BoneIndex);
	void TickBoneGizmo(const ImVec2& ViewportMin);
	void DrawGizmoToolbar();

	UWorld* PreviewWorld = nullptr;
	FEditorViewportClient ViewportClient;
	const FEditorSettings* Settings = nullptr;

	FGizmo BoneGizmo;
	FBoneGizmoTarget BoneGizmoTarget;

	FReferenceSkeleton ReferenceSkeleton;
	FSkeletalPose PreviewPose;

	TArray<TArray<int32>> Children;
	int32 SelectedBoneIndex = -1;
};

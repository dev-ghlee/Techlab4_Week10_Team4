#pragma once

#include "SkeletalTemp.h"
#include "Editor/EditorUI/EditorPanel.h"
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

	const TArray<FMatrix>& GetComponentSpacePose() const { return ComponentSpacePose; }
	const FReferenceSkeleton& GetReferenceSkeleton() const { return ReferenceSkeleton; }

private:
	void TickCameraInput();

	void BuildDummySkeleton();
	void ResetPose();
	void UpdateComponentSpacePose();

	void DrawBoneTreeNode(int32 BoneIndex);

	UWorld* PreviewWorld = nullptr;
	FEditorViewportClient ViewportClient;
	const FEditorSettings* Settings = nullptr;

	FReferenceSkeleton ReferenceSkeleton;
	TArray<FTransform> LocalPose;
	TArray<FMatrix> ComponentSpacePose;

	TArray<TArray<int32>> Children;
	int32 SelectedBoneIndex = -1;
};

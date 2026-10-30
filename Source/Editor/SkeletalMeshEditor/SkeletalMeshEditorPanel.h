#pragma once

#include "SkeletalTemp.h"
#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"

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

	const TArray<FMatrix>& GetComponentSpacePose() const { return ComponentSpacePose; }
	const FReferenceSkeleton& GetReferenceSkeleton() const { return ReferenceSkeleton; }

private:
	void BuildDummySkeleton();
	void ResetPose();
	void UpdateComponentSpacePose();

	UWorld* PreviewWorld = nullptr;
	FEditorViewportClient ViewportClient;

	FReferenceSkeleton ReferenceSkeleton;
	TArray<FTransform> LocalPose;
	TArray<FMatrix> ComponentSpacePose;
};

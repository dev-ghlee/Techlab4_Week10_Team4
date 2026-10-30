#pragma once

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

private:
	UWorld* PreviewWorld = nullptr;
	FEditorViewportClient ViewportClient;
};

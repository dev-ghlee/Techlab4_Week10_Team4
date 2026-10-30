#include "EnginePCH.h"
#include "SkeletalMeshEditorPanel.h"

#include "Engine/World.h"

bool FSkeletalMeshEditorPanel::Init()
{
	PreviewWorld = UWorld::CreateWorld(EWorldType::Editor, false, "SkeletalMeshPreview");
	ViewportClient.SetWorld(PreviewWorld);
	ViewportClient.SetViewLocation(FVector{ -4.0f, 0.0f, 2.0f });
	ViewportClient.SetViewRotation(FVector{ 20.0f, 0.0f, 0.0f });

	return true;
}

void FSkeletalMeshEditorPanel::Tick(float DeltaTime)
{
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

#include "EnginePCH.h"
#include "FbxImporter.h"

#include "Containers/Array.h"

#include <fbxsdk.h>

// Mesh Node와 해당 Skin, Cluster의 연결 관계.
// 모든 포인터는 SDK 객체를 참조하며 소유하지 않는다.
struct FFbxSkinCluster
{
	// Transform과 재질을 확인할 Mesh의 Node.
	FbxNode* MeshNode = nullptr;

	// 이 Cluster가 속한 Skin Deformer.
	FbxSkin* Skin = nullptr;

	// 연결 본, Control Point Weight, 바인드 정보를 제공한다.
	FbxCluster* Cluster = nullptr;
};

// 이번 임포트 작업에서만 사용하는 임시 정보.
struct FImportContext
{
	// 삼각형화가 끝난 Mesh를 소유한 Node 목록.
	// SDK 객체를 참조할 뿐, 소유하거나 직접 해제하지 않는다.
	TArray<FbxNode*> MeshNodes;

	// 각 Mesh Node에 연결된 Skin과 Cluster 정보.
	TArray<FFbxSkinCluster> SkinClusters;
};

bool FFbxImporter::LoadScene(const FString& Path, FSkeletalMeshData& OutData, FString& OutError)
{
	OutError.clear();

	// 함수가 끝나면 Manager의 Destroy()를 자동으로 호출한다.
	FbxAutoDestroyPtr<FbxManager> Manager(FbxManager::Create());
	if (!Manager)
	{
		OutError = "Failed to create FBX manager.";
		return false;
	}

	// Manager가 소유하므로 Manager 정리 시 함께 해제된다.
	FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager.Get(), IOSROOT);
	FbxScene* Scene = FbxScene::Create(Manager.Get(), "ImportScene");
	FbxImporter* Importer = FbxImporter::Create(Manager.Get(), "Importer");

	if (!IOSettings || !Scene || !Importer)
	{
		OutError = "Failed to create FBX import objects.";
		return false;
	}

	Manager->SetIOSettings(IOSettings);

	// 파일을 열고 형식을 자동 판별한 뒤 내용을 Scene으로 읽는다.
	if (!Importer->Initialize(Path.c_str(), -1, IOSettings)
		|| !Importer->Import(Scene))
	{
		OutError = Importer->GetStatus().GetErrorString();
		return false;
	}

	// 이후 계층 순회를 시작할 최상위 Node를 확인한다.
	if (!Scene->GetRootNode())
	{
		OutError = "FBX scene has no root node.";
		return false;
	}

	// 엔진 축: 오른쪽 +Y, 위쪽 +Z, 전방 +X.
	// SDK 문자열 순서는 오른쪽 / 위쪽 / 화면 안쪽 방향이다.
	FbxAxisSystem EngineAxis;
	if (!FbxAxisSystem::ParseAxisSystem("yzx", EngineAxis))
	{
		OutError = "Failed to define the engine axis system.";
		
		return false;
	}

	// 좌표계가 다를 때 변환하며 좌우 손잡이 변경도 처리한다.
	if (Scene->GetGlobalSettings().GetAxisSystem() != EngineAxis)
	{
		EngineAxis.DeepConvertScene(Scene);
	}

	// 엔진 길이 단위를 센티미터로 통일한다.
	const FbxSystemUnit& EngineUnit = FbxSystemUnit::cm;
	if (Scene->GetGlobalSettings().GetSystemUnit() != EngineUnit)
	{
		// 기본 옵션을 사용해 Cluster의 바인드 정보도 함께 변환한다.
		EngineUnit.ConvertScene(Scene);
	}

	// Scene의 지원 가능한 Geometry를 삼각형화한다.
	// true: 기존 Geometry를 삼각형화한 결과로 교체한다.
	// false: 구형 삼각형화 방식을 사용하지 않는다.
	FbxGeometryConverter GeometryConverter(Manager.Get());

	if (!GeometryConverter.Triangulate(Scene, true, false))
	{
		OutError = "Failed to triangulate FBX scene.";
		return false;
	}

	// 교체가 끝난 Scene에서 Mesh를 다시 조회한다.
	const int32 MeshCount = Scene->GetSrcObjectCount<FbxMesh>();
	if (MeshCount == 0)
	{
		OutError = "FBX scene contains no meshes.";
		return false;
	}

	// 각 Mesh의 Polygon이 모두 삼각형인지 확인한다.
	for (int32 MeshIndex = 0; MeshIndex < MeshCount; ++MeshIndex)
	{
		FbxMesh* Mesh = Scene->GetSrcObject<FbxMesh>(MeshIndex);

		if (!Mesh || !Mesh->IsTriangleMesh())
		{
			OutError = "FBX mesh triangulation verification failed.";
			return false;
		}
	}

	// 하나의 Context를 재귀 호출 전체에서 공유한다.
	FImportContext Context;
	LoadSceneRecursive(Scene->GetRootNode(), Context);

	if (Context.MeshNodes.IsEmpty())
	{
		OutError = "FBX scene contains no mesh nodes to import.";
		return false;
	}

	// 수집한 Mesh Node마다 연결된 Skin을 확인한다.
	for (FbxNode* MeshNode : Context.MeshNodes)
	{
		FbxMesh* Mesh = MeshNode->GetMesh();
		const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

		// Skin이 없으면 반복문을 실행하지 않는다.
		for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
		{
			// eSkin으로 조회했으므로 FbxSkin으로 변환한다.
			FbxSkin* Skin = static_cast<FbxSkin*>(
				Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));

			if (!Skin)
			{
				OutError = "Failed to access FBX skin deformer.";
				return false;
			}

			// Skin에 연결된 각 Cluster를 수집한다.
			for (int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ++ClusterIndex)
			{
				FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);

				// 연결 본이 없는 Cluster는 스키닝에 사용할 수 없다.
				if (!Cluster || !Cluster->GetLink())
				{
					OutError = "FBX cluster has no valid linked bone.";
					return false;
				}

				Context.SkinClusters.Add({ MeshNode, Skin, Cluster });
			}
		}
	}

	// SkeletalMesh를 구성할 연결 정보가 있는지 확인한다.
	if (Context.SkinClusters.IsEmpty())
	{
		OutError = "FBX scene contains no skin clusters.";
		return false;
	}

	// 이후 여기에 스키닝 방식 검사와 본, 메시 등의 데이터 추출을 추가한다.
	// 완성된 임시 데이터를 OutData에 이동한 뒤 true를 반환한다.
	OutError = "Skeletal mesh data extraction is not implemented yet.";

	return false;
}

void FFbxImporter::LoadSceneRecursive(FbxNode* Node, FImportContext& Context)
{
	if (!Node)
	{
		return;
	}

	// 이후 Transform과 재질 정보도 사용하므로 Mesh 대신 Node를 저장한다.
	if (Node->GetMesh())
	{
		Context.MeshNodes.Add(Node);
	}

	// 현재 Node에 Mesh가 없어도 자식 탐색은 계속한다.
	for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
	{
		LoadSceneRecursive(Node->GetChild(ChildIndex), Context);
	}
}

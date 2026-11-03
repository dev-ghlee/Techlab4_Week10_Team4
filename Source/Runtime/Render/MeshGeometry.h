#pragma once

#include "Core/Types.h"

class FVertexBuffer;
class FIndexBuffer;

// 버퍼의 소유권은 메시 리소스나 MeshObject에 있다. 렌더 패킷은 현재 버퍼만 참조한다.
struct FMeshGeometry
{
	FVertexBuffer* VertexBuffer = nullptr;
	FIndexBuffer* IndexBuffer = nullptr;
	uint16 SortID = 0;

	bool IsValid() const { return VertexBuffer && IndexBuffer; }
};

#pragma once

struct FBox
{
	FVector Min;
	FVector Max;
	
	FVector GetCenter() { return (Min + Max) * 0.5f; }
	FVector GetExtent() { return (Max - Min) * 0.5f; }
	FVector GetSize() const { return Max - Min; }
	FBox() : Min(FVector::ZeroVector), Max(FVector::ZeroVector) {}
	FBox(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax) {}


	FBox GetWorldAABB(const FMatrix& M) const
	{
		FVectorRegister Minreg = VectorSIMD::LoadFloat3(&Min.X);
		FVectorRegister Maxreg = VectorSIMD::LoadFloat3(&Max.X);
		FVectorRegister Centerreg = VectorSIMD::Mul(VectorSIMD::Add(Minreg, Maxreg), VectorSIMD::SetVal(0.5f));
		FVectorRegister Extentreg = VectorSIMD::Mul(VectorSIMD::Sub(Maxreg, Minreg), VectorSIMD::SetVal(0.5f));

		// 중심은 그냥 변환
		float center[4];
		VectorSIMD::Store(center, Centerreg);
		FVector4 C = FVector4(center[0], center[1], center[2], 1.0f) * M;

		// 범위는 회전 부분의 절댓값으로 변환
		FVectorRegister Ereg = VectorSIMD::Add(
			VectorSIMD::Add(
				VectorSIMD::Mul(VectorSIMD::SplatX(Extentreg), VectorSIMD::Abs(VectorSIMD::Load(M.M[0]))),
				VectorSIMD::Mul(VectorSIMD::SplatY(Extentreg), VectorSIMD::Abs(VectorSIMD::Load(M.M[1])))
			),
			VectorSIMD::Mul(VectorSIMD::SplatZ(Extentreg), VectorSIMD::Abs(VectorSIMD::Load(M.M[2])))
		);

		FVector E;
		VectorSIMD::StoreFloat3(&E.X, Ereg);

		return FBox{ FVector(C.X, C.Y, C.Z) - E, FVector(C.X, C.Y, C.Z) + E };
	}

	void Expand(const FBox& Other)
	{
		Min.X = std::min(Min.X, Other.Min.X);
		Min.Y = std::min(Min.Y, Other.Min.Y);
		Min.Z = std::min(Min.Z, Other.Min.Z);

		Max.X = std::max(Max.X, Other.Max.X);
		Max.Y = std::max(Max.Y, Other.Max.Y);
		Max.Z = std::max(Max.Z, Other.Max.Z);
	}

	bool Intersects(const FBox& Other) const
	{
		return (Min.X <= Other.Max.X && Max.X >= Other.Min.X) &&
			(Min.Y <= Other.Max.Y && Max.Y >= Other.Min.Y) &&
			(Min.Z <= Other.Max.Z && Max.Z >= Other.Min.Z);
	}
};

struct FWorldBox { FVector Center; FVector Axis[3]; FVector Extent; };
	
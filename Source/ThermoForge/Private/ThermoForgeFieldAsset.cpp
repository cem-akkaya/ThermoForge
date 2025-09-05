#include "ThermoForgeFieldAsset.h"

UThermoForgeFieldAsset::UThermoForgeFieldAsset()
{
    GridFrameWS = FTransform::Identity;
}

bool UThermoForgeFieldAsset::WorldToCellTrilinear(const FVector& P, int32& ix, int32& iy, int32& iz, FVector& Alpha) const
{
    if (Dim.X <= 1 || Dim.Y <= 1 || Dim.Z <= 1 || CellSizeCm <= 0.f) return false;


    const FTransform Frame = GetGridFrame();
    const FTransform InvFrame = Frame.Inverse();

    const FVector Pgrid = InvFrame.TransformPosition(P); 
    const FVector Local = Pgrid / CellSizeCm;
    
    const int32 x0 = FMath::FloorToInt(Local.X);
    const int32 y0 = FMath::FloorToInt(Local.Y);
    const int32 z0 = FMath::FloorToInt(Local.Z);

    const int32 x1 = x0 + 1, y1 = y0 + 1, z1 = z0 + 1;
    if (x0 < 0 || y0 < 0 || z0 < 0 || x1 >= Dim.X || y1 >= Dim.Y || z1 >= Dim.Z) return false;

    ix = x0; iy = y0; iz = z0;
    Alpha = FVector(Local.X - x0, Local.Y - y0, Local.Z - z0);
    return true;
}

static float TF_TrilinearFetch(
    const TArray<float>& Arr, const FIntVector& Dim,
    int32 x0,int32 y0,int32 z0, const FVector& A)
{
    auto I = [&](int32 x,int32 y,int32 z){ return (z*Dim.Y + y)*Dim.X + x; };

    const int32 x1=x0+1, y1=y0+1, z1=z0+1;

    const float c000 = Arr.IsValidIndex(I(x0,y0,z0)) ? Arr[I(x0,y0,z0)] : 0.f;
    const float c100 = Arr.IsValidIndex(I(x1,y0,z0)) ? Arr[I(x1,y0,z0)] : 0.f;
    const float c010 = Arr.IsValidIndex(I(x0,y1,z0)) ? Arr[I(x0,y1,z0)] : 0.f;
    const float c110 = Arr.IsValidIndex(I(x1,y1,z0)) ? Arr[I(x1,y1,z0)] : 0.f;
    const float c001 = Arr.IsValidIndex(I(x0,y0,z1)) ? Arr[I(x0,y0,z1)] : 0.f;
    const float c101 = Arr.IsValidIndex(I(x1,y0,z1)) ? Arr[I(x1,y0,z1)] : 0.f;
    const float c011 = Arr.IsValidIndex(I(x0,y1,z1)) ? Arr[I(x0,y1,z1)] : 0.f;
    const float c111 = Arr.IsValidIndex(I(x1,y1,z1)) ? Arr[I(x1,y1,z1)] : 0.f;

    const float cx00 = FMath::Lerp(c000, c100, A.X);
    const float cx10 = FMath::Lerp(c010, c110, A.X);
    const float cx01 = FMath::Lerp(c001, c101, A.X);
    const float cx11 = FMath::Lerp(c011, c111, A.X);

    const float cxy0 = FMath::Lerp(cx00, cx10, A.Y);
    const float cxy1 = FMath::Lerp(cx01, cx11, A.Y);

    return FMath::Lerp(cxy0, cxy1, A.Z);
}

float UThermoForgeFieldAsset::SampleSkyView01(const FVector& WorldPos) const
{
    int32 ix,iy,iz; FVector A;
    if (!WorldToCellTrilinear(WorldPos, ix,iy,iz, A)) return 0.f;
    return TF_TrilinearFetch(SkyView01, Dim, ix,iy,iz, A);
}

float UThermoForgeFieldAsset::SampleWallPerm01(const FVector& WorldPos) const
{
    int32 ix,iy,iz; FVector A;
    if (!WorldToCellTrilinear(WorldPos, ix,iy,iz, A)) return 1.f;
    return TF_TrilinearFetch(WallPermeability01, Dim, ix,iy,iz, A);
}

float UThermoForgeFieldAsset::SampleIndoorness01(const FVector& WorldPos) const
{
    int32 ix,iy,iz; FVector A;
    if (!WorldToCellTrilinear(WorldPos, ix,iy,iz, A)) return 0.f;
    return TF_TrilinearFetch(Indoorness01, Dim, ix,iy,iz, A);
}

float UThermoForgeFieldAsset::GetSkyViewByLinearIdx(int32 Linear) const
{
    const int32 Expect = (Dim.X>0 && Dim.Y>0 && Dim.Z>0) ? (Dim.X*Dim.Y*Dim.Z) : 0;
    if (Linear < 0 || Linear >= Expect) return 0.f;
    return SkyView01.IsValidIndex(Linear) ? SkyView01[Linear] : 0.f;
}

float UThermoForgeFieldAsset::GetWallPermByLinearIdx(int32 Linear) const
{
    const int32 Expect = (Dim.X>0 && Dim.Y>0 && Dim.Z>0) ? (Dim.X*Dim.Y*Dim.Z) : 0;
    if (Linear < 0 || Linear >= Expect) return 1.f;
    return WallPermeability01.IsValidIndex(Linear) ? WallPermeability01[Linear] : 1.f;
}

float UThermoForgeFieldAsset::GetIndoorByLinearIdx(int32 Linear) const
{
    const int32 Expect = (Dim.X>0 && Dim.Y>0 && Dim.Z>0) ? (Dim.X*Dim.Y*Dim.Z) : 0;
    if (Linear < 0 || Linear >= Expect) return 0.f;
    return Indoorness01.IsValidIndex(Linear) ? Indoorness01[Linear] : 0.f;
}

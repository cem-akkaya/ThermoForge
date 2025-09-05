#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ThermoForgeFieldAsset.generated.h"

/**
 * Geometry-invariant bake per volume.
 * Channels:
 *  - SkyView01         (0..1) openness to sky
 *  - WallPermeability01(0..1) average permeability to 6 axis neighbors
 *  - Indoorness01      (0..1) indoor proxy = (1 - SkyView01) * (1 - WallPermeability01)
 */
UCLASS(BlueprintType)
class THERMOFORGE_API UThermoForgeFieldAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UThermoForgeFieldAsset();
    
    /** Grid metadata */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Field")
    FIntVector Dim = FIntVector::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Field", meta=(Units="cm"))
    float CellSizeCm = 100.f;

    /** Grid origin in world space (cell (0,0,0) corner) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Field")
    FVector OriginWS = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Field")
    FRotator GridRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, Category="ThermoForge")
    FTransform GridFrameWS = FTransform::Identity;
    
    /** Baked channels */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Field", meta=(ToolTip="Openness to sky, 0..1"))
    TArray<float> SkyView01;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Field", meta=(ToolTip="Avg permeability toward 6 axis neighbors, 0..1"))
    TArray<float> WallPermeability01;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Field", meta=(ToolTip="Indoor proxy = (1 - SkyView01) * (1 - WallPermeability01)"))
    TArray<float> Indoorness01;

    FORCEINLINE int32 Index(int32 x, int32 y, int32 z) const { return (z * Dim.Y + y) * Dim.X + x; }

    /** Trilinear; returns false if outside grid. */
    bool WorldToCellTrilinear(const FVector& P, int32& ix, int32& iy, int32& iz, FVector& Alpha) const;

    /** Trilinear sample helpers */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThermoForge|Field")
    float SampleSkyView01(const FVector& WorldPos) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThermoForge|Field")
    float SampleWallPerm01(const FVector& WorldPos) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThermoForge|Field")
    float SampleIndoorness01(const FVector& WorldPos) const;

    /** Safe linear-index fetchers */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThermoForge|Field")
    float GetSkyViewByLinearIdx(int32 Linear) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThermoForge|Field")
    float GetWallPermByLinearIdx(int32 Linear) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThermoForge|Field")
    float GetIndoorByLinearIdx(int32 Linear) const;

    FORCEINLINE FTransform GetGridFrame() const
    {
        return FTransform(GridRotation, OriginWS, FVector::OneVector);
    }
};

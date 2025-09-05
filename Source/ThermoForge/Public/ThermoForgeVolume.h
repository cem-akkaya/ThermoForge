#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h" 
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

#include "ThermoForgeVolume.generated.h"

class UThermoForgeFieldAsset;

UENUM(BlueprintType)
enum class EThermoGridOriginMode : uint8
{
    WorldZero   UMETA(DisplayName="World Zero"),
    ActorOrigin UMETA(DisplayName="Actor Origin"),
    Custom      UMETA(DisplayName="Custom (WS)")
};

UENUM(BlueprintType)
enum class EThermoGridOrientationMode : uint8
{
    WorldAxes     UMETA(DisplayName="World Axes (default)"),
    ActorRotation UMETA(DisplayName="Actor Rotation")
};

UCLASS(HideCategories=(Collision, Input, HLOD, Cooking, Replication, Rendering, Actor, LOD))
class THERMOFORGE_API AThermoForgeVolume : public AVolume
{
    GENERATED_BODY()

public:
    AThermoForgeVolume();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, Blueprintable, Category="A Thermo Forge Volume|Field")
    TSoftObjectPtr<UThermoForgeFieldAsset> BakedFieldRef;

    UFUNCTION(BlueprintCallable, Category="A Thermo Forge Volume|Field")
    void SetBakedField(UThermoForgeFieldAsset* Asset);

    UFUNCTION(BlueprintCallable, Category="Thermo Forge Volume| Settings")
    void SetVolumeParameters(
        const FVector& InBoxExtent,
        bool bInUseGlobalGrid,
        float InGridCellSize,
        EThermoGridOriginMode InGridOriginMode,
        const FVector& InGridOriginWS,
        bool bInShowGridPreview,
        bool bInAutoRebuildPreview,
        float InGridCellGap,
        int32 InMaxPreviewInstances);

    UPROPERTY(EditAnywhere, Category="A Thermo Forge Volume")
    FVector BoxExtent = FVector(500.f);

    UPROPERTY(EditAnywhere, Category="A Thermo Forge Volume")
    bool bUnbounded = false;

    // -------- Grid --------
    UPROPERTY(EditAnywhere, Blueprintable, Category="A Thermo Forge Volume|Grid")
    bool bUseGlobalGrid = true;

    UPROPERTY(EditAnywhere, Blueprintable, Category="A Thermo Forge Volume|Grid", meta=(EditCondition="!bUseGlobalGrid", ClampMin="10.0", UIMin="10.0"))
    float GridCellSize = 250.f;

    UPROPERTY(EditAnywhere, Blueprintable, Category="A Thermo Forge Volume|Grid")
    EThermoGridOriginMode GridOriginMode = EThermoGridOriginMode::WorldZero;

    UPROPERTY(EditAnywhere, Category="A Thermo Forge Volume|Grid")
    EThermoGridOrientationMode GridOrientationMode = EThermoGridOrientationMode::WorldAxes;

    UPROPERTY(EditAnywhere, Blueprintable, Category="A Thermo Forge Volume|Grid", meta=(EditCondition="GridOriginMode==EThermoGridOriginMode::Custom"))
    FVector GridOriginWS = FVector::ZeroVector;

    // -------- Preview (declared for all builds; used mainly in editor) --------
    UPROPERTY(EditAnywhere, Blueprintable, Category="A Thermo Forge Volume|Preview")
    bool bAutoRebuildPreview = false;

    UPROPERTY(EditAnywhere, Blueprintable, Category="A Thermo Forge Volume", meta=(ClampMin="0.0"))
    float GridCellGap = 25.f;

    UPROPERTY(EditAnywhere, Blueprintable, Category="A Thermo Forge Volume", meta=(ClampMin="1"))
    int32 MaxPreviewInstances = 100000;

    UPROPERTY(EditAnywhere, Category="A Thermo Forge Volume|Preview")
    UMaterialInterface* GridPreviewMaterial = nullptr;

    UPROPERTY(Transient)
    UInstancedStaticMeshComponent* GridPreviewISM = nullptr;

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* HeatPreviewMID = nullptr;

    // -------- Baked data --------
    UPROPERTY(EditAnywhere, Category="A Thermo Forge Volume|Field")
    UThermoForgeFieldAsset* BakedField = nullptr;

    // -------- Runtime helpers --------
    FBox        GetWorldBounds() const;
    float       GetEffectiveCellSize() const;
    FVector     GetEffectiveGridOrigin() const;
    FTransform  GetGridFrame() const;

    // -------- Preview API (keep declared in all builds to match .cpp) --------
    UFUNCTION(BlueprintCallable, Category="A Thermo Forge Volume|Preview")
    void RebuildPreviewGrid();

    UFUNCTION(BlueprintCallable, Category="A Thermo Forge Volume|Preview")
    void HidePreview();

    UFUNCTION(BlueprintCallable, Category="A Thermo Forge Volume|Preview")
    void BuildHeatPreviewFromField();

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    UPROPERTY(VisibleAnywhere, Category="A Thermo Forge Volume")
    UBoxComponent* Bounds = nullptr;

    // Helpers used by preview; declared for all builds (definitions may be editor-guarded)
    void        ApplyBasePreviewMaterialIfNeeded();
    void        ApplyHeatMaterialIfPossible();
    static float ClampVisualGap(float Cell, float Gap);
};

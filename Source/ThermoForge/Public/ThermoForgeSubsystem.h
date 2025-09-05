#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ThermoForgeSubsystem.generated.h"

class UThermoForgeSourceComponent;
class AThermoForgeVolume;
class UThermoForgeFieldAsset;
class UThermoForgeProjectSettings;

// ---------- RESULT STRUCT ----------
USTRUCT(BlueprintType)
struct FThermoForgeGridHit
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="ThermoForge")
    bool bFound = false;

    UPROPERTY(BlueprintReadOnly, Category="ThermoForge")
    TObjectPtr<AThermoForgeVolume> Volume = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="ThermoForge")
    FIntVector GridIndex = FIntVector::ZeroValue;

    UPROPERTY(BlueprintReadOnly, Category="ThermoForge")
    int32 LinearIndex = -1;

    UPROPERTY(BlueprintReadOnly, Category="ThermoForge")
    FVector CellCenterWS = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="ThermoForge")
    double DistanceSq = TNumericLimits<double>::Max();

    UPROPERTY(BlueprintReadOnly, Category="ThermoForge")
    float CellSizeCm = 0.f;

    UPROPERTY(BlueprintReadOnly, Category="ThermoForge")
    FDateTime QueryTimeUTC = FDateTime(0);

    /** Composed current temperature at that cell (°C). */
    UPROPERTY(BlueprintReadOnly, Category="ThermoForge")
    float CurrentTempC = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FThermoSourcesChanged);

UCLASS()
class THERMOFORGE_API UThermoForgeSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    // lifecycle
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // sources
    void RegisterSource(UThermoForgeSourceComponent* Source);
    void UnregisterSource(UThermoForgeSourceComponent* Source);
    void MarkSourceDirty(UThermoForgeSourceComponent* Source);
    int32 GetSourceCount() const;
    void GetAllSources(TArray<UThermoForgeSourceComponent*>& OutSources) const;

    UPROPERTY(BlueprintAssignable, Category="Thermo Forge")
    FThermoSourcesChanged OnSourcesChanged;

    // --------- Geometry-only bake ----------
    UFUNCTION(BlueprintCallable, Category="Thermo Forge")
    void KickstartSamplingFromVolumes();

    /** Occlusion between two points (0..1, 1=open) using physmat density + Beer–Lambert. */
    float OcclusionBetween(const FVector& A, const FVector& B, float CellSizeCm) const;

    // --------- Queries / Composition ----------
    /** Compose current temperature (°C) at world position using baked geometry + runtime climate + dynamic sources. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Thermo Forge|Query")
    float ComputeCurrentTemperatureAt(const FVector& WorldPos, bool bWinter, float TimeHours, float WeatherAlpha01) const;

    /** Find nearest baked grid point; also fills CurrentTempC using default preview knobs. */
    UFUNCTION(BlueprintCallable, Category="ThermoForge|Query")
    FThermoForgeGridHit QueryNearestBakedGridPoint(const FVector& WorldLocation, const FDateTime& QueryTimeUTC) const;

    UFUNCTION(BlueprintCallable, Category="ThermoForge|Query")
    FThermoForgeGridHit QueryNearestBakedGridPointNow(const FVector& WorldLocation) const;

private:
    // helpers
    float TraceAmbientRay01(const FVector& P, const FVector& Dir, float MaxLen) const;

    static void TF_DumpFieldToSavedFolder(const FString& VolName,
        const FIntVector& Dim, float Cell, const FVector& OriginWS,
        const TArray<float>& SkyView01, const TArray<float>& WallPerm01, const TArray<float>& Indoor01);

    bool ComputeNearestInVolume(const AThermoForgeVolume* Vol, const FVector& WorldLocation, FThermoForgeGridHit& OutHit) const;
    bool VolumeContainsPoint(const AThermoForgeVolume* Vol, const FVector& WorldLocation) const;

#if WITH_EDITOR
    UThermoForgeFieldAsset* CreateAndSaveFieldAsset(AThermoForgeVolume* Volume, const FIntVector& Dim, float Cell, const FVector& FieldOriginWS, const FRotator& GridRotation,
                                                    const TArray<float>& SkyView01, const TArray<float>& WallPerm01, const TArray<float>& Indoor01) const;
#endif

    void CompactSources();
    const UThermoForgeProjectSettings* GetSettings() const;

    // data
    TSet<TWeakObjectPtr<UThermoForgeSourceComponent>> SourceSet;
};

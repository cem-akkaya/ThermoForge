#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h" // ECollisionChannel
#include "ThermoForgeProjectSettings.generated.h"

/**
 * Project-wide Thermo Forge settings.
 * Bake is geometry-only (sky view / wall permeability),
 * runtime composes current temperature from these + climate + sources.
 */
UCLASS(Config=Game, DefaultConfig, DisplayName="Thermo Forge")
class THERMOFORGE_API UThermoForgeProjectSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UThermoForgeProjectSettings();

    // ======== CLIMATE ========
    /** Mean winter temperature at sea level (°C). */
    UPROPERTY(EditAnywhere, Config, Category="Climate", meta=(ClampMin="-100", ClampMax="100"))
    float WinterAverageC = 5.f;

    /** Winter day-night swing (peak-to-trough) in °C. */
    UPROPERTY(EditAnywhere, Config, Category="Climate", meta=(ClampMin="0", ClampMax="60"))
    float WinterDayNightDeltaC = 8.f;

    /** Mean summer temperature at sea level (°C). */
    UPROPERTY(EditAnywhere, Config, Category="Climate", meta=(ClampMin="-100", ClampMax="100"))
    float SummerAverageC = 28.f;

    /** Summer day-night swing (peak-to-trough) in °C. */
    UPROPERTY(EditAnywhere, Config, Category="Climate", meta=(ClampMin="0", ClampMax="60"))
    float SummerDayNightDeltaC = 10.f;

    /** Default weather factor (0 clear … 1 overcast). */
    UPROPERTY(EditAnywhere, Config, Category="Climate", meta=(ClampMin="0", ClampMax="1"))
    float DefaultWeatherAlpha01 = 0.3f;

    /** °C contribution for full sun at sea level (before weather scaling). */
    UPROPERTY(EditAnywhere, Config, Category="Climate", meta=(ClampMin="0", ClampMax="50", ToolTip="How many °C does full sun add at SkyView=1, Weather=0"))
    float SolarGainScaleC = 6.f;

    // ======== ALTITUDE ========
    /** Apply environmental lapse rate with altitude. */
    UPROPERTY(EditAnywhere, Config, Category="Climate|Altitude")
    bool bEnableAltitudeLapse = true;

    /** Sea level height in world centimeters (Z). */
    UPROPERTY(EditAnywhere, Config, Category="Climate|Altitude", meta=(Units="cm"))
    float SeaLevelZcm = 0.f;

    /** °C decrease per kilometer of altitude. */
    UPROPERTY(EditAnywhere, Config, Category="Climate|Altitude", meta=(ClampMin="0.0", ClampMax="40.0"))
    float LapseRateCPerKm = 10.f;

    // ======== PERMEABILITY / OCCLUSION ========
    /** If true, read density from Physical Materials; otherwise use defaults. */
    UPROPERTY(EditAnywhere, Config, Category="Permeability")
    bool bUsePhysicsMaterialForDensity = true;

    /** Treat missing PhysMat as air (true) or unknown (false). */
    UPROPERTY(EditAnywhere, Config, Category="Permeability")
    bool bTreatMissingPhysMatAsAir = true;

    /** Density of air (kg/m^3). */
    UPROPERTY(EditAnywhere, Config, Category="Permeability", meta=(ClampMin="0.5", ClampMax="5"))
    float AirDensityKgM3 = 1.2f;

    /** Upper bound density for solids (kg/m^3). */
    UPROPERTY(EditAnywhere, Config, Category="Permeability", meta=(ClampMin="1000", ClampMax="20000"))
    float MaxSolidDensityKgM3 = 3000.f;

    /** Density to assume when PhysMat is missing and not treated as air. */
    UPROPERTY(EditAnywhere, Config, Category="Permeability", meta=(ClampMin="100", ClampMax="5000"))
    float UnknownHitDensityKgM3 = 700.f;

    /** Absorption strength in Beer–Lambert mapping. */
    UPROPERTY(EditAnywhere, Config, Category="Permeability", meta=(ClampMin="0.1", ClampMax="20"))
    float AbsorptionBeta = 8.f;

    /** Effective thickness multiplier (cell fractions) when computing attenuation. */
    UPROPERTY(EditAnywhere, Config, Category="Permeability", meta=(ClampMin="0.1", ClampMax="10"))
    float FaceThicknessFactor = 0.6f;

    /** Trace channel for occlusion tests. */
    UPROPERTY(EditAnywhere, Config, Category="Permeability")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    /** Use complex tracing. */
    UPROPERTY(EditAnywhere, Config, Category="Permeability")
    bool bTraceComplex = true;

    /** Clamp min/max permeability after mapping. */
    UPROPERTY(EditAnywhere, Config, Category="Permeability", meta=(ClampMin="0", ClampMax="1"))
    float MinPermeabilityClamp = 0.f;

    UPROPERTY(EditAnywhere, Config, Category="Permeability", meta=(ClampMin="0", ClampMax="1"))
    float MaxPermeabilityClamp = 1.f;

    // ======== GRID DEFAULTS ========
    /** Default cell size (cm) for volumes using global grid. */
    UPROPERTY(EditAnywhere, Config, Category="Grid", meta=(ClampMin="10", ClampMax="1000", Units="cm"))
    int32 DefaultCellSizeCm = 250;

    /** Default tile dimensions (safety/unused; left for future tiling). */
    UPROPERTY(EditAnywhere, Config, Category="Grid")
    FIntVector DefaultTileDim = FIntVector(128,128,64);

    /** Guard cells around volume bounds (reserved for future diffusion). */
    UPROPERTY(EditAnywhere, Config, Category="Grid", meta=(ClampMin="0", ClampMax="3"))
    int32 GuardCells = 1;

    // ======== PREVIEW (editor-time defaults for runtime composition) ========
    /** Time of day used for preview temperature composition (hours). */
    UPROPERTY(EditAnywhere, Config, Category="Preview", meta=(ClampMin="0", ClampMax="24"))
    float PreviewTimeOfDayHours = 15.f;

    /** Season toggle used for preview temperature composition. */
    UPROPERTY(EditAnywhere, Config, Category="Preview")
    bool PreviewSeasonIsWinter = false;

    /** Weather factor used for preview temperature composition (0 clear … 1 overcast). */
    UPROPERTY(EditAnywhere, Config, Category="Preview", meta=(ClampMin="0", ClampMax="1"))
    float PreviewWeatherAlpha = 0.3f;

    // ======== Helpers ========
    /** Diurnal ambient at sea level (°C). */
    UFUNCTION(BlueprintPure, Category="Thermo Forge")
    float GetAmbientCelsius(bool bWinter, float TimeOfDayHours) const;

    /** Apply altitude lapse to a base temperature (°C). */
    UFUNCTION(BlueprintPure, Category="Thermo Forge")
    float AdjustForAltitude(float BaseCelsius, float WorldZcm) const;

    /** Convenience: ambient at world Z (°C). */
    UFUNCTION(BlueprintPure, Category="Thermo Forge")
    float GetAmbientCelsiusAt(bool bWinter, float TimeOfDayHours, float WorldZcm) const;

    /** Map material density & path thickness to permeability [0..1]. */
    UFUNCTION(BlueprintPure, Category="Thermo Forge")
    float DensityToPermeability(float DensityKgM3, float ThicknessFraction) const;
};

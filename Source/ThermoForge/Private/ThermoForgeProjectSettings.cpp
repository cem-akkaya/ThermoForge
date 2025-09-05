#include "ThermoForgeProjectSettings.h"
#include "Math/UnrealMathUtility.h"

UThermoForgeProjectSettings::UThermoForgeProjectSettings()
{
}

static FORCEINLINE float TF_Diurnal(float Avg, float Delta, float TimeOfDayHours)
{
    // Cosine curve: warmest ~15:00, coolest ~03:00
    const float ClampedH = FMath::Clamp(TimeOfDayHours, 0.0f, 24.0f);
    const float Phase    = (ClampedH - 15.0f) / 24.0f;   // shift so peak at 15:00
    const float Osc      = FMath::Cos(2.0f * PI * Phase);// -1..+1
    return Avg + 0.5f * Delta * Osc;
}

float UThermoForgeProjectSettings::GetAmbientCelsius(bool bWinter, float TimeOfDayHours) const
{
    const float Avg   = bWinter ? WinterAverageC       : SummerAverageC;
    const float Delta = bWinter ? WinterDayNightDeltaC : SummerDayNightDeltaC;
    return TF_Diurnal(Avg, Delta, TimeOfDayHours);
}

float UThermoForgeProjectSettings::AdjustForAltitude(float BaseCelsius, float WorldZcm) const
{
    if (!bEnableAltitudeLapse || LapseRateCPerKm <= 0.0f)
    {
        return BaseCelsius;
    }
    // Convert world Z (cm) to kilometers
    const float AltitudeKm = (WorldZcm - SeaLevelZcm) / 100000.0f;
    return BaseCelsius - LapseRateCPerKm * AltitudeKm;
}

float UThermoForgeProjectSettings::GetAmbientCelsiusAt(bool bWinter, float TimeOfDayHours, float WorldZcm) const
{
    return AdjustForAltitude(GetAmbientCelsius(bWinter, TimeOfDayHours), WorldZcm);
}

float UThermoForgeProjectSettings::DensityToPermeability(float DensityKgM3, float ThicknessFraction) const
{
    // Normalize density to [0..1] within [air .. max solid]
    const float DenMin  = FMath::Min(AirDensityKgM3, MaxSolidDensityKgM3);
    const float DenMax  = FMath::Max(AirDensityKgM3, MaxSolidDensityKgM3);
    const float RhoNorm = FMath::Clamp((DensityKgM3 - DenMin) / FMath::Max(1e-6f, DenMax - DenMin), 0.0f, 1.0f);

    // Beer–Lambert attenuation: exp(-beta * rho_norm * L)
    const float L = FMath::Max(0.0f, ThicknessFraction);
    float P = FMath::Exp(-AbsorptionBeta * RhoNorm * L);

    // Clamp to user range
    P = FMath::Clamp(P, MinPermeabilityClamp, MaxPermeabilityClamp);
    return P;
}

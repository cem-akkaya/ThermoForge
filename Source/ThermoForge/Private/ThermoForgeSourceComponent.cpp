#include "ThermoForgeSourceComponent.h"

#include "ThermoForgeSubsystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UThermoForgeSourceComponent::UThermoForgeSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

FTransform UThermoForgeSourceComponent::GetOwnerTransformSafe() const
{
    if (const AActor* A = GetOwner()) return A->GetActorTransform();
    return FTransform::Identity;
}

FVector UThermoForgeSourceComponent::GetOwnerLocationSafe() const
{
    if (const AActor* A = GetOwner()) return A->GetActorLocation();
    return FVector::ZeroVector;
}

static float PointFalloffWeight(EThermoSourceFalloff F, float Distance, float Radius)
{
    if (Radius <= KINDA_SMALL_NUMBER) return 0.f;
    if (Distance >= Radius) return 0.f;

    switch (F)
    {
        case EThermoSourceFalloff::None:   return 1.f;
        case EThermoSourceFalloff::Linear: { const float x = Distance / Radius; return 1.f - x; }
        case EThermoSourceFalloff::InverseSquare:
        default:
        {
            const float x = Distance / Radius;
            return 1.f / (1.f + x * x);
        }
    }
}

FBox UThermoForgeSourceComponent::GetBoundsWS() const
{
    const FTransform T = GetOwnerTransformSafe();
    const FVector    L = GetOwnerLocationSafe();
    const float scale = bAffectByOwnerScale ? T.GetMaximumAxisScale() : 1.f;

    if (Shape == EThermoSourceShape::Point)
    {
        return FBox::BuildAABB(L, FVector(RadiusCm * scale));
    }
    else
    {
        const FVector Ext = bAffectByOwnerScale ? (BoxExtent * scale) : BoxExtent;
        const FBox Local(-Ext, Ext);
        return Local.TransformBy(T);
    }
}

float UThermoForgeSourceComponent::SampleAt(const FVector& P) const
{
    if (!bEnabled) return 0.f;

    const FTransform T = GetOwnerTransformSafe();
    const FVector    L = T.GetLocation();
    const float      scale = bAffectByOwnerScale ? T.GetMaximumAxisScale() : 1.f;

    if (Shape == EThermoSourceShape::Point)
    {
        const float R = RadiusCm * scale;
        const float d = FVector::Distance(P, L);
        const float w = PointFalloffWeight(Falloff, d, R);
        return IntensityCelsius * w;
    }
    else
    {
        const FVector Ext = bAffectByOwnerScale ? (BoxExtent * scale) : BoxExtent;
        const FVector LocalP = T.InverseTransformPosition(P);
        const FVector Min = -Ext, Max = Ext;

        const bool bInside =
            (LocalP.X >= Min.X && LocalP.X <= Max.X) &&
            (LocalP.Y >= Min.Y && LocalP.Y <= Max.Y) &&
            (LocalP.Z >= Min.Z && LocalP.Z <= Max.Z);

        return bInside ? IntensityCelsius : 0.f;
    }
}

void UThermoForgeSourceComponent::OnRegister()
{
    Super::OnRegister();
    if (UWorld* W = GetWorld())
        if (auto* SS = W->GetSubsystem<UThermoForgeSubsystem>())
            SS->RegisterSource(this);
}

void UThermoForgeSourceComponent::OnUnregister()
{
    if (UWorld* W = GetWorld())
        if (auto* SS = W->GetSubsystem<UThermoForgeSubsystem>())
            SS->UnregisterSource(this);
    Super::OnUnregister();
}

#if WITH_EDITOR
void UThermoForgeSourceComponent::PostEditChangeProperty(FPropertyChangedEvent& E)
{
    Super::PostEditChangeProperty(E);
    // Optional: notify previews
}
#endif

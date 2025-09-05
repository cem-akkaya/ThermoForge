#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThermoForgeSourceComponent.generated.h"

UENUM(BlueprintType)
enum class EThermoSourceShape : uint8
{
    Point UMETA(DisplayName="Point (Sphere around Owner)"),
    Box   UMETA(DisplayName="Box (Oriented by Owner)")
};

UENUM(BlueprintType)
enum class EThermoSourceFalloff : uint8
{
    None          UMETA(DisplayName="None"),
    Linear        UMETA(DisplayName="Linear (1 - d/R)"),
    InverseSquare UMETA(DisplayName="Inverse Square (1 / (1 + (d/R)^2))")
};

UCLASS(ClassGroup=(ThermoForge), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class THERMOFORGE_API UThermoForgeSourceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UThermoForgeSourceComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thermo Source")
    bool bEnabled = true;

    /** Signed delta in °C at the source center (hot +, cold -). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thermo Source", meta=(ClampMin="-1000.0", ClampMax="1000.0"))
    float IntensityCelsius = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thermo Source")
    EThermoSourceShape Shape = EThermoSourceShape::Point;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thermo Source|Point", meta=(EditCondition="Shape==EThermoSourceShape::Point", ClampMin="0.0", Units="cm"))
    float RadiusCm = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thermo Source|Point", meta=(EditCondition="Shape==EThermoSourceShape::Point"))
    EThermoSourceFalloff Falloff = EThermoSourceFalloff::Linear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thermo Source|Box", meta=(EditCondition="Shape==EThermoSourceShape::Box", Units="cm"))
    FVector BoxExtent = FVector(200.f, 200.f, 200.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thermo Source")
    bool bAffectByOwnerScale = false;

    UFUNCTION(BlueprintCallable, Category="Thermo Source")
    FBox GetBoundsWS() const;

    UFUNCTION(BlueprintCallable, Category="Thermo Source")
    float SampleAt(const FVector& WorldPos) const;

    UFUNCTION(BlueprintPure, Category="Thermo Source")
    FTransform GetOwnerTransformSafe() const;

    UFUNCTION(BlueprintPure, Category="Thermo Source")
    FVector GetOwnerLocationSafe() const;

protected:
    virtual void OnRegister() override;
    virtual void OnUnregister() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& E) override;
#endif
};

#include "ThermoForgeVolume.h"

#include "ThermoForgeFieldAsset.h"
#include "ThermoForgeProjectSettings.h"
#include "ThermoForgeSubsystem.h"

#include "Components/BoxComponent.h"
#include "Engine/LevelBounds.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogThermoForgeVolume, Log, All);

AThermoForgeVolume::AThermoForgeVolume()
{
    bColored   = true;
    BrushColor = FColor(255, 128, 0);

    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetGenerateOverlapEvents(false);
    Bounds->bHiddenInGame = true;
    Bounds->SetMobility(EComponentMobility::Static);

#if WITH_EDITORONLY_DATA
    GridPreviewISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GridPreviewISM"));
    GridPreviewISM->SetupAttachment(RootComponent);
    GridPreviewISM->SetIsVisualizationComponent(true);
    GridPreviewISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GridPreviewISM->SetGenerateOverlapEvents(false);
    GridPreviewISM->bHiddenInGame = true;
    GridPreviewISM->SetMobility(EComponentMobility::Static);
    GridPreviewISM->SetCastShadow(false);
    GridPreviewISM->SetReceivesDecals(false);
    GridPreviewISM->bSelectable = false;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
        GridPreviewISM->SetStaticMesh(CubeMesh.Object);

    if (!GridPreviewMaterial)
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> TransMat(
            TEXT("Material'/ThermoForge/ThermoForgeDispersion_M.ThermoForgeDispersion_M'"));
        if (TransMat.Succeeded())
            GridPreviewMaterial = TransMat.Object;
    }
    if (GridPreviewMaterial)
        GridPreviewISM->SetMaterial(0, GridPreviewMaterial);
#endif

    Bounds->InitBoxExtent(BoxExtent);
}
void AThermoForgeVolume::BeginPlay()
{
    Super::BeginPlay();

    if (!BakedField)
    {
        if (BakedFieldRef.IsValid())
        {
            BakedField = BakedFieldRef.Get();
        }
        else if (BakedFieldRef.ToSoftObjectPath().IsValid())
        {
            BakedField = BakedFieldRef.LoadSynchronous();
        }
    }
}

void AThermoForgeVolume::SetBakedField(UThermoForgeFieldAsset* Asset)
{
#if WITH_EDITOR
    Modify();
#endif
    BakedField    = Asset;
    BakedFieldRef = Asset;  // ensures asset gets cooked
#if WITH_EDITOR
    MarkPackageDirty();
#endif
}


FBox AThermoForgeVolume::GetWorldBounds() const
{
    if (bUnbounded)
    {
        if (const ULevel* Level = GetLevel())
            if (const ALevelBounds* LB = Level->LevelBoundsActor.Get())
                return LB->GetComponentsBoundingBox(true);
        return FBox(FVector(-1e6f), FVector(1e6f));
    }
    const FTransform T = GetActorTransform();
    return FBox::BuildAABB(T.GetLocation(), BoxExtent);
}

float AThermoForgeVolume::GetEffectiveCellSize() const
{
#if WITH_EDITORONLY_DATA
    if (!bUseGlobalGrid)
        return FMath::Max(10.f, GridCellSize);
#endif
    const UThermoForgeProjectSettings* S = GetDefault<UThermoForgeProjectSettings>();
    return S ? FMath::Max(10.f, float(S->DefaultCellSizeCm)) : 100.f;
}

FVector AThermoForgeVolume::GetEffectiveGridOrigin() const
{
#if WITH_EDITORONLY_DATA
    switch (GridOriginMode)
    {
        case EThermoGridOriginMode::ActorOrigin: return GetActorLocation();
        case EThermoGridOriginMode::Custom:      return GridOriginWS;
        case EThermoGridOriginMode::WorldZero:
        default:                                 return FVector::ZeroVector;
    }
#else
    return FVector::ZeroVector;
#endif
}

#if WITH_EDITOR
static inline int32 FloorDiv(double X, double Step, double Origin)
{ return FMath::FloorToInt((X - Origin) / Step); }
static inline int32 CeilDiv(double X, double Step, double Origin)
{ return FMath::CeilToInt((X - Origin) / Step); }
#endif // WITH_EDITOR

float AThermoForgeVolume::ClampVisualGap(float Cell, float Gap)
{
    const float MaxGap = 0.95f * Cell;
    return FMath::Clamp(Gap, 0.f, MaxGap);
}

FTransform AThermoForgeVolume::GetGridFrame() const
{
    const FVector Origin = GetEffectiveGridOrigin();
    const FRotator R = (GridOrientationMode == EThermoGridOrientationMode::ActorRotation)
        ? GetActorRotation()
        : FRotator::ZeroRotator;
    return FTransform(R, Origin, FVector::OneVector);
}

void AThermoForgeVolume::ApplyBasePreviewMaterialIfNeeded()
{
    if (!GridPreviewISM) return;
    if (GridPreviewMaterial)
    {
        UMaterialInterface* Current = GridPreviewISM->GetMaterial(0);
        if (Current != GridPreviewMaterial && !Cast<UMaterialInstanceDynamic>(Current))
            GridPreviewISM->SetMaterial(0, GridPreviewMaterial);
    }
}

void AThermoForgeVolume::ApplyHeatMaterialIfPossible()
{
    if (!GridPreviewISM) return;

    UMaterialInterface* Parent = GridPreviewMaterial ? GridPreviewMaterial : GridPreviewISM->GetMaterial(0)->GetBaseMaterial();
    if (!Parent) return;

    HeatPreviewMID = GridPreviewISM->CreateDynamicMaterialInstance(0, Parent);
    GridPreviewISM->SetMaterial(0, HeatPreviewMID);
    GridPreviewISM->MarkRenderStateDirty();
}

void AThermoForgeVolume::OnConstruction(const FTransform& Xform)
{
    Super::OnConstruction(Xform);
    Bounds->SetBoxExtent(BoxExtent);

    if (GridPreviewISM) GridPreviewISM->SetVisibility(true, true);

    if (BakedField && BakedField->Dim.X>0 && BakedField->Dim.Y>0 && BakedField->Dim.Z>0)
    {
        if (GridPreviewISM) GridPreviewISM->SetVisibility(true, true);
        BuildHeatPreviewFromField();
        return;
    }

    ApplyBasePreviewMaterialIfNeeded();
    if (bAutoRebuildPreview)
        RebuildPreviewGrid();
}

#if WITH_EDITOR
void AThermoForgeVolume::PostEditChangeProperty(FPropertyChangedEvent& E)
{
    Super::PostEditChangeProperty(E);
    if (!E.Property) return;

    const FName N = E.Property->GetFName();

    if (N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, BoxExtent))
        Bounds->SetBoxExtent(BoxExtent);

    const bool bPreviewRelevant =
        N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, bUnbounded)          ||
        N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, bUseGlobalGrid)      ||
        N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridCellSize)        ||
        N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridCellGap)         ||
        N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridOriginMode)      ||
        N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridOriginWS)        ||
        N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, MaxPreviewInstances) ||
        N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridPreviewMaterial) ||
        N == GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, BakedField);

    if (bPreviewRelevant && GridPreviewISM)
    {
        const bool bShow = !bUnbounded;
        GridPreviewISM->SetVisibility(bShow, true);

        if (BakedField && bShow)
            BuildHeatPreviewFromField();
        else
        {
            HeatPreviewMID = nullptr;
            ApplyBasePreviewMaterialIfNeeded();
            if (bAutoRebuildPreview && bShow)
                RebuildPreviewGrid();
        }
    }
}
#endif

void AThermoForgeVolume::RebuildPreviewGrid()
{
    if (!GridPreviewISM || !GridPreviewISM->GetStaticMesh()) return;

    GridPreviewISM->ClearInstances();
    GridPreviewISM->SetVisibility(true, true);
    GridPreviewISM->NumCustomDataFloats = 1;

    const float Cell = GetEffectiveCellSize();
    if (Cell <= KINDA_SMALL_NUMBER) return;

    const FBox WorldBox = GetWorldBounds();

    // Rotated grid frame (origin + actor rotation if selected)
    const FTransform Frame = GetGridFrame();
    const FTransform InvFrame = Frame.Inverse();

    const float Gap        = ClampVisualGap(Cell, GridCellGap);
    const float VisualEdge = FMath::Max(1.f, Cell - Gap);
    const FVector Scale(VisualEdge / 100.f);
    FTransform Xf(FQuat::Identity, FVector::ZeroVector, Scale);

    // 1) Transform world AABB corners -> grid space to get index bounds in the rotated frame
    FVector C[8] = {
        FVector(WorldBox.Min.X, WorldBox.Min.Y, WorldBox.Min.Z),
        FVector(WorldBox.Min.X, WorldBox.Min.Y, WorldBox.Max.Z),
        FVector(WorldBox.Min.X, WorldBox.Max.Y, WorldBox.Min.Z),
        FVector(WorldBox.Min.X, WorldBox.Max.Y, WorldBox.Max.Z),
        FVector(WorldBox.Max.X, WorldBox.Min.Y, WorldBox.Min.Z),
        FVector(WorldBox.Max.X, WorldBox.Min.Y, WorldBox.Max.Z),
        FVector(WorldBox.Max.X, WorldBox.Max.Y, WorldBox.Min.Z),
        FVector(WorldBox.Max.X, WorldBox.Max.Y, WorldBox.Max.Z),
    };
    FBox GridBox(ForceInit);
    for (int i=0; i<8; ++i)
        GridBox += InvFrame.TransformPosition(C[i]); // world → grid

    auto FloorDiv0 = [](double X, double Step)->int32 { return FMath::FloorToInt(X / Step); };
    auto CeilDiv0  = [](double X, double Step)->int32 { return FMath::CeilToInt (X / Step); };

    const int32 ix0 = FloorDiv0(GridBox.Min.X, Cell);
    const int32 iy0 = FloorDiv0(GridBox.Min.Y, Cell);
    const int32 iz0 = FloorDiv0(GridBox.Min.Z, Cell);
    const int32 ix1 = CeilDiv0 (GridBox.Max.X, Cell) - 1;
    const int32 iy1 = CeilDiv0 (GridBox.Max.Y, Cell) - 1;
    const int32 iz1 = CeilDiv0 (GridBox.Max.Z, Cell) - 1;

    // 2) Oriented‐box “inside” check (uses actor’s rotation, not AABB)
    auto IsInsideOBB = [&](const FVector& PWS)->bool
    {
        const FTransform T = GetActorTransform();
        const FVector L = T.InverseTransformPosition(PWS);
        const FVector Min = -BoxExtent, Max = BoxExtent;
        return (L.X >= Min.X && L.X <= Max.X) &&
               (L.Y >= Min.Y && L.Y <= Max.Y) &&
               (L.Z >= Min.Z && L.Z <= Max.Z);
    };

    int32 Count = 0;
    for (int32 iz = iz0; iz <= iz1 && Count < MaxPreviewInstances; ++iz)
    for (int32 iy = iy0; iy <= iy1 && Count < MaxPreviewInstances; ++iy)
    for (int32 ix = ix0; ix <= ix1 && Count < MaxPreviewInstances; ++ix)
    {
        const FVector CenterLS( (ix + 0.5f) * Cell, (iy + 0.5f) * Cell, (iz + 0.5f) * Cell );
        const FVector CenterWS = Frame.TransformPosition(CenterLS);
        if (!IsInsideOBB(CenterWS)) continue;

        Xf.SetLocation(CenterWS);
        Xf.SetRotation(Frame.GetRotation());

        const int32 InstanceIndex = GridPreviewISM->AddInstance(Xf,true);
        if (InstanceIndex >= 0)
            GridPreviewISM->SetCustomDataValue(InstanceIndex, 0, 0.f, false); // preview color slot
        ++Count;
    }

    GridPreviewISM->MarkRenderStateDirty();
}

void AThermoForgeVolume::HidePreview()
{
    GridPreviewISM->SetVisibility(false, true);
}


void AThermoForgeVolume::BuildHeatPreviewFromField()
{
    if (!GridPreviewISM) return;
    if (!BakedField || BakedField->Dim.X<=0 || BakedField->Dim.Y<=0 || BakedField->Dim.Z<=0) return;

    if (!GridPreviewISM->GetStaticMesh())
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (CubeMesh.Succeeded()) GridPreviewISM->SetStaticMesh(CubeMesh.Object);
    }

    ApplyHeatMaterialIfPossible();

    GridPreviewISM->ClearInstances();
    GridPreviewISM->NumCustomDataFloats = 1; // slot0 = normalized current temp

    const FIntVector D = BakedField->Dim;
    const int32 Nx = D.X, Ny = D.Y, Nz = D.Z;
    const float Cell = BakedField->CellSizeCm;

    // Asset’s oriented frame at bake time
    const FTransform Frame = BakedField->GetGridFrame();

    const float Gap        = ClampVisualGap(Cell, GridCellGap);
    const float VisualEdge = FMath::Max(1.f, Cell - Gap);
    const FVector Scale(VisualEdge / 100.f);
    FTransform Xf(FQuat::Identity, FVector::ZeroVector, Scale);

    auto Index = [&](int32 x,int32 y,int32 z){ return (z*Ny + y)*Nx + x; };

    // Compose temps using subsystem preview knobs
    float TMin = -100.f, TMax = 100.f; 
    TArray<float> Temp; Temp.SetNumZeroed(Nx*Ny*Nz);

    const UThermoForgeProjectSettings* Settings = GetDefault<UThermoForgeProjectSettings>();
    const bool  bWinter     = Settings ? Settings->PreviewSeasonIsWinter : false;
    const float TimeHours   = Settings ? Settings->PreviewTimeOfDayHours : 15.f;
    const float WeatherAlfa = Settings ? Settings->PreviewWeatherAlpha   : 0.3f;

    UThermoForgeSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UThermoForgeSubsystem>() : nullptr;

    for (int32 z=0; z<Nz; ++z)
    for (int32 y=0; y<Ny; ++y)
    for (int32 x=0; x<Nx; ++x)
    {
        const int32 i = Index(x,y,z);
        const FVector CenterLS((x+0.5f)*Cell, (y+0.5f)*Cell, (z+0.5f)*Cell);
        const FVector CenterWS = Frame.TransformPosition(CenterLS); // <-- FIX

        const float T = (Sub)
            ? Sub->ComputeCurrentTemperatureAt(CenterWS, bWinter, TimeHours, WeatherAlfa)
            : 0.f;

        Temp[i] = T;
    }

    const float Range = FMath::Max(1e-6f, TMax - TMin);

    int32 Count = 0;
    for (int32 z=0; z<Nz; ++z)
    for (int32 y=0; y<Ny; ++y)
    for (int32 x=0; x<Nx; ++x)
    {
        if (Count >= MaxPreviewInstances) break;

        const int32 i = Index(x,y,z);
        const float Heat01 = FMath::Clamp((Temp[i] - TMin) / Range, 0.f, 1.f);

        // Place in the baked field’s rotated frame
        const FVector CenterLS( (x + 0.5f)*Cell, (y + 0.5f)*Cell, (z + 0.5f)*Cell );
        const FVector CenterWS = Frame.TransformPosition(CenterLS);

        Xf.SetLocation(CenterWS);
        Xf.SetRotation(Frame.GetRotation());

        const int32 InstanceIndex = GridPreviewISM->AddInstance(Xf,true);
        if (InstanceIndex >= 0)
            GridPreviewISM->SetCustomDataValue(InstanceIndex, 0, Heat01, false);
            GridPreviewISM->SetCustomDataValue(InstanceIndex, 1, 0,     false);

        ++Count;
    }

    GridPreviewISM->MarkRenderStateDirty();
}

void AThermoForgeVolume::SetVolumeParameters(
    const FVector& InBoxExtent,
    bool bInUseGlobalGrid,
    float InGridCellSize,
    EThermoGridOriginMode InGridOriginMode,
    const FVector& InGridOriginWS,
    bool bInShowGridPreview,
    bool bInAutoRebuildPreview,
    float InGridCellGap,
    int32 InMaxPreviewInstances)
{
#if WITH_EDITOR
    Modify();
#endif

    // --- core fields ---
    BoxExtent         = InBoxExtent;
    bUseGlobalGrid    = bInUseGlobalGrid;
    GridCellSize      = InGridCellSize;
    GridOriginMode    = InGridOriginMode;
    GridOriginWS      = InGridOriginWS;
    GridCellGap       = InGridCellGap;
    MaxPreviewInstances = InMaxPreviewInstances;

    // Keep the bounds component in sync with BoxExtent
    if (Bounds)
        Bounds->SetBoxExtent(BoxExtent);

#if WITH_EDITORONLY_DATA
    // Preview toggles live only in editor builds
    bAutoRebuildPreview= bInAutoRebuildPreview;
#endif

#if WITH_EDITOR
    // Refresh preview if requested
    if (BakedField)
    {
        BuildHeatPreviewFromField();
    }
    else if (bAutoRebuildPreview)
    {
        ApplyBasePreviewMaterialIfNeeded();
        RebuildPreviewGrid();
    }

    MarkPackageDirty();
#endif
}


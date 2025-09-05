#include "ThermoForgeSubsystem.h"
#include "ThermoForgeProjectSettings.h"
#include "ThermoForgeFieldAsset.h"
#include "ThermoForgeVolume.h"
#include "ThermoForgeSourceComponent.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"

#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "PackageTools.h"
#endif

// ---- settings access ----
const UThermoForgeProjectSettings* UThermoForgeSubsystem::GetSettings() const
{
    return GetDefault<UThermoForgeProjectSettings>();
}

void UThermoForgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UThermoForgeSubsystem::Deinitialize()
{
    SourceSet.Empty();
    Super::Deinitialize();
}

void UThermoForgeSubsystem::RegisterSource(UThermoForgeSourceComponent* Source)
{
    if (!IsValid(Source)) return;
    SourceSet.Add(Source);
    CompactSources();
    OnSourcesChanged.Broadcast();
}

void UThermoForgeSubsystem::UnregisterSource(UThermoForgeSourceComponent* Source)
{
    if (!Source) return;
    SourceSet.Remove(Source);
    CompactSources();
    OnSourcesChanged.Broadcast();
}

void UThermoForgeSubsystem::MarkSourceDirty(UThermoForgeSourceComponent* /*Source*/)
{
    OnSourcesChanged.Broadcast();
}

int32 UThermoForgeSubsystem::GetSourceCount() const
{
    int32 Count = 0;
    for (const TWeakObjectPtr<UThermoForgeSourceComponent>& W : SourceSet)
        if (W.IsValid()) ++Count;
    return Count;
}

void UThermoForgeSubsystem::GetAllSources(TArray<UThermoForgeSourceComponent*>& OutSources) const
{
    OutSources.Reset();
    for (const TWeakObjectPtr<UThermoForgeSourceComponent>& W : SourceSet)
        if (UThermoForgeSourceComponent* S = W.Get())
            OutSources.Add(S);
}

// ---- physmat helpers ----
static UPhysicalMaterial* TF_ResolvePhysicalMaterial(const FHitResult& Hit)
{
    if (UPhysicalMaterial* PM = Hit.PhysMaterial.Get()) return PM;

    if (UPrimitiveComponent* PC = Hit.GetComponent())
    {
        if (PC->BodyInstance.GetSimplePhysicalMaterial())
            return PC->BodyInstance.GetSimplePhysicalMaterial();
        if (UBodySetup* BS = PC->GetBodySetup())
            if (BS->PhysMaterial)
                return BS->PhysMaterial;
    }
    return nullptr;
}

static float TF_GetHitDensityKgM3(const FHitResult& Hit, const UThermoForgeProjectSettings* S)
{
    if (!S) return 1.f;

    if (S->bUsePhysicsMaterialForDensity)
    {
        if (UPhysicalMaterial* PM = TF_ResolvePhysicalMaterial(Hit))
        {
            const float Found = PM->Density;
            return FMath::Max(0.f, Found);
        }
    }
    return S->bTreatMissingPhysMatAsAir ? S->AirDensityKgM3 : S->UnknownHitDensityKgM3;
}

// ---- single ray permeability (Beer–Lambert on hit) ----
float UThermoForgeSubsystem::TraceAmbientRay01(const FVector& P, const FVector& Dir, float MaxLen) const
{
    const UWorld* W = GetWorld();
    const UThermoForgeProjectSettings* S = GetSettings();
    if (!W || !S) return 1.f;

    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ThermoAmbient), S->bTraceComplex);
    Q.bReturnPhysicalMaterial = true;

    const bool bHit = W->LineTraceSingleByChannel(
        Hit, P, P + Dir * MaxLen,
        static_cast<ECollisionChannel>(S->TraceChannel.GetValue()),
        Q);

    if (!bHit) return 1.f;

    const float rho   = TF_GetHitDensityKgM3(Hit, S);
    const float Lfrac = S->FaceThicknessFactor;
    return S->DensityToPermeability(rho, Lfrac);
}

float UThermoForgeSubsystem::OcclusionBetween(const FVector& A, const FVector& B, float CellSizeCm) const
{
    const UWorld* W = GetWorld();
    const UThermoForgeProjectSettings* S = GetSettings();
    if (!W || !S) return 1.f;

    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ThermoSource), S->bTraceComplex);
    Q.bReturnPhysicalMaterial = true;

    const bool bHit = W->LineTraceSingleByChannel(
        Hit, A, B,
        static_cast<ECollisionChannel>(S->TraceChannel.GetValue()),
        Q);

    if (!bHit) return 1.f; // open

    const float rho   = TF_GetHitDensityKgM3(Hit, S);
    const float Dist  = FVector::Distance(A, B);
    const float Cell  = FMath::Max(1.f, CellSizeCm);
    const float Lfrac = (Dist / Cell) * S->FaceThicknessFactor;

    return S->DensityToPermeability(rho, Lfrac);
}

// ---- main bake: SkyView01 + WallPermeability01 (+ Indoorness01) ----
void UThermoForgeSubsystem::KickstartSamplingFromVolumes()
{
    UWorld* W = GetWorld();
    const UThermoForgeProjectSettings* S = GetSettings();
    if (!W || !S) return;

    // Small, deterministic hemisphere (sky openness)
    TArray<FVector> HemiDirs;
    {
        const FVector base[12] = {
            { 0, 0, 1}, { 0.5, 0, 0.866f}, {-0.5, 0, 0.866f}, {0, 0.5, 0.866f}, {0, -0.5, 0.866f},
            { 0.707f, 0.707f, 0}, {-0.707f, 0.707f, 0}, {0.707f,-0.707f, 0}, {-0.707f,-0.707f, 0},
            { 0.923f, 0, 0.382f}, {-0.923f, 0, 0.382f}, {0, 0.923f, 0.382f}
        };
        HemiDirs.Append(base, UE_ARRAY_COUNT(base));
        for (FVector& d : HemiDirs) d.Normalize();
    }

    auto FloorDiv = [](double X, double Step, double Origin) -> int32
    { return FMath::FloorToInt((X - Origin) / Step); };
    auto CeilDiv = [](double X, double Step, double Origin) -> int32
    { return FMath::CeilToInt((X - Origin) / Step); };

    int32 VolumeCount = 0;
    for (TActorIterator<AThermoForgeVolume> It(W); It; ++It)
    {
        AThermoForgeVolume* V = *It; ++VolumeCount;
        const FTransform Frame = V->GetGridFrame();
        const FTransform InvFrame = Frame.Inverse();
        
        const FBox Bounds = V->GetWorldBounds();
        const float   Cell   = V->GetEffectiveCellSize();
        const FVector Origin = V->GetEffectiveGridOrigin();

        // Transform world AABB corners into grid space (so indices align to the rotated grid)
        FVector Corners[8] = {
            FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z),
            FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.Z),
            FVector(Bounds.Min.X, Bounds.Max.Y, Bounds.Min.Z),
            FVector(Bounds.Min.X, Bounds.Max.Y, Bounds.Max.Z),
            FVector(Bounds.Max.X, Bounds.Min.Y, Bounds.Min.Z),
            FVector(Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Z),
            FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Min.Z),
            FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z),
        };
        FBox GridBox(ForceInit);
        for (int i=0; i<8; ++i)
        {
            GridBox += InvFrame.TransformPosition(Corners[i]); // world → grid (rotate & translate)
        }

        // Index range in grid space
        auto FloorDiv0 = [](double X, double Step)->int32 { return FMath::FloorToInt(X / Step); };
        auto CeilDiv0  = [](double X, double Step)->int32 { return FMath::CeilToInt (X / Step); };

        const int32 ix0 = FloorDiv0(GridBox.Min.X, Cell);
        const int32 iy0 = FloorDiv0(GridBox.Min.Y, Cell);
        const int32 iz0 = FloorDiv0(GridBox.Min.Z, Cell);

        const int32 ix1 = CeilDiv0 (GridBox.Max.X, Cell) - 1;
        const int32 iy1 = CeilDiv0 (GridBox.Max.Y, Cell) - 1;
        const int32 iz1 = CeilDiv0 (GridBox.Max.Z, Cell) - 1;

        const FIntVector Dim(
            FMath::Max(0, ix1 - ix0 + 1),
            FMath::Max(0, iy1 - iy0 + 1),
            FMath::Max(0, iz1 - iz0 + 1)
        );

        const int32 Nx = Dim.X, Ny = Dim.Y, Nz = Dim.Z;
        const int32 N  = Nx * Ny * Nz;
        if (N <= 0) continue;

        // World origin of the [ix0,iy0,iz0] corner via the frame
        const FVector FieldOriginWS = Frame.TransformPosition(FVector(ix0 * Cell, iy0 * Cell, iz0 * Cell));

        auto Index = [&](int32 x,int32 y,int32 z)->int32 { return (z * Ny + y) * Nx + x; };

        // Center of a cell in WORLD space: go LS → WS through the frame
        auto Center = [&](int32 x,int32 y,int32 z)
        {
            const FVector CenterLS(
                (ix0 + x + 0.5f) * Cell,
                (iy0 + y + 0.5f) * Cell,
                (iz0 + z + 0.5f) * Cell
            );
            return Frame.TransformPosition(CenterLS);
        };
        
        TArray<float> Sky;   Sky.SetNumZeroed(N);
        TArray<float> Wall;  Wall.SetNumZeroed(N);
        TArray<float> Indoor;Indoor.SetNumZeroed(N);

        const float RayLen = 100000.f; // 1km

        for (int32 z=0; z<Nz; ++z)
        for (int32 y=0; y<Ny; ++y)
        for (int32 x=0; x<Nx; ++x)
        {
            const int32 idx = Index(x,y,z);
            const FVector P = Center(x,y,z);

            // Sky openness (hemisphere)
            float openness = 0.f;
            for (const FVector& d : HemiDirs)
                openness += TraceAmbientRay01(P, d, RayLen);
            openness /= (float)HemiDirs.Num();
            openness = FMath::Clamp(openness, 0.f, 1.f);
            Sky[idx] = openness;

            // Wall permeability: average occlusion to 6 neighbor centers
            float sumPerm = 0.f; int32 cnt = 0;
            const int32 nx[6] = { x-1, x+1, x,   x,   x,   x   };
            const int32 ny[6] = { y,   y,   y-1, y+1, y,   y   };
            const int32 nz[6] = { z,   z,   z,   z,   z-1, z+1 };

            for (int i=0;i<6;++i)
            {
                const int32 xx = nx[i], yy = ny[i], zz = nz[i];
                if (xx<0 || yy<0 || zz<0 || xx>=Nx || yy>=Ny || zz>=Nz) continue;

                const FVector Q = Center(xx,yy,zz);
                const float perm = OcclusionBetween(P, Q, Cell); // 0..1, uses physmat density
                sumPerm += FMath::Clamp(perm, 0.f, 1.f);
                ++cnt;
            }
            const float wallPerm = (cnt>0) ? (sumPerm / cnt) : 1.f;
            Wall[idx] = wallPerm;

            // Composite indoor proxy
            Indoor[idx] = (1.f - Sky[idx]) * (1.f - Wall[idx]);
        }

    #if WITH_EDITOR
        if (UThermoForgeFieldAsset* Saved = CreateAndSaveFieldAsset(V, Dim, Cell, FieldOriginWS, Frame.Rotator(), Sky, Wall, Indoor))
        {
            V->Modify();
            V->BakedField = Saved;
        #if WITH_EDITORONLY_DATA
            V->GridPreviewISM->SetVisibility(true);
        #endif
            V->BuildHeatPreviewFromField();
            V->MarkPackageDirty();
        }
    #endif
    }

    UE_LOG(LogTemp, Log, TEXT("[ThermoForge] KickstartSamplingFromVolumes (with wall traces): volumes=%d"), VolumeCount);
}
// ---------- Public BP entry: nearest baked cell ----------
bool UThermoForgeSubsystem::VolumeContainsPoint(const AThermoForgeVolume* Vol, const FVector& WorldLocation) const
{
    if (!Vol) return false;
    if (Vol->bUnbounded) return true;

    const FTransform T = Vol->GetActorTransform();
    const FVector L = T.InverseTransformPosition(WorldLocation);
    const FVector Min = -Vol->BoxExtent, Max = Vol->BoxExtent;
    return (L.X >= Min.X && L.X <= Max.X)
        && (L.Y >= Min.Y && L.Y <= Max.Y)
        && (L.Z >= Min.Z && L.Z <= Max.Z);
}

bool UThermoForgeSubsystem::ComputeNearestInVolume(const AThermoForgeVolume* Vol, const FVector& WorldLocation, FThermoForgeGridHit& OutHit) const
{
    if (!Vol || !Vol->BakedField) return false;

    const UThermoForgeFieldAsset* Field = Vol->BakedField;
    const FIntVector D = Field->Dim;
    if (D.X <= 0 || D.Y <= 0 || D.Z <= 0) return false;

    const float Cell = Field->CellSizeCm;
    if (Cell <= 0.f) return false;

    // Use the asset's oriented frame (origin + rotation at bake time)
    const FTransform Frame = Field->GetGridFrame();
    const FTransform InvFrame = Frame.Inverse();

    // Map world → grid-local (in cell units)
    const FVector LocalGrid = InvFrame.TransformPosition(WorldLocation) / Cell;

    // Nearest cell indices in grid space
    const int32 ix = FMath::Clamp(FMath::FloorToInt(LocalGrid.X + 0.5f), 0, D.X - 1);
    const int32 iy = FMath::Clamp(FMath::FloorToInt(LocalGrid.Y + 0.5f), 0, D.Y - 1);
    const int32 iz = FMath::Clamp(FMath::FloorToInt(LocalGrid.Z + 0.5f), 0, D.Z - 1);

    const int32 Nx = D.X, Ny = D.Y;
    const int32 Linear = ix + iy * Nx + iz * Nx * Ny;

    // Reconstruct WORLD cell center via the same frame
    const FVector CellCenterWS = Frame.TransformPosition(
        FVector((ix + 0.5f) * Cell, (iy + 0.5f) * Cell, (iz + 0.5f) * Cell));

    const double DistSq = FVector::DistSquared(CellCenterWS, WorldLocation);

    OutHit.bFound       = true;
    OutHit.Volume       = const_cast<AThermoForgeVolume*>(Vol);
    OutHit.GridIndex    = FIntVector(ix, iy, iz);
    OutHit.LinearIndex  = Linear;
    OutHit.CellCenterWS = CellCenterWS;
    OutHit.DistanceSq   = DistSq;
    OutHit.CellSizeCm   = Cell;
    return true;
}



FThermoForgeGridHit UThermoForgeSubsystem::QueryNearestBakedGridPoint(const FVector& WorldLocation, const FDateTime& QueryTimeUTC) const
{
    FThermoForgeGridHit Best;

    UWorld* World = GetWorld();
    if (!World) return Best;

    bool FoundInContaining = false;

    for (TActorIterator<AThermoForgeVolume> It(World); It; ++It)
    {
        const AThermoForgeVolume* Vol = *It;
        if (!Vol || !Vol->BakedField) continue;

        if (!VolumeContainsPoint(Vol, WorldLocation))
            continue;

        FThermoForgeGridHit Hit;
        if (ComputeNearestInVolume(Vol, WorldLocation, Hit))
        {
            Hit.QueryTimeUTC = QueryTimeUTC;
            if (!FoundInContaining || Hit.DistanceSq < Best.DistanceSq)
            {
                Best = Hit;
                FoundInContaining = true;
            }
        }
    }

    if (!FoundInContaining)
    {
        for (TActorIterator<AThermoForgeVolume> It(World); It; ++It)
        {
            const AThermoForgeVolume* Vol = *It;
            if (!Vol || !Vol->BakedField) continue;

            FThermoForgeGridHit Hit;
            if (ComputeNearestInVolume(Vol, WorldLocation, Hit))
            {
                Hit.QueryTimeUTC = QueryTimeUTC;
                if (!Best.bFound || Hit.DistanceSq < Best.DistanceSq)
                {
                    Best = Hit;
                }
            }
        }
    }

   // Fill composed temperature (derived from QueryTimeUTC) with post-process ambient fix
if (Best.bFound)
{
    const UThermoForgeProjectSettings* S = GetSettings();

    // Weather alpha: keep using the preview knob unless you have a live feed
    const float WeatherAlfa = S ? S->PreviewWeatherAlpha : 0.3f;

    // --- Time of day from UTC (continuous hours) ---
    const double SecUTC   = Best.QueryTimeUTC.GetTimeOfDay().GetTotalSeconds();
    auto Wrap24 = [](float h){ float r = FMath::Fmod(h, 24.f); return (r < 0.f) ? r + 24.f : r; };
    const float TimeHours = ([](float h){ float r = FMath::Fmod(h, 24.f); return (r < 0.f) ? r + 24.f : r; })
                            (static_cast<float>(SecUTC / 3600.0f));

    // --- Smooth seasonal alpha (0 = deep winter, 1 = peak summer), Northern Hemisphere ---
    // Dec 21 (~355) -> 0, Jun 21 (~172) -> 1, smooth cosine over the year
    auto Wrap01 = [](float x){ return x - FMath::FloorToFloat(x); };
    const int32 DOY           = Best.QueryTimeUTC.GetDayOfYear();      // 1..365/366
    const float YearPos       = Wrap01((float(DOY) - 355.0f) / 365.0f); // 0..1 starting at Dec 21
    const float SeasonAlpha01 = 0.5f * (1.0f - FMath::Cos(2.0f * PI * YearPos)); // 0→1→0 yearly

    // --- Compute BASELINE using the SAME season (blend winter/summer) to avoid steps ---
    // We re-use your existing composition path for both seasons and blend:
    const float BaseWinter = ComputeCurrentTemperatureAt(Best.CellCenterWS, /*bWinter=*/true , TimeHours, WeatherAlfa);
    const float BaseSummer = ComputeCurrentTemperatureAt(Best.CellCenterWS, /*bWinter=*/false, TimeHours, WeatherAlfa);
    const float BaselineTotalC = FMath::Lerp(BaseWinter, BaseSummer, SeasonAlpha01);

    // Extract the ambient part used by baseline (also blended), so we can phase-correct it:
    const float AmbWinter = S ? S->GetAmbientCelsiusAt(/*bWinter=*/true , TimeHours, Best.CellCenterWS.Z) : 0.0f;
    const float AmbSummer = S ? S->GetAmbientCelsiusAt(/*bWinter=*/false, TimeHours, Best.CellCenterWS.Z) : 0.0f;
    const float BaselineAmbientC = FMath::Lerp(AmbWinter, AmbSummer, SeasonAlpha01);

    // --- Desired ambient with new phase: coldest at 00:00, hottest at 12:00 (also seasonal blend) ---
    const float WinterAvg   = S ? S->WinterAverageC       : 5.0f;
    const float SummerAvg   = S ? S->SummerAverageC       : 28.0f;
    const float WinterDelta = S ? S->WinterDayNightDeltaC : 8.0f;
    const float SummerDelta = S ? S->SummerDayNightDeltaC : 10.0f;

    const float AvgC   = FMath::Lerp(WinterAvg,   SummerAvg,   SeasonAlpha01);
    const float DeltaC = FMath::Lerp(WinterDelta, SummerDelta, SeasonAlpha01);

    // 00:00 trough, 12:00 peak
    const float Phase   = (TimeHours - 12.0f) / 24.0f;
    const float CosWave = FMath::Cos(2.0f * PI * Phase);
    const float DesiredAmbientSeaC = AvgC + 0.5f * DeltaC * CosWave;

    // Altitude adjustment
    const float DesiredAmbientC = S ? S->AdjustForAltitude(DesiredAmbientSeaC, Best.CellCenterWS.Z)
                                    : DesiredAmbientSeaC;

    // --- Apply phase correction without jumps ---
    const float AmbientDelta = DesiredAmbientC - BaselineAmbientC;
    Best.CurrentTempC = BaselineTotalC + AmbientDelta;

}


    return Best;
}

FThermoForgeGridHit UThermoForgeSubsystem::QueryNearestBakedGridPointNow(const FVector& WorldLocation) const
{
    return QueryNearestBakedGridPoint(WorldLocation, FDateTime::UtcNow());
}

// --------- Runtime composition ---------
float UThermoForgeSubsystem::ComputeCurrentTemperatureAt(const FVector& WorldPos, bool bWinter, float TimeHours, float WeatherAlpha01) const
{
    const UThermoForgeProjectSettings* S = GetSettings();
    if (!S) return 0.f;

    // Find nearest baked field and read both scalars
    float Sky = 0.f;
    float WallPerm = 1.f;

    {
        UWorld* World = GetWorld();
        FThermoForgeGridHit Best;
        if (World)
        {
            for (TActorIterator<AThermoForgeVolume> It(World); It; ++It)
            {
                const AThermoForgeVolume* Vol = *It;
                if (!Vol || !Vol->BakedField) continue;

                FThermoForgeGridHit Hit;
                if (!ComputeNearestInVolume(Vol, WorldPos, Hit)) continue;

                if (!Best.bFound || Hit.DistanceSq < Best.DistanceSq)
                    Best = Hit;
            }
        }

        if (Best.bFound && Best.Volume && Best.Volume->BakedField)
        {
            Sky      = FMath::Clamp(Best.Volume->BakedField->GetSkyViewByLinearIdx(Best.LinearIndex), 0.f, 1.f);
            WallPerm = FMath::Clamp(Best.Volume->BakedField->GetWallPermByLinearIdx(Best.LinearIndex), 0.f, 1.f);
        }
    }

    // Ambient + altitude
    float AmbientC = S->GetAmbientCelsiusAt(bWinter, TimeHours, WorldPos.Z);

    // Solar gain (reduced by weather)
    const float Solar = S->SolarGainScaleC * Sky * (1.f - FMath::Clamp(WeatherAlpha01, 0.f, 1.f));

    // Dynamic sources (attenuated by LOS * local wall permeability)
    float SourceSum = 0.f;
    {
        for (const TWeakObjectPtr<UThermoForgeSourceComponent>& W : SourceSet)
        {
            const UThermoForgeSourceComponent* Sc = W.Get();
            if (!Sc || !Sc->bEnabled) continue;

            const float Intensity = Sc->SampleAt(WorldPos); // °C delta
            if (Intensity == 0.f) continue;

            const float CellSize = S->DefaultCellSizeCm;
            const float Occ = OcclusionBetween(WorldPos, Sc->GetOwnerLocationSafe(), CellSize);
            // WallPerm scales local transmissivity
            SourceSum += Intensity * Occ * WallPerm;
        }
    }

    return AmbientC + Solar + SourceSum;
}

// ---- Save helpers ----
#if WITH_EDITOR
UThermoForgeFieldAsset* UThermoForgeSubsystem::CreateAndSaveFieldAsset(AThermoForgeVolume* Volume,
    const FIntVector& Dim, float Cell, const FVector& FieldOriginWS, const FRotator& GridRotation,
    const TArray<float>& SkyView01, const TArray<float>& WallPerm01, const TArray<float>& Indoor01) const
{
    if (!Volume) return nullptr;

    const FString VolName     = Volume->GetName();
    const FString PackageName = FString::Printf(TEXT("/Game/ThermoForge/Bakes/%s_Field"), *VolName);
    const FString AssetName   = FPackageName::GetLongPackageAssetName(PackageName);

    UPackage* Pkg = CreatePackage(*PackageName);
    Pkg->FullyLoad();

    UThermoForgeFieldAsset* Saved = FindObject<UThermoForgeFieldAsset>(Pkg, *AssetName);
    const bool bIsNew = (Saved == nullptr);
    if (!Saved)
    {
        Saved = NewObject<UThermoForgeFieldAsset>(Pkg, UThermoForgeFieldAsset::StaticClass(),
                                                  *AssetName, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Saved);
    }

    Saved->Dim               = Dim;
    Saved->CellSizeCm        = Cell;
    Saved->OriginWS          = FieldOriginWS;
    Saved->GridRotation = GridRotation;
    Saved->SkyView01         = SkyView01;
    Saved->WallPermeability01= WallPerm01;
    Saved->Indoorness01      = Indoor01;

    Saved->MarkPackageDirty();
    Pkg->MarkPackageDirty();

    const FString Filename = FPackageName::LongPackageNameToFilename(
        PackageName, FPackageName::GetAssetPackageExtension());

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags     = ESaveFlags::SAVE_None;
    SaveArgs.Error         = GWarn;

    const bool bOk = UPackage::SavePackage(Pkg, Saved, *Filename, SaveArgs);
    UE_LOG(LogTemp, Log, TEXT("[ThermoForge] Asset %s : %s"),
           *Filename, bOk ? TEXT("Saved") : TEXT("FAILED"));

    if (!bOk) return nullptr;

    return Saved;
}
#endif

void UThermoForgeSubsystem::TF_DumpFieldToSavedFolder(const FString& VolName, const FIntVector& Dim, float Cell,
    const FVector& OriginWS, const TArray<float>& SkyView01, const TArray<float>& WallPerm01, const TArray<float>& Indoor01)
{
#if WITH_EDITOR
    const FString Dir  = FPaths::ProjectSavedDir() / TEXT("ThermoForge/Bakes");
    IFileManager::Get().MakeDirectory(*Dir, true);

    const FString Fn   = FString::Printf(TEXT("%s/%s_Field_%s.csv"),
                        *Dir, *VolName, *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));

    FString Out;
    Out.Reserve(256 + SkyView01.Num() * 48);
    Out += TEXT("# DimX,DimY,DimZ,CellSizeCm,OriginX,OriginY,OriginZ\n");
    Out += FString::Printf(TEXT("%d,%d,%d,%.6f,%.6f,%.6f,%.6f\n"),
                           Dim.X, Dim.Y, Dim.Z, Cell, OriginWS.X, OriginWS.Y, OriginWS.Z);
    Out += TEXT("index,skyview,wallperm,indoor\n");

    const int32 N = SkyView01.Num();
    for (int32 i=0; i<N; ++i)
    {
        const float A = SkyView01.IsValidIndex(i) ? SkyView01[i] : 0.f;
        const float B = WallPerm01.IsValidIndex(i) ? WallPerm01[i] : 1.f;
        const float C = Indoor01.IsValidIndex(i) ? Indoor01[i] : 0.f;
        Out += FString::Printf(TEXT("%d,%.6f,%.6f,%.6f\n"), i, A, B, C);
    }

#endif
}

void UThermoForgeSubsystem::CompactSources()
{
    for (auto It = SourceSet.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
        else
        {
            const UThermoForgeSourceComponent* S = It->Get();
            if (!S->GetWorld())
            {
                It.RemoveCurrent();
            }
        }
    }
}

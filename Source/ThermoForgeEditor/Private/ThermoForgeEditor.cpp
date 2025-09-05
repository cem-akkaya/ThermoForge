#include "ThermoForgeEditor.h"

#include "ToolMenus.h"

// Optional includes for tab management and Slate UI
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"

// Editor helpers
#include "Editor.h"
#include "Engine/Selection.h"
#include "ScopedTransaction.h"
#include "Engine/World.h"
#include "LevelEditorSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"  

// Thermo Forge types
#include "ThermoForgeVolume.h"
#include "ThermoForgeSourceComponent.h"
#include "ThermoForgeSubsystem.h"
#include "ThermoForgeVolumeCustomization.h"

#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "EngineUtils.h"
#include "ISettingsModule.h"
#include "ThermoForgeProjectSettings.h"
#include "Components/InstancedStaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "FThermoForgeEditorModule"

IMPLEMENT_MODULE(FThermoForgeEditorModule, ThermoForgeEditor)

// ---------------- Helper: build a consistent tool button ----------------
TSharedRef<SWidget> FThermoForgeEditorModule::MakeToolButton(
    const FString& Label,
    const FName& Icon,
    FOnClicked OnClicked)
{
    return SNew(SBox)
        .WidthOverride(220.0f)
        .HeightOverride(40.0f)
        [
            SNew(SButton)
            .OnClicked(OnClicked)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(5,0))
                [
                    SNew(SImage).Image(FAppStyle::GetBrush(Icon))
                ]
                + SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(FMargin(5,0))
                [
                    SNew(STextBlock).Text(FText::FromString(Label))
                ]
            ]
        ];
}

void FThermoForgeEditorModule::StartupModule()
{
    // Example registration for menu or tab extensions
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FThermoForgeEditorModule::RegisterMenus));
    }

    // Register the tab
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        "ThermoForgeTab",
        FOnSpawnTab::CreateRaw(this, &FThermoForgeEditorModule::OnSpawnThermoForgeTab)
    )
        .SetDisplayName(FText::FromString("ThermoForge"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    
    // Volume Customise
    FPropertyEditorModule& PM = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PM.RegisterCustomClassLayout(AThermoForgeVolume::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FThermoForgeVolumeCustomization::MakeInstance));
}

void FThermoForgeEditorModule::ShutdownModule()
{
    // Clean Customisation
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        FPropertyEditorModule& PM = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PM.UnregisterCustomClassLayout(AThermoForgeVolume::StaticClass()->GetFName());
    }
}

TSharedRef<SDockTab> FThermoForgeEditorModule::OnSpawnThermoForgeTab(const FSpawnTabArgs& SpawnTabArgs)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString("ThermoForge"))
        [
            SNew(SBorder)
            .Padding(FMargin(10))
            .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
            [
                SNew(SVerticalBox)

                // Title
                + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString("Thermo Definitions"))
                    .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyleBold"))

                ]

                + SVerticalBox::Slot()
                .Padding(10, 0, 10, 10)
                .AutoHeight()
                .HAlign(HAlign_Fill) 
                [
                    SNew(SBox)
                    .HeightOverride(48.f) 
                    [
                        SNew(SButton)
                        .HAlign(HAlign_Center)
                        .ContentPadding(FMargin(12.f, 10.f)) 
                        .OnClicked(FOnClicked::CreateRaw(this, &FThermoForgeEditorModule::OnOpenSettingsClicked))
                        [
                            SNew(STextBlock)
                            .Justification(ETextJustify::Center)
                            .Text(FText::FromString("Open Thermo Forge Settings"))
                        ]
                    ]
                ]


                // Two-column layout
                + SVerticalBox::Slot()
                .Padding(0)
                .FillHeight(1.0f)
                [
                    SNew(SHorizontalBox)

                    // First Column = Common Control
                    + SHorizontalBox::Slot()
                    .Padding(10)
                    .FillWidth(0.5f)
                    [
                        SNew(SVerticalBox)

                        // Section Header
                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString("Common Control"))
                            .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyleBold"))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            MakeToolButton("Spawn Thermal Volume", "Icons.Plus",
                                FOnClicked::CreateRaw(this, &FThermoForgeEditorModule::OnSpawnThermalVolumeClicked))
                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            MakeToolButton("Add Heat Source to Selection", "Icons.Plus",
                                FOnClicked::CreateRaw(this, &FThermoForgeEditorModule::OnAddHeatSourceClicked))
                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            MakeToolButton("Kickstart Sampling", "Icons.Refresh",
                                FOnClicked::CreateRaw(this, &FThermoForgeEditorModule::OnKickstartSamplingClicked))
                        ]
                    ]

                    // Second Column = Misc Tools
                    + SHorizontalBox::Slot()
                    .Padding(10)
                    .FillWidth(0.5f)
                    [
                        SNew(SVerticalBox)

                        // Section Header
                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString("Level Tools"))
                            .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyleBold"))
                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            MakeToolButton("Show All Previews", "Icons.Visible",
                                FOnClicked::CreateRaw(this, &FThermoForgeEditorModule::OnShowAllPreviewsClicked))
                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            MakeToolButton("Hide All Previews", "Icons.Visible",
                                FOnClicked::CreateRaw(this, &FThermoForgeEditorModule::OnHideAllPreviewsClicked))
                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(5)
                        [
                            MakeToolButton("Set Mesh Insulated", "ClassIcon.PhysicalMaterial",
                                FOnClicked::CreateRaw(this, &FThermoForgeEditorModule::OnSetMeshInsulatedClicked))
                        ]
                    ]
                ]
            ]
        ];
}

// Open the ThermoForge tab
void FThermoForgeEditorModule::OpenThermoForgeTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(FName("ThermoForgeTab"));
}

void FThermoForgeEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // Extend the "Tools" menu in Level Editor
    if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
    {
        FToolMenuSection& Section = Menu->AddSection("ThermoForgeSection", FText::FromString("Thermo Forge"));

        const FSlateIcon ThermoForgeIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.KeyEvent_16x");

        Section.AddMenuEntry(
            "ThermoForgeTab",
            FText::FromString("Thermo Forge"),
            FText::FromString("Open the Thermo Forge tool tab"),
            ThermoForgeIcon,
            FUIAction(FExecuteAction::CreateRaw(this, &FThermoForgeEditorModule::OpenThermoForgeTab))
        );
    }
}

// ---------------- Button Handlers ----------------

FReply FThermoForgeEditorModule::OnSpawnThermalVolumeClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[ThermoForge] Spawn Thermal Volume"));
    SpawnThermalVolume();
    return FReply::Handled();
}

static void TF_GetEditorCamera(FVector& OutLoc, FRotator& OutRot)
{
    OutLoc = FVector::ZeroVector;
    OutRot = FRotator::ZeroRotator;

    if (!GEditor) return;
    
    if  (UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
    {
        FVector CamLoc; FRotator CamRot;
        if (UES->GetLevelViewportCameraInfo(CamLoc, CamRot))
        {
            OutLoc = CamLoc;
            OutRot = CamRot;
            return;
        }
    }
}

void FThermoForgeEditorModule::SpawnThermalVolume()
{
    if (!GEditor) return;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return;

    FVector CamLoc; FRotator CamRot;
    TF_GetEditorCamera(CamLoc, CamRot);

    const FVector SpawnLoc = CamLoc + CamRot.Vector() * 600.f;

    const FScopedTransaction Tx(NSLOCTEXT("ThermoForge", "SpawnVolume", "Add Thermo Forge Volume"));
    World->Modify();

    FActorSpawnParameters P;
    P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    P.Name = MakeUniqueObjectName(World, AThermoForgeVolume::StaticClass(), TEXT("ThermoForgeVolume"), EUniqueObjectNameOptions::GloballyUnique);

    AThermoForgeVolume* Vol = World->SpawnActor<AThermoForgeVolume>(SpawnLoc, FRotator::ZeroRotator, P);

    GEditor->SelectNone(false, true);
    if (Vol) GEditor->SelectActor(Vol, true, true);
    GEditor->NoteSelectionChange();
}

FReply FThermoForgeEditorModule::OnAddHeatSourceClicked()
{
    if (!GEditor) return FReply::Handled();

    USelection* Sel = GEditor->GetSelectedActors();
    if (!Sel || Sel->Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ThermoForge] No actor selected."));
        return FReply::Handled();
    }

    const FScopedTransaction Tx(NSLOCTEXT("ThermoForge", "AddSourceComponent", "Add Thermo Forge Source Component"));

    int32 AddedCount = 0;

    for (FSelectionIterator It(*Sel); It; ++It)
    {
        AActor* Actor = Cast<AActor>(*It);
        if (!Actor) continue;

        // Skip if already present
        if (Actor->FindComponentByClass<UThermoForgeSourceComponent>())
            continue;

        Actor->Modify();

        UThermoForgeSourceComponent* Comp =
            NewObject<UThermoForgeSourceComponent>(Actor, UThermoForgeSourceComponent::StaticClass(), NAME_None, RF_Transactional);

        if (!Comp) continue;

        Comp->CreationMethod = EComponentCreationMethod::Instance;
        Actor->AddInstanceComponent(Comp);
        Comp->RegisterComponent();

        Actor->MarkPackageDirty();
        ++AddedCount;
    }

    UE_LOG(LogTemp, Log, TEXT("[ThermoForge] Added ThermoForgeSourceComponent to %d actor(s)."), AddedCount);
    GEditor->NoteSelectionChange();

    return FReply::Handled();
}

FReply FThermoForgeEditorModule::OnKickstartSamplingClicked()
{
    if (!GEditor) return FReply::Handled();

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ThermoForge] No editor world available."));
        return FReply::Handled();
    }

    if (UThermoForgeSubsystem* Sub = World->GetSubsystem<UThermoForgeSubsystem>())
    {
        Sub->KickstartSamplingFromVolumes(); // stub for now
        UE_LOG(LogTemp, Log, TEXT("[ThermoForge] KickstartSamplingFromVolumes() called."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ThermoForge] ThermoForgeSubsystem not found on this world."));
    }

    return FReply::Handled();
}


// ---------- NEW: Hide All Previews ----------
FReply FThermoForgeEditorModule::OnShowAllPreviewsClicked()
{
    if (!GEditor) return FReply::Handled();
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return FReply::Handled();

    int32 Hidden = 0;
    for (TActorIterator<AThermoForgeVolume> It(World); It; ++It)
    {
        AThermoForgeVolume* Vol = *It;
        if (Vol)
        {
#if WITH_EDITORONLY_DATA
            Vol->GridPreviewISM->SetVisibility(true);
#endif
            ++Hidden;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[ThermoForge] HideAllPreviews: %d volumes updated."), Hidden);
    return FReply::Handled();
}

// ---------- NEW: Hide All Previews ----------
FReply FThermoForgeEditorModule::OnHideAllPreviewsClicked()
{
    if (!GEditor) return FReply::Handled();
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return FReply::Handled();

    int32 Hidden = 0;
    for (TActorIterator<AThermoForgeVolume> It(World); It; ++It)
    {
        AThermoForgeVolume* Vol = *It;
        if (Vol)
        {
        #if WITH_EDITORONLY_DATA
            Vol->GridPreviewISM->SetVisibility(false);
        #endif
            ++Hidden;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[ThermoForge] HideAllPreviews: %d volumes updated."), Hidden);
    return FReply::Handled();
}

// ---------- NEW: Set Mesh Insulated ----------
FReply FThermoForgeEditorModule::OnSetMeshInsulatedClicked()
{
    if (!GEditor) return FReply::Handled();
    USelection* Sel = GEditor->GetSelectedActors();
    if (!Sel || Sel->Num() == 0) return FReply::Handled();

    UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(
        nullptr,
        TEXT("/ThermoForge/ThermoForgeInsulator_M.ThermoForgeInsulator_M")
    );

    if (!PhysMat)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ThermoForge] Failed to load ThermoForgeInsulator_M physical material."));
        return FReply::Handled();
    }

    int32 Count = 0;
    for (FSelectionIterator It(*Sel); It; ++It)
    {
        AActor* Actor = Cast<AActor>(*It);
        if (!Actor) continue;

        TArray<UStaticMeshComponent*> Meshes;
        Actor->GetComponents<UStaticMeshComponent>(Meshes);
        for (UStaticMeshComponent* Mesh : Meshes)
        {
            if (Mesh)
            {
                Mesh->SetPhysMaterialOverride(PhysMat);
                ++Count;
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[ThermoForge] SetMeshInsulated applied to %d mesh components."), Count);
    return FReply::Handled();
}
FReply FThermoForgeEditorModule::OnOpenSettingsClicked()
{
#if WITH_EDITOR
    if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        // Ask the settings CDO where it actually lives
        const UThermoForgeProjectSettings* CDO = GetDefault<UThermoForgeProjectSettings>();
        const FName Container = CDO ? CDO->GetContainerName() : FName("Project");
        const FName Category  = CDO ? CDO->GetCategoryName()  : FName("Game");
        const FName Section   = CDO ? CDO->GetSectionName()   : FName("ThermoForgeProjectSettings");

        Settings->ShowViewer(Container, Category, Section);
    }
    else
    {
        // Fallback: at least open Project Settings tab
        FGlobalTabmanager::Get()->TryInvokeTab(FName("ProjectSettings"));
    }
#endif
    return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE

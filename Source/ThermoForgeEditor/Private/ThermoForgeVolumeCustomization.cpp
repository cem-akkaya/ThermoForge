#include "ThermoForgeVolumeCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "ThermoForgeVolume.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "DetailWidgetRow.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ThermoForgeVolumeCustomization"

TSharedRef<IDetailCustomization> FThermoForgeVolumeCustomization::MakeInstance()
{
    return MakeShareable(new FThermoForgeVolumeCustomization);
}

void FThermoForgeVolumeCustomization::CustomizeDetails(IDetailLayoutBuilder& Detail)
{
    // Hide clutter
    Detail.HideCategory("Brush");
    Detail.HideCategory("Rendering");
    Detail.HideCategory("Collision");
    Detail.HideCategory("LOD");
    Detail.HideCategory("Input");
    Detail.HideCategory("Actor");
    Detail.HideCategory("HLOD");
    Detail.HideCategory("Cooking");
    Detail.HideCategory("Materials");
    Detail.HideCategory("A Thermo Forge Volume"); // hide original bucket

    // --- Category: Thermo Forge (tools) ---
    IDetailCategoryBuilder& Cat = Detail.EditCategory(
        TEXT("Thermo Forge"),
        LOCTEXT("ThermoForgeCat","Thermo Forge"),
        ECategoryPriority::Transform
    );
    Cat.SetSortOrder(-10000);

#if WITH_EDITOR
    // Row with TWO buttons
    Cat.AddCustomRow(LOCTEXT("RebuildAndHeatRowFilter","Rebuild / Heat Preview"))
    .WholeRowContent()
    [
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().Padding(2.f, 2.f)
        [
            SNew(SButton)
            .Text(LOCTEXT("RebuildPreviewGrid","Rebuild Preview Grid"))
            .ToolTipText(LOCTEXT("RebuildPreviewGrid_TT","Rebuild the visualization cubes for the current volume"))
            .OnClicked_Lambda([&Detail]() -> FReply
            {
                TArray<TWeakObjectPtr<UObject>> Objects;
                Detail.GetObjectsBeingCustomized(Objects);
                for (const TWeakObjectPtr<UObject>& Obj : Objects)
                {
                    if (AThermoForgeVolume* Vol = Cast<AThermoForgeVolume>(Obj.Get()))
                    {
                        Vol->RebuildPreviewGrid();
                    }
                }
                return FReply::Handled();
            })
        ]

        + SHorizontalBox::Slot().AutoWidth().Padding(4.f, 2.f)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT(" ")))
        ]

        + SHorizontalBox::Slot().AutoWidth().Padding(2.f, 2.f)
        [
            SNew(SButton)
            .Text(LOCTEXT("BuildHeatPreview","Build Heat Preview"))
            .ToolTipText(LOCTEXT("BuildHeatPreview_TT","Use the baked field (if any) to build the colored heat preview cubes"))
            .OnClicked_Lambda([&Detail]() -> FReply
            {
                TArray<TWeakObjectPtr<UObject>> Objects;
                Detail.GetObjectsBeingCustomized(Objects);
                for (const TWeakObjectPtr<UObject>& Obj : Objects)
                {
                    if (AThermoForgeVolume* Vol = Cast<AThermoForgeVolume>(Obj.Get()))
                    {
                    #if WITH_EDITORONLY_DATA
                        Vol->GridPreviewISM->SetVisibility(true);
                    #endif
                        Vol->BuildHeatPreviewFromField();
                    }
                }
                return FReply::Handled();
            })
        ]
        
        + SHorizontalBox::Slot().AutoWidth().Padding(4.f, 2.f)
         [
              SNew(STextBlock).Text(FText::FromString(TEXT(" ")))
         ]

        + SHorizontalBox::Slot().AutoWidth().Padding(2.f, 2.f)
        [
            SNew(SButton)
            .Text(LOCTEXT("Hide Preview","Hide Preview"))
            .ToolTipText(LOCTEXT("HidePreview_TT","Hides all previews"))
            .OnClicked_Lambda([&Detail]() -> FReply
            {
                TArray<TWeakObjectPtr<UObject>> Objects;
                Detail.GetObjectsBeingCustomized(Objects);
                for (const TWeakObjectPtr<UObject>& Obj : Objects)
                {
                    if (AThermoForgeVolume* Vol = Cast<AThermoForgeVolume>(Obj.Get()))
                    {
                    #if WITH_EDITORONLY_DATA
                        Vol->HidePreview();
                    #endif
                    }
                }
                return FReply::Handled();
            })
        ]
    ];

    // Info row
    Cat.AddCustomRow(LOCTEXT("EffectiveCellSizeFilter","Effective Cell Size"))
    .WholeRowContent()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 2.f)
        [
            SNew(STextBlock)
            .Text_Lambda([&Detail]()
            {
                TArray<TWeakObjectPtr<UObject>> Objects;
                Detail.GetObjectsBeingCustomized(Objects);
                float Cell = 0.f;
                for (const TWeakObjectPtr<UObject>& Obj : Objects)
                {
                    if (AThermoForgeVolume* Vol = Cast<AThermoForgeVolume>(Obj.Get()))
                    {
                        Cell = Vol->GetEffectiveCellSize();
                        break;
                    }
                }
                return FText::Format(LOCTEXT("EffectiveCellSizeFmt","Effective Cell Size: {0} cm"), FText::AsNumber(Cell));
            })
        ]
    ];
#endif

    // --- Core Properties ---
    auto Unbounded = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, bUnbounded));
    auto Extent    = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, BoxExtent));
    Cat.AddProperty(Unbounded);
    Cat.AddProperty(Extent);

    // --- Grid Settings ---
    IDetailCategoryBuilder& GridCat = Detail.EditCategory(
        TEXT("Thermo Forge Grid"),
        LOCTEXT("ThermoForgeGridCat","Thermo Forge - Grid Settings"),
        ECategoryPriority::Important
    );
    auto UseGlobal = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, bUseGlobalGrid));
    auto Cell      = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridCellSize));
    auto OriginMd  = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridOriginMode));
    auto OriginWS  = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridOriginWS));
    auto RotationMd= Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridOrientationMode));
    auto Gap       = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridCellGap));
    auto MaxI      = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, MaxPreviewInstances));
    GridCat.AddProperty(UseGlobal);
    GridCat.AddProperty(Cell);
    GridCat.AddProperty(OriginMd);
    GridCat.AddProperty(OriginWS);
    GridCat.AddProperty(RotationMd);
    GridCat.AddProperty(Gap);
    GridCat.AddProperty(MaxI);

#if WITH_EDITORONLY_DATA
    // --- Preview Settings ---
    IDetailCategoryBuilder& PrevCat = Detail.EditCategory(
        TEXT("Thermo Forge Preview"),
        LOCTEXT("ThermoForgePreviewCat","Thermo Forge - Preview"),
        ECategoryPriority::Default
    );
    auto AutoPrev  = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, bAutoRebuildPreview));
    auto Mat       = Detail.GetProperty(GET_MEMBER_NAME_CHECKED(AThermoForgeVolume, GridPreviewMaterial));
    PrevCat.AddProperty(AutoPrev);
    PrevCat.AddProperty(Mat);
#endif
}

#undef LOCTEXT_NAMESPACE

using UnrealBuildTool;

public class ThermoForgeEditor : ModuleRules
{
    public ThermoForgeEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",  
                "SlateCore",
                "PhysicsCore"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "UnrealEd",
                "EditorWidgets",
                "EditorSubsystem",
                "DeveloperSettings",
                "UMG",
                "ToolMenus",
                "Blutility", 
                "UMG",
                "UMGEditor", 
                "BSPUtils",
                "ThermoForge"
            }
        );
    }
}
using UnrealBuildTool;

public class PurpleVsGreen : ModuleRules
{
    public PurpleVsGreen(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore" });
        PrivateDependencyModuleNames.Add("ApplicationCore");
    }
}

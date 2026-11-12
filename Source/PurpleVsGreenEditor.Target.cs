using UnrealBuildTool;

public class PurpleVsGreenEditorTarget : TargetRules
{
    public PurpleVsGreenEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("PurpleVsGreen");
    }
}

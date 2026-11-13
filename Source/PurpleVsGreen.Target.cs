using UnrealBuildTool;

public class PurpleVsGreenTarget : TargetRules
{
    public PurpleVsGreenTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("PurpleVsGreen");
        if (Target.Platform == UnrealTargetPlatform.Linux)
            ExtraModuleNames.Add("PvGLinuxAudio");
    }
}

using UnrealBuildTool;

public class PvGLinuxAudio : ModuleRules
{
    public PvGLinuxAudio(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AudioMixer", "AudioMixerCore" });
        AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL3");
    }
}

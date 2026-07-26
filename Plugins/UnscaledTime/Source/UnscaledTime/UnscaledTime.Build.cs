using UnrealBuildTool;

public class UnscaledTime : ModuleRules
{
	public UnscaledTime(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings"
		});
	}
}

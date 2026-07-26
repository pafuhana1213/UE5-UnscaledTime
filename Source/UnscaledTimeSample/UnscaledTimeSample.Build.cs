using UnrealBuildTool;

public class UnscaledTimeSample : ModuleRules
{
	public UnscaledTimeSample(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnscaledTime"
		});
	}
}

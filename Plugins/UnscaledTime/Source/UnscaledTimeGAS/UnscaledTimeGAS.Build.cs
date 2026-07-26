using UnrealBuildTool;

public class UnscaledTimeGAS : ModuleRules
{
	public UnscaledTimeGAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnscaledTime",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks"
		});
	}
}

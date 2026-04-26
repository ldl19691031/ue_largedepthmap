using UnrealBuildTool;

public class LargeDepthMap : ModuleRules
{
	public LargeDepthMap(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"UMG"
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"ApplicationCore",
				"Json",
				"RenderCore",
				"RHI"
			});
	}
}

using UnrealBuildTool;

public class LargeDepthMapEditor : ModuleRules
{
	public LargeDepthMapEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"LargeDepthMap"
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"AssetTools",
				"Json",
				"Slate",
				"SlateCore",
				"UnrealEd"
			});
	}
}

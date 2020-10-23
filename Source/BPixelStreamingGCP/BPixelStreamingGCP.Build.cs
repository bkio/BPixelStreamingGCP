/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

using UnrealBuildTool;

public class BPixelStreamingGCP : ModuleRules
{
	public BPixelStreamingGCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DesktopPlatform",
				"UnrealEd",
				"Json",
				"JsonUtilities",
				"HTTP",
				"BZipLib",
				"BUtilities"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"UMG",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UMGEditor",
				"LevelEditor",
				"UnrealEd",
				"Blutility"
			});
	}
}
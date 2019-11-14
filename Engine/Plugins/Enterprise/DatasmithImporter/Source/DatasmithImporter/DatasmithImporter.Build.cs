// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DatasmithImporter : ModuleRules
	{
		public DatasmithImporter(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"CinematicCamera",
					"ContentBrowser",
					"Core",
					"CoreUObject",
					"DesktopPlatform",
					"EditorStyle",
					"Engine",
					"Foliage",
					"FreeImage",
					"InputCore",
					"Json",
					"Landscape",
					"LandscapeEditor",
					"LandscapeEditorUtilities",
					"LevelSequence",
					"MainFrame",
					"MaterialEditor",
					"MeshDescription",
					"MeshDescriptionOperations",
					"MeshUtilities",
					"MeshUtilitiesCommon",
					"MessageLog",
					"MovieScene",
					"MovieSceneTracks",
                    "RawMesh",
					"RHI",
					"Slate",
					"SlateCore",
					"SourceControl",
					"StaticMeshDescription",
					"UnrealEd",
					"VariantManager",
					"VariantManagerContent",
                    "StaticMeshEditorExtension",
                }
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"DataprepCore",
					"DatasmithContent",
                    "DatasmithCore",
                    "DatasmithContentEditor",
				}
			);
        }
    }
}
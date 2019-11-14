// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class MDLImporter : ModuleRules
    {
        private string ThirdPartyPath
        {
            get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/NotForLicensees/")); }
        }

        public MDLImporter(ReadOnlyTargetRules Target) : base(Target)
        {
			bLegalToDistributeObjectCode = true;

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Analytics",
                    "Core",
                    "RenderCore",
                    "ImageCore",
                    "CoreUObject",
                    "Engine",
                    "MessageLog",
                    "UnrealEd",
                    "Slate",
                    "SlateCore",
                    "Mainframe",
                    "InputCore",
                    "EditorStyle",
                    "MaterialEditor",
                    "Projects"
                }
            );

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                }
            );

            string MdlSdkModuleName = "libmdl_sdk.so";
            
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                MdlSdkModuleName = "libmdl_sdk.dll";

            }
            else
            {
				MdlSdkModuleName = "libmdl_sdk.so";
            }

            PublicDelayLoadDLLs.Add(MdlSdkModuleName);
            string ModulePath = Path.Combine(EngineDirectory, "Plugins/Enterprise/MDLImporter/Binaries/ThirdParty/MDL", Target.Platform.ToString(), MdlSdkModuleName);
            if (!File.Exists(ModulePath))
            {
                string Err = string.Format("MDL SDK module '{0}' not found.", ModulePath);
                System.Console.WriteLine(Err);
                throw new BuildException(Err);
            }
            RuntimeDependencies.Add(ModulePath);

            if (Directory.Exists(ThirdPartyPath))
            {
                //third party libraries
                string[] Libs = { "mdl-sdk-314800.830"};
                foreach (string Lib in Libs)
                {
                    string IncludePath = Path.Combine(ThirdPartyPath, Lib, "include");
                    if (Directory.Exists(IncludePath))
                    {
                        PublicSystemIncludePaths.Add(IncludePath);
                    }
                    else
                    {
                        return;
                    }
                }

                PrivateDefinitions.Add("USE_MDLSDK");

                if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
                {
                    PublicDefinitions.Add("MI_PLATFORM_WINDOWS");
                }
                else if (Target.Platform == UnrealTargetPlatform.Linux)
                {
                    PublicDefinitions.Add("MI_PLATFORM_LINUX");
                }
                else if (Target.Platform == UnrealTargetPlatform.Mac)
                {
                    PublicDefinitions.Add("MI_PLATFORM_MACOSX");
                }
            }
        }
    }
}

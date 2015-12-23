// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class KinectPlugin : ModuleRules
	{
		public KinectPlugin(TargetInfo Target)
		{
                    string SDKDIR = Utils.ResolveEnvironmentVariable("%KINECTSDK20_DIR%");

                    SDKDIR = SDKDIR.Replace("\\", "/");
                    
                    if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
                        {
                            PublicIncludePaths.Add(SDKDIR+"inc/");
                            
                            string PlatformPath =  (Target.Platform == UnrealTargetPlatform.Win64) ? "x64/" : "x86/";
                            
                            string LibPath = SDKDIR+"Lib/"+PlatformPath;
                            
                            PublicLibraryPaths.Add(LibPath);
                            PublicAdditionalLibraries.AddRange(new string[]{ "Kinect20.lib","Kinect20.VisualGestureBuilder.lib","Kinect20.Fusion.lib"});
                            

                            string DllPath = SDKDIR + "Bin/";
                            
                            //PublicDelayLoadDLLs.AddRange(new string[] {DllPath+"Kinect20.Fusion.dll"});
                            
                        }
                    

			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"KinectPlugin/Private",
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                                        "Engine",        
                                        "RHI",        
                                        "RenderCore",        
					"ProceduralMeshComponent",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);
		}
	}
}

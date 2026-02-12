// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BluetoothWinRT : ModuleRules
{
	public BluetoothWinRT(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		// Add WinRT support for Windows platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("windowsapp.lib");
			PublicDefinitions.Add("WINRT_LEAN_AND_MEAN");
			
			// Enable C++/WinRT
			bEnableExceptions = true;
			PublicDefinitions.Add("_SILENCE_CLANG_COROUTINE_MESSAGE");
			
			// Add WinRT include path
			string WinSDKVersion = Target.WindowsPlatform.WindowsSdkVersion;
			string WinSDKPath = @"C:\Program Files (x86)\Windows Kits\10\Include\" + WinSDKVersion;
			
			PublicSystemIncludePaths.Add(WinSDKPath + @"\cppwinrt");
		}

		// Note: This plugin uses WinRT APIs on Windows; you may need to enable C++/WinRT support in your build environment.
	}
}

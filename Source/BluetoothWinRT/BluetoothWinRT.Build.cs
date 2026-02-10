// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BluetoothWinRT : ModuleRules
{
	public BluetoothWinRT(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
			}
		);

		// WinRT / C++/WinRT support on Windows
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Add C++/WinRT include path from Windows SDK
			string WindowsSDKDir = Path.Combine(
				System.Environment.GetFolderPath(System.Environment.SpecialFolder.ProgramFilesX86),
				"Windows Kits", "10", "Include", "10.0.26100.0", "cppwinrt");

			if (Directory.Exists(WindowsSDKDir))
			{
				PublicSystemIncludePaths.Add(WindowsSDKDir);
			}

			// Link required Windows runtime libraries for WinRT APIs
			PublicSystemLibraries.AddRange(new string[]
			{
				"WindowsApp.lib",
				"runtimeobject.lib"
			});

			// Enable C++17 for C++/WinRT coroutines and features
			CppStandard = CppStandardVersion.Cpp17;
		}
	}
}

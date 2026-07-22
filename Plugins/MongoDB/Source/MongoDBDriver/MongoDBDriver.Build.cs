// 2025 Copyright Pandores Marketplace

using UnrealBuildTool;
using System.IO;

public class MongoDBDriver : ModuleRules
{

	/**
	 * Module rules target constructor.
	 **/
	public MongoDBDriver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// Enable exceptions as MongoCXX relies on it.
		bEnableExceptions = true;

        // Engine dependencies
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
		PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "Engine", "zlib",  });

		// For Project config panel.
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] 
			{ 
				"Slate", 
				"SlateCore", 
				"UnrealEd", 
				"PropertyEditor", 
				"Settings",
			});
		}

		// Darwin and iOS system dependencies.
		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
        {
			PublicFrameworks.AddRange(new string[]
			{
				"CoreFoundation",
				"Foundation",
				"Security",
				"GSS",
				"SystemConfiguration",
			});

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicFrameworks.Add("Kerberos");
            }

            PublicSystemLibraries.Add("resolv");
		}

		// Path to Source/ThirdParty.
		string ThirdPartyPath = Path.Combine(PluginDirectory, "Source/ThirdParty/");
		string MongocxxPath   = Path.Combine(ThirdPartyPath,  "mongocxx/");

		//////////////////////////////////////////////////////////////////////////////
		//																			//
		//								WINDOWS										//
		//																			//
		//////////////////////////////////////////////////////////////////////////////
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// System libraries
			PublicSystemLibraries.AddRange(new string[] { "crypt32.lib" });

            // Libraries
            PublicAdditionalLibraries.AddRange(new string[]
			{
				Path.Combine(MongocxxPath, "windows-x64/libs/bsoncxx-static-rti-md.lib"),
				Path.Combine(MongocxxPath, "windows-x64/libs/bson-static-1.0.lib"),
				Path.Combine(MongocxxPath, "windows-x64/libs/mongoc-static-1.0.lib"),
				Path.Combine(MongocxxPath, "windows-x64/libs/mongocxx-static-rti-md.lib"),
				Path.Combine(MongocxxPath, "windows-x64/libs/utf8proc_static.lib"),
				Path.Combine(MongocxxPath, "windows-x64/libs/zlib.lib"),
			});

			// Includes
			PrivateIncludePaths.Add(MongocxxPath + "windows-x64/include/");
        }

		//////////////////////////////////////////////////////////////////////////////
		//																			//
		//								ANDROID										//
		//																			//
		//////////////////////////////////////////////////////////////////////////////
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
            if (Target.Architectures.Contains(UnrealArch.X64))
            {
                // Libraries
                PublicAdditionalLibraries.AddRange(new string[]
				{
					Path.Combine(MongocxxPath, "android-x64/libs/libmongocxx-x64.so"),
					Path.Combine(MongocxxPath, "android-x64/libs/libbsoncxx-static-x64.a"),
					Path.Combine(MongocxxPath, "android-x64/libs/libbson-static-1.0-x64.a")
				});

                // Includes
                PrivateIncludePaths.Add(MongocxxPath + "android-x64/include/");
            }

            if (Target.Architectures.Contains(UnrealArch.Arm64))
            {
                // Libraries
                PublicAdditionalLibraries.AddRange(new string[]
				{
					Path.Combine(MongocxxPath, "android-arm64/libs/libmongocxx-arm64.so"),
					Path.Combine(MongocxxPath, "android-arm64/libs/libbsoncxx-static.a"),
					Path.Combine(MongocxxPath, "android-arm64/libs/libbson-static-1.0.a"),
				});

                // Includes
                PrivateIncludePaths.Add(MongocxxPath + "android-arm64/include/");
            }


            // UPL File for copying the libraries inside the APK.
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "UPL/MongoDB.Android.UPL.xml"));
        }

		//////////////////////////////////////////////////////////////////////////////
		//																			//
		//									IOS										//
		//																			//
		//////////////////////////////////////////////////////////////////////////////
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
            // Libraries
            PublicAdditionalLibraries.AddRange(new string[]
			{
				Path.Combine(MongocxxPath, "ios-arm64/libs/libbsoncxx-static.a"),
				Path.Combine(MongocxxPath, "ios-arm64/libs/libbson-static-1.0.a"),
				Path.Combine(MongocxxPath, "ios-arm64/libs/libmongoc-static-1.0.a"),
				Path.Combine(MongocxxPath, "ios-arm64/libs/libmongocxx-static.a"),
				Path.Combine(MongocxxPath, "ios-arm64/libs/libutf8proc.a"),
				Path.Combine(MongocxxPath, "ios-arm64/libs/libz.a"),
			});
            
            // Includes
			PrivateIncludePaths.Add(MongocxxPath + "ios-arm64/include/");
        }

		//////////////////////////////////////////////////////////////////////////////
		//																			//
		//									MACOS									//
		//																			//
		//////////////////////////////////////////////////////////////////////////////
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			if (Target.Architectures.Contains(UnrealArch.Arm64))
            {
                // Libraries
                PublicAdditionalLibraries.AddRange(new string[]
				{
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libbsoncxx-static.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libbson-static-1.0.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libmongoc-static-1.0.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libmongocxx-static.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libutf8proc.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libz.a"),
				});
            }

            if (Target.Architectures.Contains(UnrealArch.X64))
            {
                // Libraries
                PublicAdditionalLibraries.AddRange(new string[]
				{
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libbsoncxx-static.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libbson-static-1.0.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libmongoc-static-1.0.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libmongocxx-static.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libutf8proc.a"),
					Path.Combine(MongocxxPath, "darwin-arm64/libs/libz.a"),
				});
            }
        }

		//////////////////////////////////////////////////////////////////////////////
		//																			//
		//								LINUX										//
		//																			//
		//////////////////////////////////////////////////////////////////////////////
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            System.Console.Error.WriteLine("`" + Target.Platform + "` supported is suspended.");
            PublicSystemLibraries.Add("resolv");
        }

		//////////////////////////////////////////////////////////////////////////////
		//																			//
		//						UNSUPPORTED PLATFORM								//
		//																			//
		//////////////////////////////////////////////////////////////////////////////
		else
		{
			// This platform is not supported.
			// Note that you can add platform by compiling mongocxx for this platform.
			System.Console.Error.WriteLine("Unsupported platform `" + Target.Platform + "`.");
	    }

		// Plugin files
		PublicIncludePaths .Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
	}
}

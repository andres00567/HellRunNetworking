// Copyright 1998-2024 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HellRunNetworking : ModuleRules
{
    public HellRunNetworking(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "OnlineBase",
                "OnlineSubsystem",
                "OnlineSubsystemUtils",
                "OnlineSubsystemNull",
                "OnlineSubsystemSteam",
                "SocketSubsystemSteamIP"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "OnlineSubsystem",
                "OnlineSubsystemNull",
                "OnlineSubsystemSteam",
                "SocketSubsystemSteamIP"
            }
        );
    }
}

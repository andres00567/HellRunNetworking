# Hell Run Networking

Multiplayer networking layer for Unreal Engine with session management, social features, and chat support.

## Features

- Central `NetworkingManager`
- Networking interface abstraction
- Social interface
- Chat interface
- Session-oriented multiplayer support
- Online Subsystem integration
- Steam support
- Null subsystem support for local testing
- Cross-platform runtime module configuration

## Enabled plugin dependencies

The plugin descriptor enables:

- OnlineSubsystem
- OnlineSubsystemUtils
- OnlineSubsystemNull
- OnlineSubsystemSteam
- SteamShared
- SocketSubsystemSteamIP

## Module

- `HellRunNetworking` — Runtime

The module is allowed on Win64, Mac, Linux, iOS, and Android.

## Basic setup

1. Copy the plugin into your project's `Plugins` directory.
2. Enable **Hell Run Networking** and the Online Subsystem plugins required by your target platform.
3. Configure the desired subsystem in your project settings/INI files.
4. Access multiplayer, social, and chat functionality through the plugin's manager and interfaces.

## Steam/LAN notes

See [STEAM_LAN_FIX.md](STEAM_LAN_FIX.md) for the repository's Steam/LAN troubleshooting notes.

## Status

Version 1.0.0.

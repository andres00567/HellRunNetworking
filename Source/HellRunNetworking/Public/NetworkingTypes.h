#pragma once

#include "CoreMinimal.h"
#include "NetworkingTypes.generated.h"

class UWorld;

/**
 * Network connection states
 */
UENUM(BlueprintType)
enum class ENetworkConnectionState : uint8
{
    Disconnected = 0 UMETA(DisplayName = "Disconnected"),
    Connecting = 1 UMETA(DisplayName = "Connecting"),
    Connected = 2 UMETA(DisplayName = "Connected"),
    Hosting = 3 UMETA(DisplayName = "Hosting"),
    Error = 4 UMETA(DisplayName = "Error")
};

UENUM(BlueprintType)
enum class ENetworkMatchMode : uint8
{
    Coop = 0 UMETA(DisplayName = "Co-op"),
    Deathmatch = 1 UMETA(DisplayName = "Deathmatch"),
    TeamDeathmatch = 2 UMETA(DisplayName = "Team Deathmatch")
};

UENUM(BlueprintType)
enum class EHellRunGameModeType : uint8
{
    MainMenu = 0 UMETA(DisplayName = "Main Menu"),
    Lobby = 1 UMETA(DisplayName = "Lobby"),
    Campaign = 2 UMETA(DisplayName = "Campaign"),
    Coop = 3 UMETA(DisplayName = "Co-op"),
    FreeForAll = 4 UMETA(DisplayName = "Free For All"),
    TeamDeathmatch = 5 UMETA(DisplayName = "Team Deathmatch")
};

UENUM(BlueprintType)
enum class ENetworkSessionMode : uint8
{
    Online = 0 UMETA(DisplayName = "Online"),
    LAN = 1 UMETA(DisplayName = "LAN")
};

UENUM(BlueprintType)
enum class ESteamLoginState : uint8
{
    NotLoggedIn = 0 UMETA(DisplayName = "Not Logged In"),
    UsingLocalProfile = 1 UMETA(DisplayName = "Using Local Profile"),
    LoggedIn = 2 UMETA(DisplayName = "Logged In")
};

UENUM(BlueprintType)
enum class ESteamPresenceState : uint8
{
    Online = 0 UMETA(DisplayName = "Online"),
    Offline = 1 UMETA(DisplayName = "Offline"),
    Away = 2 UMETA(DisplayName = "Away"),
    ExtendedAway = 3 UMETA(DisplayName = "Extended Away"),
    DoNotDisturb = 4 UMETA(DisplayName = "Do Not Disturb"),
    Chat = 5 UMETA(DisplayName = "Chat")
};

USTRUCT(BlueprintType)
struct FSteamUserInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Steam")
    bool bSubsystemAvailable = false;

    UPROPERTY(BlueprintReadOnly, Category="Steam")
    bool bIsSteamActive = false;

    UPROPERTY(BlueprintReadOnly, Category="Steam")
    FString SubsystemName;

    UPROPERTY(BlueprintReadOnly, Category="Steam")
    ESteamLoginState LoginState = ESteamLoginState::NotLoggedIn;

    UPROPERTY(BlueprintReadOnly, Category="Steam")
    FString UserId;

    UPROPERTY(BlueprintReadOnly, Category="Steam")
    FString DisplayName;
};

USTRUCT(BlueprintType)
struct FSteamPresenceInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    FString UserId;

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    bool bWasFound = false;

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    bool bIsOnline = false;

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    bool bIsPlaying = false;

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    bool bIsPlayingThisGame = false;

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    bool bIsJoinable = false;

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    bool bHasVoiceSupport = false;

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    FString Status;

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    ESteamPresenceState State = ESteamPresenceState::Offline;

    UPROPERTY(BlueprintReadOnly, Category="Steam|Presence")
    FString SessionId;
};

USTRUCT(BlueprintType)
struct FHellRunNetModeInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Networking")
    FString NetMode;

    UPROPERTY(BlueprintReadOnly, Category="Networking")
    bool bIsStandalone = false;

    UPROPERTY(BlueprintReadOnly, Category="Networking")
    bool bIsDedicatedServer = false;

    UPROPERTY(BlueprintReadOnly, Category="Networking")
    bool bIsListenServer = false;

    UPROPERTY(BlueprintReadOnly, Category="Networking")
    bool bIsClient = false;

    UPROPERTY(BlueprintReadOnly, Category="Networking")
    bool bHasAuthority = false;

    UPROPERTY(BlueprintReadOnly, Category="Networking")
    bool bIsLocallyControlled = false;

    UPROPERTY(BlueprintReadOnly, Category="Networking")
    int32 LocalPlayerCount = 0;
};

USTRUCT(BlueprintType)
struct FHellRunMutatorSetting
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mutator")
    FName MutatorId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mutator")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mutator")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mutator")
    TMap<FName, FString> Parameters;
};

USTRUCT(BlueprintType)
struct FLobbyMatchSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    TSoftObjectPtr<UWorld> LobbyMap;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    TSoftObjectPtr<UWorld> MatchMap;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    FString LobbyMapPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    FString MapPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    FString LobbyGameModePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    ENetworkMatchMode MatchMode = ENetworkMatchMode::TeamDeathmatch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxPlayers = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby", meta=(ClampMin="1", ClampMax="4"))
    int32 NumTeams = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby", meta=(ClampMin="0"))
    int32 KillLimit = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby", meta=(ClampMin="0"))
    int32 TeamScoreLimit = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby", meta=(ClampMin="0"))
    int32 TimeLimitSeconds = 600;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby", meta=(ClampMin="0"))
    int32 RespawnDelaySeconds = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby|Mutators")
    TArray<FHellRunMutatorSetting> Mutators;
};

USTRUCT(BlueprintType)
struct FHellRunSessionInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Session")
    FString SessionId;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    FString SessionName;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    FString MapPath;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    FString LobbyMapPath;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    FLobbyMatchSettings MatchSettings;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    FString MatchMode;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    int32 CurrentPlayers = 0;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    int32 MaxPlayers = 0;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    int32 OpenPublicConnections = 0;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    int32 PingInMs = 0;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    bool bIsLAN = false;

    UPROPERTY(BlueprintReadOnly, Category="Session")
    bool bIsJoinable = false;
};

/**
 * Friendship states
 */
UENUM(BlueprintType)
enum class EFriendshipState : uint8
{
    None = 0 UMETA(DisplayName = "None"),
    Pending = 1 UMETA(DisplayName = "Pending"),
    Friends = 2 UMETA(DisplayName = "Friends"),
    Blocked = 3 UMETA(DisplayName = "Blocked")
};

/**
 * Chat message types
 */
UENUM(BlueprintType)
enum class EChatMessageType : uint8
{
    Global = 0 UMETA(DisplayName = "Global"),
    Team = 1 UMETA(DisplayName = "Team"),
    Proximity = 2 UMETA(DisplayName = "Proximity"),
    Whisper = 3 UMETA(DisplayName = "Whisper"),
    System = 4 UMETA(DisplayName = "System")
};

/**
 * Voice transmission states
 */
UENUM(BlueprintType)
enum class EVoiceState : uint8
{
    Idle = 0 UMETA(DisplayName = "Idle"),
    Transmitting = 1 UMETA(DisplayName = "Transmitting"),
    Receiving = 2 UMETA(DisplayName = "Receiving"),
    Muted = 3 UMETA(DisplayName = "Muted")
};

/**
 * Game invite status
 */
UENUM(BlueprintType)
enum class EGameInviteStatus : uint8
{
    Pending = 0 UMETA(DisplayName = "Pending"),
    Accepted = 1 UMETA(DisplayName = "Accepted"),
    Declined = 2 UMETA(DisplayName = "Declined"),
    Expired = 3 UMETA(DisplayName = "Expired")
};

/**
 * Friend data structure
 */
USTRUCT(BlueprintType)
struct FFriendData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Friends")
    FString UserId;

    UPROPERTY(BlueprintReadOnly, Category="Friends")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category="Friends")
    EFriendshipState FriendshipState = EFriendshipState::None;

    UPROPERTY(BlueprintReadOnly, Category="Friends")
    bool bIsOnline = false;

    UPROPERTY(BlueprintReadOnly, Category="Friends")
    FString CurrentGame;

    UPROPERTY(BlueprintReadOnly, Category="Friends")
    FString CurrentStatus;
};

/**
 * Game invite data structure
 */
USTRUCT(BlueprintType)
struct FGameInviteData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Invites")
    FString InviteId;

    UPROPERTY(BlueprintReadOnly, Category="Invites")
    FString FromUserId;

    UPROPERTY(BlueprintReadOnly, Category="Invites")
    FString FromDisplayName;

    UPROPERTY(BlueprintReadOnly, Category="Invites")
    FString SessionId;

    UPROPERTY(BlueprintReadOnly, Category="Invites")
    EGameInviteStatus Status = EGameInviteStatus::Pending;

    UPROPERTY(BlueprintReadOnly, Category="Invites")
    FDateTime CreatedTime;

    UPROPERTY(BlueprintReadOnly, Category="Invites")
    FString Message;
};

/**
 * Chat message structure
 */
USTRUCT(BlueprintType)
struct FChatMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Chat")
    FString SenderId;

    UPROPERTY(BlueprintReadOnly, Category="Chat")
    FString SenderName;

    UPROPERTY(BlueprintReadOnly, Category="Chat")
    FString Message;

    UPROPERTY(BlueprintReadOnly, Category="Chat")
    EChatMessageType MessageType = EChatMessageType::Global;

    UPROPERTY(BlueprintReadOnly, Category="Chat")
    FDateTime Timestamp;

    UPROPERTY(BlueprintReadOnly, Category="Chat")
    float Distance = 0.0f;
};

/**
 * Proximity voice information
 */
USTRUCT(BlueprintType)
struct FProximityVoiceInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Voice")
    FString UserId;

    UPROPERTY(BlueprintReadOnly, Category="Voice")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category="Voice")
    bool bIsSpeaking = false;

    UPROPERTY(BlueprintReadOnly, Category="Voice")
    bool bIsMuted = false;

    UPROPERTY(BlueprintReadOnly, Category="Voice")
    float Distance = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Voice")
    float VolumeModifier = 1.0f;
};

// Declare delegates for networking events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectionStateChanged, ENetworkConnectionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionCreated, FString, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSessionFound, FString, SessionName, int32, PlayerCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionError, FString, ErrorMessage);

// Declare delegates for social events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSocialFriendRequestReceived, const FString&, FromUserId, const FString&, FromDisplayName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSocialGameInviteReceived, const FString&, FromUserId, const FString&, SessionId);

// Declare delegates for chat events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatMessageReceived, const FChatMessage&, ChatMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoiceStateChanged, const FString&, UserId, EVoiceState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerMuted, const FString&, UserId);

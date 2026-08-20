#include "NetworkingManager.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"

namespace
{
constexpr const TCHAR* DefaultHostLobbyMapPath = TEXT("/Game/Levels/GameFlow/PlayerTestMap");
constexpr const TCHAR* DefaultMatchMapPath = TEXT("/Game/FirstPerson/Lvl_FirstPerson");
constexpr const TCHAR* HellRunGameIdKey = TEXT("GAME_ID");
constexpr const TCHAR* HellRunGameIdValue = TEXT("HellRun");
constexpr const TCHAR* GameNetDriverDefName = TEXT("GameNetDriver");
constexpr const TCHAR* SteamNetDriverClassName = TEXT("/Script/SocketSubsystemSteamIP.SteamNetDriver");
constexpr const TCHAR* SteamNetDriverFallbackClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
constexpr const TCHAR* IpNetDriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
constexpr const TCHAR* IpNetDriverFallbackClassName = TEXT("/Script/SocketSubsystemSteamIP.SteamNetDriver");

FString ResolveLevelReferencePath(const TSoftObjectPtr<UWorld>& LevelReference)
{
    const FSoftObjectPath ObjectPath = LevelReference.ToSoftObjectPath();
    return ObjectPath.IsValid() ? ObjectPath.GetLongPackageName() : FString();
}

void ShowNetworkDebugMessage(const FString& Message, const FColor& Color = FColor::Cyan)
{
    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, Color, Message);
    }
}

bool IsSupportedMatchModeValue(const FString& MatchMode)
{
    return MatchMode.IsEmpty()
        || MatchMode.Equals(TEXT("Coop"), ESearchCase::IgnoreCase)
        || MatchMode.Equals(TEXT("Deathmatch"), ESearchCase::IgnoreCase)
        || MatchMode.Equals(TEXT("TDM"), ESearchCase::IgnoreCase)
        || MatchMode.Equals(TEXT("TeamDeathmatch"), ESearchCase::IgnoreCase)
        || MatchMode.Equals(TEXT("Team Deathmatch"), ESearchCase::IgnoreCase);
}

bool LooksLikeHellRunSession(const FOnlineSessionSearchResult& Result)
{
    FString GameIdValue;
    if (Result.Session.SessionSettings.Get(FName(HellRunGameIdKey), GameIdValue))
    {
        return GameIdValue.Equals(HellRunGameIdValue, ESearchCase::IgnoreCase);
    }

    FString IgnoredStringValue;
    return Result.Session.SessionSettings.Get(FName("SERVER_NAME"), IgnoredStringValue)
        || Result.Session.SessionSettings.Get(FName("MATCH_MODE"), IgnoredStringValue)
        || Result.Session.SessionSettings.Get(FName("LOBBY_MAP"), IgnoredStringValue)
        || Result.Session.SessionSettings.Get(SETTING_MAPNAME, IgnoredStringValue);
}

FString GetQuickJoinLabel(bool bIsLanQuery, bool bRandom)
{
    const TCHAR* ModeLabel = bIsLanQuery ? TEXT("LAN Quick Join") : TEXT("Online Quick Join");
    if (bRandom)
    {
        return FString::Printf(TEXT("%s (Random)"), ModeLabel);
    }

    return FString(ModeLabel);
}

bool IsSteamConnectString(const FString& ConnectString)
{
    return ConnectString.StartsWith(TEXT("steam."), ESearchCase::IgnoreCase);
}

bool IsPIEWorld(const UWorld* World)
{
    return World && World->WorldType == EWorldType::PIE;
}

void ConfigureGameNetDriver(bool bPreferSteam)
{
    if (!GEngine)
    {
        return;
    }

    if (FNetDriverDefinition* DriverDefinition = GEngine->NetDriverDefinitions.FindByPredicate([](const FNetDriverDefinition& Definition)
        {
            return Definition.DefName == FName(GameNetDriverDefName);
        }))
    {
        const FName DesiredPrimary = bPreferSteam ? FName(SteamNetDriverClassName) : FName(IpNetDriverClassName);
        const FName DesiredFallback = bPreferSteam ? FName(SteamNetDriverFallbackClassName) : FName(IpNetDriverFallbackClassName);

        if (DriverDefinition->DriverClassName != DesiredPrimary || DriverDefinition->DriverClassNameFallback != DesiredFallback)
        {
            DriverDefinition->DriverClassName = DesiredPrimary;
            DriverDefinition->DriverClassNameFallback = DesiredFallback;
            UE_LOG(LogTemp, Log, TEXT("NetworkingManager: GameNetDriver configured for %s (Primary=%s Fallback=%s)"),
                bPreferSteam ? TEXT("Steam") : TEXT("LAN/IP"),
                *DesiredPrimary.ToString(),
                *DesiredFallback.ToString());
        }
    }
}
}

void UNetworkingManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    OnlineSubsystem = IOnlineSubsystem::Get();
    if (!OnlineSubsystem)
    {
        OnlineSubsystem = IOnlineSubsystem::Get(FName(TEXT("Steam")));
    }

    if (OnlineSubsystem)
    {
        const FName SubsystemName = OnlineSubsystem->GetSubsystemName();
        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Online subsystem initialized - %s"), *SubsystemName.ToString());

        if (SubsystemName != FName(TEXT("STEAM")) && SubsystemName != FName(TEXT("Steam")))
        {
            UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: Steam OSS is not active. Online Steam functionality may be limited. Current OSS: %s"), *SubsystemName.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Steam OSS active for online matchmaking. LAN sessions use the NULL subsystem."));
        }

        if (IOnlineFriendsPtr Friends = OnlineSubsystem->GetFriendsInterface())
        {
            Friends->ReadFriendsList(0, EFriendsLists::ToString(EFriendsLists::Default));
        }

        if (IOnlineSessionPtr Sessions = GetSessionInterface())
        {
            SessionUserInviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
                FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UNetworkingManager::OnSessionUserInviteAccepted));
            SessionInviteReceivedHandle = Sessions->AddOnSessionInviteReceivedDelegate_Handle(
                FOnSessionInviteReceivedDelegate::CreateUObject(this, &UNetworkingManager::OnSessionInviteReceived));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: No online subsystem found"));
        SetConnectionState(ENetworkConnectionState::Error);
    }
}

void UNetworkingManager::Deinitialize()
{
    if (IOnlineSessionPtr Sessions = GetSessionInterface())
    {
        if (SessionUserInviteAcceptedHandle.IsValid())
        {
            Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedHandle);
        }

        if (SessionInviteReceivedHandle.IsValid())
        {
            Sessions->ClearOnSessionInviteReceivedDelegate_Handle(SessionInviteReceivedHandle);
        }

        if (FindFriendSessionCompleteHandle.IsValid())
        {
            Sessions->ClearOnFindFriendSessionCompleteDelegate_Handle(0, FindFriendSessionCompleteHandle);
        }
    }

    CachedFoundSessions.Empty();
    PendingInviteResultsById.Empty();
    FriendsCache.Empty();
    InvitesCache.Empty();
    GlobalChatHistory.Empty();
    TeamChatHistory.Empty();
    ProximityChatHistory.Empty();
    WhisperChatHistory.Empty();
    SystemChatHistory.Empty();
    MutedPlayers.Empty();
    PlayersInProximity.Empty();

    Super::Deinitialize();
}

// === Networking Implementation ===

void UNetworkingManager::CreateSession_Implementation(const FString& SessionName, int32 MaxPlayers, bool bIsLAN)
{
    bActiveSessionIsLAN = ShouldForceLANForSession(bIsLAN);
    ConfigureGameNetDriver(!bActiveSessionIsLAN);

    const ENetworkSessionMode SessionMode = bActiveSessionIsLAN ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online;
    IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(SessionMode);
    if (!Sessions.IsValid())
    {
        OnSessionError.Broadcast(TEXT("No online session interface available"));
        SetConnectionState(ENetworkConnectionState::Error);
        return;
    }

    const FString SafeSessionName = SessionName.IsEmpty() ? FString(TEXT("HellRunSession")) : SessionName;
    const FName RequestedSessionName = NAME_GameSession;

    if (Sessions->GetNamedSession(RequestedSessionName))
    {
        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Existing game session found, destroying before recreate as %s."), *SafeSessionName);

        bCreateSessionAfterDestroy = true;
        PendingSessionName = SafeSessionName;
        PendingMaxPlayers = MaxPlayers;
        bPendingSessionIsLAN = bIsLAN;

        ActiveSessionName = RequestedSessionName;
        DestroySession_Implementation();
        return;
    }

    ActiveSessionName = RequestedSessionName;
    IOnlineSubsystem* ActiveSubsystem = GetOnlineSubsystemForMode(SessionMode);
    const FName SubsystemName = ActiveSubsystem ? ActiveSubsystem->GetSubsystemName() : NAME_None;
    MaxPlayerCount = FMath::Clamp(MaxPlayers > 0 ? MaxPlayers : LobbyMatchSettings.MaxPlayers, 1, 64);
    LobbyMatchSettings.MaxPlayers = MaxPlayerCount;
    SetConnectionState(ENetworkConnectionState::Connecting);

    FString EnabledMutators;
    for (const FHellRunMutatorSetting& Mutator : LobbyMatchSettings.Mutators)
    {
        if (Mutator.bEnabled && !Mutator.MutatorId.IsNone())
        {
            if (!EnabledMutators.IsEmpty())
            {
                EnabledMutators += TEXT(",");
            }
            EnabledMutators += Mutator.MutatorId.ToString();
        }
    }

    FOnlineSessionSettings Settings;
    Settings.NumPublicConnections = MaxPlayerCount;
    Settings.bIsLANMatch = bActiveSessionIsLAN;
    Settings.bShouldAdvertise = true;
    Settings.bAllowJoinInProgress = true;
    Settings.bAllowJoinViaPresence = !bActiveSessionIsLAN;
    Settings.bAllowInvites = !bActiveSessionIsLAN;
    Settings.bUsesPresence = !bActiveSessionIsLAN;
    Settings.bUseLobbiesIfAvailable = !bActiveSessionIsLAN;
    Settings.bUsesStats = false;
    Settings.bAntiCheatProtected = false;

    Settings.Set(FName(HellRunGameIdKey), FString(HellRunGameIdValue), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("SERVER_NAME"), SafeSessionName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("LOBBY_MAP"), LobbyMatchSettings.LobbyMapPath, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("LOBBY_GAME_MODE"), LobbyMatchSettings.LobbyGameModePath, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(SETTING_MAPNAME, LobbyMatchSettings.MapPath, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("MATCH_MODE"), GetMatchModeOptionString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("NUM_TEAMS"), LobbyMatchSettings.NumTeams, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("KILL_LIMIT"), LobbyMatchSettings.KillLimit, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("TEAM_SCORE_LIMIT"), LobbyMatchSettings.TeamScoreLimit, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("TIME_LIMIT"), LobbyMatchSettings.TimeLimitSeconds, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("RESPAWN_DELAY"), LobbyMatchSettings.RespawnDelaySeconds, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName("MUTATORS"), EnabledMutators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    if (!PendingCampaignId.IsEmpty())
    {
        Settings.Set(FName("CAMPAIGN_ID"), PendingCampaignId,
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
        Settings.Set(FName("MATCH_MODE"), FString(TEXT("Coop")),
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    }
    PendingCampaignId.Reset();

    CreateSessionCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &UNetworkingManager::OnCreateSessionComplete));

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Creating %s session on %s - %s (Max: %d)"),
        bActiveSessionIsLAN ? TEXT("LAN") : TEXT("online"), *SubsystemName.ToString(), *SafeSessionName, MaxPlayerCount);
    if (bActiveSessionIsLAN)
    {
        ShowNetworkDebugMessage(FString::Printf(TEXT("LAN Host: creating '%s' on %s (Max: %d)"),
            *SafeSessionName,
            *SubsystemName.ToString(),
            MaxPlayerCount));
    }

    if (!Sessions->CreateSession(0, ActiveSessionName, Settings))
    {
        Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
        if (bActiveSessionIsLAN)
        {
            ShowNetworkDebugMessage(TEXT("LAN Host: CreateSession failed to start"), FColor::Red);
        }
        OnSessionError.Broadcast(TEXT("CreateSession failed to start"));
        SetConnectionState(ENetworkConnectionState::Error);
    }
}

void UNetworkingManager::FindSessions_Implementation(int32 MaxSearchResults)
{
    if (bFindSessionsInProgress)
    {
        ShowNetworkDebugMessage(TEXT("Session search already in progress"), FColor::Yellow);
        return;
    }

    bQuickJoinAfterFind = false;
    bQuickJoinRandomAfterFind = false;
    bQuickJoinCampaignOnly = false;

    const bool bSearchLAN = ShouldSearchLAN();
    const ENetworkSessionMode SearchMode = bSearchLAN ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online;
    IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(SearchMode);
    if (!Sessions.IsValid())
    {
        OnSessionError.Broadcast(TEXT("No online session interface available"));
        return;
    }

    SearchResultsById.Empty();
    CachedFoundSessions.Empty();
    CurrentSessionSearch = MakeShared<FOnlineSessionSearch>();
    CurrentSessionSearch->MaxSearchResults = FMath::Max(1, MaxSearchResults);
    CurrentSessionSearch->bIsLanQuery = bSearchLAN;
    IOnlineSubsystem* ActiveSubsystem = GetOnlineSubsystemForMode(SearchMode);
    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Finding sessions on %s (Max: %d, LANQuery: %s)"),
        ActiveSubsystem ? *ActiveSubsystem->GetSubsystemName().ToString() : TEXT("None"),
        MaxSearchResults,
        CurrentSessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"));
    if (!CurrentSessionSearch->bIsLanQuery)
    {
        CurrentSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    }

    FindSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &UNetworkingManager::OnFindSessionsComplete));

    bFindSessionsInProgress = true;
    SetConnectionState(ENetworkConnectionState::Connecting);

    if (!Sessions->FindSessions(0, CurrentSessionSearch.ToSharedRef()))
    {
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        bFindSessionsInProgress = false;
        OnSessionError.Broadcast(TEXT("FindSessions failed to start"));
        SetConnectionState(ENetworkConnectionState::Disconnected);
    }
}

void UNetworkingManager::QuickJoinFirstSession(int32 MaxSearchResults)
{
    BeginQuickJoinFirstSession(MaxSearchResults, false);
}

void UNetworkingManager::QuickJoinCampaignSession(int32 MaxSearchResults)
{
    BeginQuickJoinFirstSession(MaxSearchResults, true);
}

void UNetworkingManager::BeginQuickJoinFirstSession(
    int32 MaxSearchResults, bool bCampaignOnly)
{
    if (bFindSessionsInProgress)
    {
        ShowNetworkDebugMessage(TEXT("Quick Join: search already in progress"), FColor::Yellow);
        return;
    }

    bQuickJoinAfterFind = true;
    bQuickJoinRandomAfterFind = false;
    bQuickJoinCampaignOnly = bCampaignOnly;

    const bool bSearchLAN = ShouldSearchLAN();
    const ENetworkSessionMode SearchMode = bSearchLAN ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online;
    IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(SearchMode);
    if (!Sessions.IsValid())
    {
        bQuickJoinAfterFind = false;
        bQuickJoinCampaignOnly = false;
        OnSessionError.Broadcast(TEXT("No online session interface available"));
        SetConnectionState(ENetworkConnectionState::Error);
        return;
    }

    SearchResultsById.Empty();
    CachedFoundSessions.Empty();
    CurrentSessionSearch = MakeShared<FOnlineSessionSearch>();
    CurrentSessionSearch->MaxSearchResults = FMath::Max(1, MaxSearchResults);
    CurrentSessionSearch->bIsLanQuery = bSearchLAN;
    IOnlineSubsystem* ActiveSubsystem = GetOnlineSubsystemForMode(SearchMode);
    const FString QuickJoinLabel = GetQuickJoinLabel(CurrentSessionSearch->bIsLanQuery, false);
    ShowNetworkDebugMessage(FString::Printf(TEXT("%s: searching on %s (Max: %d, LANQuery: %s, SearchLobbies: %s)"),
        *QuickJoinLabel,
        ActiveSubsystem ? *ActiveSubsystem->GetSubsystemName().ToString() : TEXT("None"),
        CurrentSessionSearch->MaxSearchResults,
        CurrentSessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
        CurrentSessionSearch->bIsLanQuery ? TEXT("false") : TEXT("true")));
    if (!CurrentSessionSearch->bIsLanQuery)
    {
        CurrentSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    }

    FindSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &UNetworkingManager::OnFindSessionsComplete));

    bFindSessionsInProgress = true;
    SetConnectionState(ENetworkConnectionState::Connecting);

    if (!Sessions->FindSessions(0, CurrentSessionSearch.ToSharedRef()))
    {
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        bQuickJoinAfterFind = false;
        bQuickJoinCampaignOnly = false;
        bFindSessionsInProgress = false;
        OnSessionError.Broadcast(TEXT("Quick join search failed to start"));
        SetConnectionState(ENetworkConnectionState::Disconnected);
    }
}

bool UNetworkingManager::AdvertiseCampaignSession(
    const FString& CampaignId, const FString& FirstMapPath)
{
    FName SessionName = NAME_None;
    bool bIsLAN = false;
    IOnlineSessionPtr Sessions;
    if (CampaignId.IsEmpty()
        || !FindExistingSession(SessionName, bIsLAN, &Sessions)
        || !Sessions.IsValid())
    {
        return false;
    }

    FNamedOnlineSession* NamedSession = Sessions->GetNamedSession(SessionName);
    if (!NamedSession)
    {
        return false;
    }
    NamedSession->SessionSettings.Set(
        FName("CAMPAIGN_ID"), CampaignId,
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    NamedSession->SessionSettings.Set(
        FName("MATCH_MODE"), FString(TEXT("Coop")),
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    if (!FirstMapPath.IsEmpty())
    {
        NamedSession->SessionSettings.Set(
            SETTING_MAPNAME, FirstMapPath,
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    }
    const bool bStarted = Sessions->UpdateSession(
        SessionName, NamedSession->SessionSettings, true);
    UE_LOG(LogTemp, Display,
        TEXT("NetworkingManager: Campaign session advertisement update campaign=%s map=%s lan=%d started=%d"),
        *CampaignId, *FirstMapPath, bIsLAN, bStarted);
    return bStarted;
}

void UNetworkingManager::ConfigurePendingCampaignSession(
    const FString& CampaignId, const FString& FirstMapPath)
{
    PendingCampaignId = CampaignId;
    LobbyMatchSettings.MatchMode = ENetworkMatchMode::Coop;
    if (!FirstMapPath.IsEmpty())
    {
        LobbyMatchSettings.MapPath = FirstMapPath;
        LobbyMatchSettings.LobbyMapPath = FirstMapPath;
        // Let the campaign map's World Settings select CampaignGameMode.
        LobbyMatchSettings.LobbyGameModePath.Reset();
    }
    UE_LOG(LogTemp, Display,
        TEXT("NetworkingManager: Pending campaign session configured campaign=%s map=%s"),
        *PendingCampaignId, *FirstMapPath);
}

void UNetworkingManager::QuickJoinRandomSession(int32 MaxSearchResults)
{
    if (bFindSessionsInProgress)
    {
        ShowNetworkDebugMessage(TEXT("Quick Join: search already in progress"), FColor::Yellow);
        return;
    }

    bQuickJoinAfterFind = false;
    bQuickJoinRandomAfterFind = true;
    bQuickJoinCampaignOnly = false;

    const bool bSearchLAN = ShouldSearchLAN();
    const ENetworkSessionMode SearchMode = bSearchLAN ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online;
    IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(SearchMode);
    if (!Sessions.IsValid())
    {
        bQuickJoinRandomAfterFind = false;
        OnSessionError.Broadcast(TEXT("No online session interface available"));
        SetConnectionState(ENetworkConnectionState::Error);
        return;
    }

    SearchResultsById.Empty();
    CachedFoundSessions.Empty();
    CurrentSessionSearch = MakeShared<FOnlineSessionSearch>();
    CurrentSessionSearch->MaxSearchResults = FMath::Max(1, MaxSearchResults);
    CurrentSessionSearch->bIsLanQuery = bSearchLAN;
    IOnlineSubsystem* ActiveSubsystem = GetOnlineSubsystemForMode(SearchMode);
    const FString QuickJoinLabel = GetQuickJoinLabel(CurrentSessionSearch->bIsLanQuery, true);
    ShowNetworkDebugMessage(FString::Printf(TEXT("%s: searching on %s (Max: %d, LANQuery: %s, SearchLobbies: %s)"),
        *QuickJoinLabel,
        ActiveSubsystem ? *ActiveSubsystem->GetSubsystemName().ToString() : TEXT("None"),
        CurrentSessionSearch->MaxSearchResults,
        CurrentSessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
        CurrentSessionSearch->bIsLanQuery ? TEXT("false") : TEXT("true")));
    if (!CurrentSessionSearch->bIsLanQuery)
    {
        CurrentSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    }

    FindSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &UNetworkingManager::OnFindSessionsComplete));

    bFindSessionsInProgress = true;
    SetConnectionState(ENetworkConnectionState::Connecting);

    if (!Sessions->FindSessions(0, CurrentSessionSearch.ToSharedRef()))
    {
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        bQuickJoinRandomAfterFind = false;
        bFindSessionsInProgress = false;
        OnSessionError.Broadcast(TEXT("Random quick join search failed to start"));
        SetConnectionState(ENetworkConnectionState::Disconnected);
    }
}

void UNetworkingManager::GetFoundSessions(TArray<FHellRunSessionInfo>& OutSessions) const
{
    OutSessions = CachedFoundSessions;
}

void UNetworkingManager::SetNetworkSessionMode(ENetworkSessionMode NewMode)
{
    PreferredSessionMode = NewMode;
    ConfigureGameNetDriver(PreferredSessionMode != ENetworkSessionMode::LAN);
    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Network session mode set to %s"),
        PreferredSessionMode == ENetworkSessionMode::LAN ? TEXT("LAN") : TEXT("Online"));
}

void UNetworkingManager::LeaveMatchAndOpenLevel(const FString& LevelPath)
{
    LeaveMatchAndOpenLevelFromWorld(GetWorld(), LevelPath);
}

void UNetworkingManager::LeaveMatchAndOpenLevelFromWorld(UWorld* TravelWorld, const FString& LevelPath)
{
    PendingOpenLevelPath = LevelPath.IsEmpty() ? TEXT("/Game/Levels/GameFlow/MainMenu") : LevelPath;
    PendingOpenLevelWorld = TravelWorld;
    bOpenLevelAfterDestroy = true;
    bCreateSessionAfterDestroy = false;
    PendingSessionName.Empty();
    PendingMaxPlayers = 4;
    bPendingSessionIsLAN = false;

    FName SessionNameToDestroy = NAME_None;
    bool bSessionIsLAN = false;
    IOnlineSessionPtr Sessions;
    const bool bHasActiveSession = FindExistingSession(SessionNameToDestroy, bSessionIsLAN, &Sessions);

    if (bHasActiveSession)
    {
        bActiveSessionIsLAN = bSessionIsLAN;
        ActiveSessionName = SessionNameToDestroy;
        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Leaving session %s before opening %s"),
            *ActiveSessionName.ToString(), *PendingOpenLevelPath);
        DestroySession_Implementation();

        if (TravelWorld)
        {
            TravelWorld->GetTimerManager().ClearTimer(LeaveMatchFallbackTimer);
            TravelWorld->GetTimerManager().SetTimer(LeaveMatchFallbackTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
            {
                OpenPendingLevelAfterDestroy();
            }), 2.0f, false);
        }
        return;
    }

    CurrentSessionId = TEXT("");
    CurrentPlayerCount = 0;
    ActiveSessionName = NAME_None;
    SetConnectionState(ENetworkConnectionState::Disconnected);
    OpenPendingLevelAfterDestroy();
}

void UNetworkingManager::JoinParty(const FString& PartyId)
{
    if (PartyId.IsEmpty())
    {
        OnSessionError.Broadcast(TEXT("Cannot join party without a party/session id"));
        return;
    }

    JoinSession_Implementation(PartyId);
}

void UNetworkingManager::LeaveParty()
{
    DestroySession_Implementation();
}

bool UNetworkingManager::OpenSteamInviteOverlay()
{
    if (!OnlineSubsystem)
    {
        OnSessionError.Broadcast(TEXT("Steam online subsystem is unavailable"));
        return false;
    }

    const FName SessionNameForInvite = FindActiveSessionName();
    if (SessionNameForInvite == NAME_None)
    {
        OnSessionError.Broadcast(TEXT("Cannot open Steam invite overlay without an active session"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Attempting to open Steam invite overlay. Subsystem=%s Session=%s"),
        *OnlineSubsystem->GetSubsystemName().ToString(), *SessionNameForInvite.ToString());

    if (IOnlineExternalUIPtr ExternalUI = OnlineSubsystem->GetExternalUIInterface())
    {
        if (ExternalUI->ShowInviteUI(0, SessionNameForInvite))
        {
            UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Opened Steam invite UI for session %s"), *SessionNameForInvite.ToString());
            return true;
        }

        UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: Steam External UI interface is available, but ShowInviteUI returned false for session %s"),
            *SessionNameForInvite.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: Steam External UI interface is unavailable. Overlay may be disabled, not hooked, or unsupported in this launch mode."));
    }

    OnSessionError.Broadcast(TEXT("Steam invite overlay is unavailable"));
    return false;
}

bool UNetworkingManager::InviteSteamFriend(const FString& FriendUserId)
{
    if (FriendUserId.IsEmpty())
    {
        return OpenSteamInviteOverlay();
    }

    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid() || !OnlineSubsystem)
    {
        OnSessionError.Broadcast(TEXT("No online session interface available"));
        return false;
    }

    const FName SessionNameForInvite = FindActiveSessionName();
    if (SessionNameForInvite == NAME_None)
    {
        OnSessionError.Broadcast(TEXT("Cannot invite Steam friend without an active session"));
        return false;
    }

    IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        OnSessionError.Broadcast(TEXT("Steam identity interface is unavailable"));
        return false;
    }

    FUniqueNetIdPtr FriendId = Identity->CreateUniquePlayerId(FriendUserId);
    if (!FriendId.IsValid())
    {
        OnSessionError.Broadcast(FString::Printf(TEXT("Invalid Steam friend id: %s"), *FriendUserId));
        return false;
    }

    if (Sessions->SendSessionInviteToFriend(0, SessionNameForInvite, *FriendId))
    {
        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Sent Steam session invite to friend %s for session %s"),
            *FriendUserId, *SessionNameForInvite.ToString());
        return true;
    }

    OnSessionError.Broadcast(FString::Printf(TEXT("Failed to send Steam session invite to %s"), *FriendUserId));
    return false;
}

bool UNetworkingManager::JoinSteamFriend(const FString& FriendUserId)
{
    if (FriendUserId.IsEmpty())
    {
        OnSessionError.Broadcast(TEXT("Cannot join Steam friend without a friend id"));
        return false;
    }

    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid() || !OnlineSubsystem)
    {
        OnSessionError.Broadcast(TEXT("No online session interface available"));
        return false;
    }

    IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        OnSessionError.Broadcast(TEXT("Steam identity interface is unavailable"));
        return false;
    }

    FUniqueNetIdPtr FriendId = Identity->CreateUniquePlayerId(FriendUserId);
    if (!FriendId.IsValid())
    {
        OnSessionError.Broadcast(FString::Printf(TEXT("Invalid Steam friend id: %s"), *FriendUserId));
        return false;
    }

    if (FindFriendSessionCompleteHandle.IsValid())
    {
        Sessions->ClearOnFindFriendSessionCompleteDelegate_Handle(0, FindFriendSessionCompleteHandle);
    }

    FindFriendSessionCompleteHandle = Sessions->AddOnFindFriendSessionCompleteDelegate_Handle(
        0, FOnFindFriendSessionCompleteDelegate::CreateUObject(this, &UNetworkingManager::OnFindFriendSessionComplete));

    SetConnectionState(ENetworkConnectionState::Connecting);

    if (Sessions->FindFriendSession(0, *FriendId))
    {
        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Finding Steam friend session for %s"), *FriendUserId);
        return true;
    }

    Sessions->ClearOnFindFriendSessionCompleteDelegate_Handle(0, FindFriendSessionCompleteHandle);
    FindFriendSessionCompleteHandle.Reset();
    SetConnectionState(ENetworkConnectionState::Disconnected);
    OnSessionError.Broadcast(FString::Printf(TEXT("Failed to start Steam friend session search for %s"), *FriendUserId));
    return false;
}

void UNetworkingManager::JoinSession_Implementation(const FString& SessionId)
{
    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid())
    {
        OnSessionError.Broadcast(TEXT("No online session interface available"));
        SetConnectionState(ENetworkConnectionState::Error);
        return;
    }

    const FOnlineSessionSearchResult* SearchResult = SearchResultsById.Find(SessionId);
    if (!SearchResult)
    {
        OnSessionError.Broadcast(FString::Printf(TEXT("Unknown session id: %s. Find sessions first."), *SessionId));
        SetConnectionState(ENetworkConnectionState::Disconnected);
        return;
    }

    if (!JoinSearchResult(*SearchResult, SessionId))
    {
        OnSessionError.Broadcast(TEXT("JoinSession failed to start"));
        SetConnectionState(ENetworkConnectionState::Error);
    }
}

void UNetworkingManager::StartSession_Implementation()
{
    if (CurrentConnectionState != ENetworkConnectionState::Hosting)
    {
        OnSessionError.Broadcast(TEXT("Only host can start session"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Session started - %s"), *CurrentSessionId);

    if (IOnlineSessionPtr Sessions = GetSessionInterface())
    {
        Sessions->StartSession(ActiveSessionName);
    }

    if (UWorld* World = GetWorld())
    {
        CurrentPlayerCount = CountConnectedPlayers();
        World->ServerTravel(GetDeathmatchTravelURL(false));
    }
}

void UNetworkingManager::DestroySession_Implementation()
{
    FName SessionNameToDestroy = NAME_None;
    bool bSessionIsLAN = false;
    IOnlineSessionPtr Sessions;
    if (FindExistingSession(SessionNameToDestroy, bSessionIsLAN, &Sessions) && Sessions.IsValid())
    {
        bActiveSessionIsLAN = bSessionIsLAN;
        DestroySessionCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(this, &UNetworkingManager::OnDestroySessionComplete));

        if (SessionNameToDestroy != NAME_None)
        {
            ActiveSessionName = SessionNameToDestroy;
            if (Sessions->DestroySession(ActiveSessionName))
            {
                return;
            }
        }

        Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
    }

    CurrentSessionId = TEXT("");
    CurrentPlayerCount = 0;
    ActiveSessionName = NAME_None;
    SetConnectionState(ENetworkConnectionState::Disconnected);

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Session destroyed"));
    OpenPendingLevelAfterDestroy();
}

ENetworkConnectionState UNetworkingManager::GetConnectionState_Implementation() const
{
    return CurrentConnectionState;
}

bool UNetworkingManager::IsConnected_Implementation() const
{
    return CurrentConnectionState == ENetworkConnectionState::Connected;
}

bool UNetworkingManager::IsHosting_Implementation() const
{
    return CurrentConnectionState == ENetworkConnectionState::Hosting;
}

int32 UNetworkingManager::GetPlayerCount_Implementation() const
{
    return CountConnectedPlayers();
}

FString UNetworkingManager::GetCurrentSessionId_Implementation() const
{
    return CurrentSessionId;
}

// === Social Implementation ===

void UNetworkingManager::SendFriendRequest_Implementation(const FString& UserId)
{
    UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: Steam friend requests are handled by the Steam overlay/profile UI. Requested user: %s"), *UserId);
}

void UNetworkingManager::SetLobbyMatchSettings(const FLobbyMatchSettings& NewSettings)
{
    LobbyMatchSettings = NewSettings;
    LobbyMatchSettings.MaxPlayers = FMath::Clamp(LobbyMatchSettings.MaxPlayers, 1, 64);
    LobbyMatchSettings.NumTeams = FMath::Clamp(LobbyMatchSettings.NumTeams, 1, 4);
    LobbyMatchSettings.KillLimit = FMath::Max(0, LobbyMatchSettings.KillLimit);
    LobbyMatchSettings.TeamScoreLimit = FMath::Max(0, LobbyMatchSettings.TeamScoreLimit);
    LobbyMatchSettings.TimeLimitSeconds = FMath::Max(0, LobbyMatchSettings.TimeLimitSeconds);
    LobbyMatchSettings.RespawnDelaySeconds = FMath::Max(0, LobbyMatchSettings.RespawnDelaySeconds);

    const FString ResolvedMatchMapPath = ResolveLevelReferencePath(LobbyMatchSettings.MatchMap);
    if (!ResolvedMatchMapPath.IsEmpty())
    {
        LobbyMatchSettings.MapPath = ResolvedMatchMapPath;
    }

    const FString ResolvedLobbyMapPath = ResolveLevelReferencePath(LobbyMatchSettings.LobbyMap);
    if (!ResolvedLobbyMapPath.IsEmpty())
    {
        LobbyMatchSettings.LobbyMapPath = ResolvedLobbyMapPath;
    }

    if (LobbyMatchSettings.MapPath.IsEmpty())
    {
        LobbyMatchSettings.MapPath = DefaultMatchMapPath;
    }

    if (LobbyMatchSettings.LobbyMapPath.IsEmpty())
    {
        LobbyMatchSettings.LobbyMapPath = LobbyMatchSettings.MapPath.IsEmpty() ? FString(DefaultHostLobbyMapPath) : LobbyMatchSettings.MapPath;
    }

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Match settings resolved. LobbyMap=%s MatchMap=%s LobbyGameMode=%s"),
        *LobbyMatchSettings.LobbyMapPath,
        *LobbyMatchSettings.MapPath,
        LobbyMatchSettings.LobbyGameModePath.IsEmpty() ? TEXT("<default>") : *LobbyMatchSettings.LobbyGameModePath);
}

void UNetworkingManager::AcceptFriendRequest_Implementation(const FString& UserId)
{
    if (!FriendsCache.Contains(UserId))
    {
        FFriendData NewFriend;
        NewFriend.UserId = UserId;
        NewFriend.FriendshipState = EFriendshipState::Friends;
        NewFriend.DisplayName = TEXT("Friend_") + UserId;
        FriendsCache.Add(UserId, NewFriend);
    }

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Friend request accepted from %s"), *UserId);
}

void UNetworkingManager::DeclineFriendRequest_Implementation(const FString& UserId)
{
    if (FriendsCache.Contains(UserId))
    {
        FriendsCache.Remove(UserId);
    }

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Friend request declined from %s"), *UserId);
}

void UNetworkingManager::RemoveFriend_Implementation(const FString& UserId)
{
    if (FriendsCache.Contains(UserId))
    {
        FriendsCache.Remove(UserId);
        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Friend removed - %s"), *UserId);
    }
}

void UNetworkingManager::BlockPlayer_Implementation(const FString& UserId)
{
    if (FriendsCache.Contains(UserId))
    {
        FriendsCache[UserId].FriendshipState = EFriendshipState::Blocked;
    }

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Player blocked - %s"), *UserId);
}

void UNetworkingManager::UnblockPlayer_Implementation(const FString& UserId)
{
    if (FriendsCache.Contains(UserId))
    {
        FriendsCache[UserId].FriendshipState = EFriendshipState::None;
    }

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Player unblocked - %s"), *UserId);
}

void UNetworkingManager::GetFriendsList_Implementation(TArray<FFriendData>& OutFriends)
{
    PopulateFriendsCacheFromOnlineSubsystem();

    OutFriends.Empty();
    for (const auto& Friend : FriendsCache)
    {
        if (Friend.Value.FriendshipState == EFriendshipState::Friends)
        {
            OutFriends.Add(Friend.Value);
        }
    }
}

EFriendshipState UNetworkingManager::GetFriendshipStatus_Implementation(const FString& UserId) const
{
    if (FriendsCache.Contains(UserId))
    {
        return FriendsCache[UserId].FriendshipState;
    }
    return EFriendshipState::None;
}

void UNetworkingManager::SendGameInvite_Implementation(const FString& UserId, const FString& SessionId, const FString& Message)
{
    if (!UserId.IsEmpty() && InviteSteamFriend(UserId))
    {
        return;
    }

    if (OpenSteamInviteOverlay())
    {
        return;
    }

    FString InviteId = FGuid::NewGuid().ToString();

    FGameInviteData NewInvite;
    NewInvite.InviteId = InviteId;
    NewInvite.FromUserId = TEXT("LocalPlayer");
    NewInvite.FromDisplayName = TEXT("You");
    NewInvite.SessionId = SessionId;
    NewInvite.Status = EGameInviteStatus::Pending;
    NewInvite.CreatedTime = FDateTime::Now();
    NewInvite.Message = Message;

    InvitesCache.Add(InviteId, NewInvite);

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Game invite sent to %s for session %s"), *UserId, *SessionId);
}

void UNetworkingManager::AcceptGameInvite_Implementation(const FString& InviteId)
{
    if (InvitesCache.Contains(InviteId))
    {
        FGameInviteData& Invite = InvitesCache[InviteId];
        Invite.Status = EGameInviteStatus::Accepted;

        if (const FOnlineSessionSearchResult* InviteResult = PendingInviteResultsById.Find(InviteId))
        {
            UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Steam invite accepted - joining invite %s"), *InviteId);
            JoinSearchResult(*InviteResult, InviteId);
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Game invite accepted - joining session %s"), *Invite.SessionId);
        JoinSession_Implementation(Invite.SessionId);
    }
}

void UNetworkingManager::DeclineGameInvite_Implementation(const FString& InviteId)
{
    if (InvitesCache.Contains(InviteId))
    {
        FGameInviteData& Invite = InvitesCache[InviteId];
        Invite.Status = EGameInviteStatus::Declined;

        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Game invite declined"));
    }
}

void UNetworkingManager::GetPendingInvites_Implementation(TArray<FGameInviteData>& OutInvites)
{
    OutInvites.Empty();
    for (const auto& Invite : InvitesCache)
    {
        if (Invite.Value.Status == EGameInviteStatus::Pending)
        {
            OutInvites.Add(Invite.Value);
        }
    }
}

void UNetworkingManager::SetPlayerStatus_Implementation(const FString& Status)
{
    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Player status updated - %s"), *Status);
}

void UNetworkingManager::UpdateGameInfo_Implementation(const FString& GameName, const FString& SessionId)
{
    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Game info updated - %s (Session: %s)"), *GameName, *SessionId);
}

// === Chat Implementation ===

void UNetworkingManager::SendChatMessage_Implementation(const FString& Message, EChatMessageType MessageType)
{
    if (Message.IsEmpty())
    {
        return;
    }

    FChatMessage ChatMessage;
    ChatMessage.SenderId = TEXT("LocalPlayer");
    ChatMessage.SenderName = TEXT("You");
    ChatMessage.Message = Message;
    ChatMessage.MessageType = MessageType;
    ChatMessage.Timestamp = FDateTime::Now();
    ChatMessage.Distance = 0.0f;

    AddChatMessage(ChatMessage);
    OnChatMessageReceived.Broadcast(ChatMessage);

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Chat message sent - %s"), *Message);
}

void UNetworkingManager::GetChatHistory_Implementation(EChatMessageType MessageType, TArray<FChatMessage>& OutMessages)
{
    OutMessages.Empty();

    switch (MessageType)
    {
        case EChatMessageType::Global:
            OutMessages = GlobalChatHistory;
            break;
        case EChatMessageType::Team:
            OutMessages = TeamChatHistory;
            break;
        case EChatMessageType::Proximity:
            OutMessages = ProximityChatHistory;
            break;
        case EChatMessageType::Whisper:
            OutMessages = WhisperChatHistory;
            break;
        case EChatMessageType::System:
            OutMessages = SystemChatHistory;
            break;
    }
}

void UNetworkingManager::ClearChatHistory_Implementation(EChatMessageType MessageType)
{
    switch (MessageType)
    {
        case EChatMessageType::Global:
            GlobalChatHistory.Empty();
            break;
        case EChatMessageType::Team:
            TeamChatHistory.Empty();
            break;
        case EChatMessageType::Proximity:
            ProximityChatHistory.Empty();
            break;
        case EChatMessageType::Whisper:
            WhisperChatHistory.Empty();
            break;
        case EChatMessageType::System:
            SystemChatHistory.Empty();
            break;
    }

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Chat history cleared for type %d"), (int32)MessageType);
}

void UNetworkingManager::StartVoiceTransmission_Implementation()
{
    if (CurrentVoiceState != EVoiceState::Transmitting)
    {
        CurrentVoiceState = EVoiceState::Transmitting;
        OnVoiceStateChanged.Broadcast(TEXT("LocalPlayer"), CurrentVoiceState);

        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Voice transmission started"));
    }
}

void UNetworkingManager::StopVoiceTransmission_Implementation()
{
    if (CurrentVoiceState == EVoiceState::Transmitting)
    {
        CurrentVoiceState = EVoiceState::Idle;
        OnVoiceStateChanged.Broadcast(TEXT("LocalPlayer"), CurrentVoiceState);

        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Voice transmission stopped"));
    }
}

bool UNetworkingManager::IsTransmitting_Implementation() const
{
    return CurrentVoiceState == EVoiceState::Transmitting;
}

void UNetworkingManager::MutePlayer_Implementation(const FString& UserId)
{
    if (!MutedPlayers.Contains(UserId))
    {
        MutedPlayers.Add(UserId);
        OnPlayerMuted.Broadcast(UserId);

        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Player muted - %s"), *UserId);
    }
}

void UNetworkingManager::UnmutePlayer_Implementation(const FString& UserId)
{
    if (MutedPlayers.Contains(UserId))
    {
        MutedPlayers.Remove(UserId);

        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Player unmuted - %s"), *UserId);
    }
}

void UNetworkingManager::MuteAll_Implementation()
{
    bAllMuted = true;

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: All players muted"));
}

void UNetworkingManager::UnmuteAll_Implementation()
{
    bAllMuted = false;

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: All players unmuted"));
}

bool UNetworkingManager::IsPlayerMuted_Implementation(const FString& UserId) const
{
    return bAllMuted || MutedPlayers.Contains(UserId);
}

void UNetworkingManager::SetProximityChatRange_Implementation(float NewRange)
{
    ProximityChatRange = FMath::Max(0.0f, NewRange);

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Proximity chat range set to %.0f cm"), ProximityChatRange);
}

float UNetworkingManager::GetProximityChatRange_Implementation() const
{
    return ProximityChatRange;
}

void UNetworkingManager::GetPlayersInProximity_Implementation(TArray<FProximityVoiceInfo>& OutPlayers)
{
    OutPlayers.Empty();

    for (const auto& PlayerInfo : PlayersInProximity)
    {
        OutPlayers.Add(PlayerInfo.Value);
    }
}

// === Helper Methods ===

void UNetworkingManager::SetConnectionState(ENetworkConnectionState NewState)
{
    if (CurrentConnectionState != NewState)
    {
        CurrentConnectionState = NewState;
        OnConnectionStateChanged.Broadcast(NewState);
    }
}

IOnlineSubsystem* UNetworkingManager::GetOnlineSubsystemForMode(ENetworkSessionMode Mode) const
{
    const UWorld* World = GetWorld();
    if (Mode == ENetworkSessionMode::LAN)
    {
        if (IOnlineSubsystem* NullSubsystem = Online::GetSubsystem(World, FName(TEXT("NULL"))))
        {
            return NullSubsystem;
        }

        UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: LAN mode requested but NULL subsystem is unavailable. Falling back to default subsystem."));
    }

    if (IOnlineSubsystem* WorldSubsystem = Online::GetSubsystem(World))
    {
        return WorldSubsystem;
    }

    return OnlineSubsystem;
}

IOnlineSubsystem* UNetworkingManager::GetActiveOnlineSubsystem() const
{
    return GetOnlineSubsystemForMode(PreferredSessionMode);
}

IOnlineSessionPtr UNetworkingManager::GetSessionInterfaceForMode(ENetworkSessionMode Mode) const
{
    IOnlineSubsystem* ActiveSubsystem = GetOnlineSubsystemForMode(Mode);
    return ActiveSubsystem ? ActiveSubsystem->GetSessionInterface() : nullptr;
}

IOnlineSessionPtr UNetworkingManager::GetSessionInterface() const
{
    return GetSessionInterfaceForMode(PreferredSessionMode);
}

FString UNetworkingManager::GetLobbyTravelURL(bool bIncludeListenOption) const
{
    FString TravelURL = NormalizeMapPackagePath(LobbyMatchSettings.LobbyMapPath.IsEmpty()
        ? FString(DefaultHostLobbyMapPath)
        : LobbyMatchSettings.LobbyMapPath);

    const FString Options = GetLobbyTravelOptions(bIncludeListenOption);
    if (!Options.IsEmpty())
    {
        TravelURL += TEXT("?");
        TravelURL += Options;
    }
    return TravelURL;
}

FString UNetworkingManager::GetLobbyTravelOptions(bool bIncludeListenOption) const
{
    FString Options;
    if (bIncludeListenOption)
    {
        Options = TEXT("listen");
    }

    if (!LobbyMatchSettings.LobbyGameModePath.IsEmpty())
    {
        if (!Options.IsEmpty())
        {
            Options += TEXT("?");
        }
        Options += FString::Printf(TEXT("game=%s"), *LobbyMatchSettings.LobbyGameModePath);
    }
    return Options;
}

bool UNetworkingManager::FindExistingSession(FName& OutSessionName, bool& bOutIsLAN, IOnlineSessionPtr* OutSessions) const
{
    struct FModeCandidate
    {
        ENetworkSessionMode Mode;
        bool bIsLAN;
    };

    TArray<FModeCandidate> Candidates;
    Candidates.Add({ PreferredSessionMode, PreferredSessionMode == ENetworkSessionMode::LAN });

    const ENetworkSessionMode ActiveMode = bActiveSessionIsLAN ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online;
    if (Candidates[0].Mode != ActiveMode)
    {
        Candidates.Add({ ActiveMode, bActiveSessionIsLAN });
    }

    const ENetworkSessionMode OtherMode = PreferredSessionMode == ENetworkSessionMode::LAN
        ? ENetworkSessionMode::Online
        : ENetworkSessionMode::LAN;
    if (Candidates[0].Mode != OtherMode && (Candidates.Num() < 2 || Candidates[1].Mode != OtherMode))
    {
        Candidates.Add({ OtherMode, OtherMode == ENetworkSessionMode::LAN });
    }

    static const FName HellRunSessionName(TEXT("HellRunSession"));
    for (const FModeCandidate& Candidate : Candidates)
    {
        IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(Candidate.Mode);
        if (!Sessions.IsValid())
        {
            continue;
        }

        if (ActiveSessionName != NAME_None && Sessions->GetNamedSession(ActiveSessionName))
        {
            OutSessionName = ActiveSessionName;
            bOutIsLAN = Candidate.bIsLAN;
            if (OutSessions)
            {
                *OutSessions = Sessions;
            }
            return true;
        }

        if (Sessions->GetNamedSession(NAME_GameSession))
        {
            OutSessionName = NAME_GameSession;
            bOutIsLAN = Candidate.bIsLAN;
            if (OutSessions)
            {
                *OutSessions = Sessions;
            }
            return true;
        }

        if (Sessions->GetNamedSession(HellRunSessionName))
        {
            OutSessionName = HellRunSessionName;
            bOutIsLAN = Candidate.bIsLAN;
            if (OutSessions)
            {
                *OutSessions = Sessions;
            }
            return true;
        }
    }

    OutSessionName = NAME_None;
    bOutIsLAN = false;
    if (OutSessions)
    {
        *OutSessions = nullptr;
    }
    return false;
}

FName UNetworkingManager::FindActiveSessionName() const
{
    FName FoundSessionName = NAME_None;
    bool bFoundIsLAN = false;
    FindExistingSession(FoundSessionName, bFoundIsLAN, nullptr);
    return FoundSessionName;
}

bool UNetworkingManager::TravelHostToLobby()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const bool bAlreadyNetworkedServer = World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer;
    const FString LobbyURL = GetLobbyTravelURL(!bAlreadyNetworkedServer);
    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Opening listen lobby - %s"), *LobbyURL);

    const FString LobbyMapPath = NormalizeMapPackagePath(LobbyMatchSettings.LobbyMapPath.IsEmpty()
        ? FString(DefaultHostLobbyMapPath)
        : LobbyMatchSettings.LobbyMapPath);

    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        PC->ResetIgnoreMoveInput();
        PC->ResetIgnoreLookInput();
        if (PC->PlayerInput)
        {
            PC->PlayerInput->FlushPressedKeys();
        }
    }

    if (bAlreadyNetworkedServer)
    {
        return World->ServerTravel(LobbyURL, false);
    }

    UGameplayStatics::OpenLevel(World, FName(*LobbyMapPath), true, GetLobbyTravelOptions(true));
    return true;
}

bool UNetworkingManager::JoinSearchResult(const FOnlineSessionSearchResult& SearchResult, const FString& SourceSessionId)
{
    bActiveSessionIsLAN = SearchResult.Session.SessionSettings.bIsLANMatch;
    const ENetworkSessionMode SessionMode = bActiveSessionIsLAN ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online;
    IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(SessionMode);
    if (!Sessions.IsValid())
    {
        return false;
    }

    const FName RequestedSessionName = NAME_GameSession;
    if (Sessions->GetNamedSession(RequestedSessionName))
    {
        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Existing game session found, destroying before join of session %s."), *SourceSessionId);

        bJoinSessionAfterDestroy = true;
        PendingJoinSessionId = SourceSessionId;
        ActiveSessionName = RequestedSessionName;
        DestroySession_Implementation();
        return true;
    }

    ConfigureGameNetDriver(!bActiveSessionIsLAN);
    ActiveSessionName = RequestedSessionName;
    CurrentSessionId = SourceSessionId;
    SetConnectionState(ENetworkConnectionState::Connecting);

    if (JoinSessionCompleteHandle.IsValid())
    {
        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
    }

    JoinSessionCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &UNetworkingManager::OnJoinSessionComplete));

    if (Sessions->JoinSession(0, ActiveSessionName, SearchResult))
    {
        return true;
    }

    Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
    JoinSessionCompleteHandle.Reset();
    return false;
}

bool UNetworkingManager::ShouldSearchLAN() const
{
    return PreferredSessionMode == ENetworkSessionMode::LAN || bActiveSessionIsLAN || IsPIEWorld(GetWorld());
}

bool UNetworkingManager::ShouldForceLANForSession(bool bRequestedLAN) const
{
    return bRequestedLAN || PreferredSessionMode == ENetworkSessionMode::LAN || IsPIEWorld(GetWorld());
}

void UNetworkingManager::OpenPendingLevelAfterDestroy()
{
    if (!bOpenLevelAfterDestroy)
    {
        return;
    }

    const FString LevelPath = PendingOpenLevelPath.IsEmpty() ? TEXT("/Game/Levels/GameFlow/MainMenu") : PendingOpenLevelPath;
    bOpenLevelAfterDestroy = false;
    PendingOpenLevelPath.Empty();

    UWorld* World = PendingOpenLevelWorld.Get();
    PendingOpenLevelWorld.Reset();
    if (!World)
    {
        World = GetWorld();
    }

    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: Cannot open %s after leaving match, world is unavailable."), *LevelPath);
        return;
    }

    World->GetTimerManager().ClearTimer(LeaveMatchFallbackTimer);
    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Traveling to %s after session cleanup from world %s"), *LevelPath, *GetNameSafe(World));

    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        PC->ResetIgnoreMoveInput();
        PC->ResetIgnoreLookInput();
        if (PC->PlayerInput)
        {
            PC->PlayerInput->FlushPressedKeys();
        }

        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: ClientTravel %s via %s"), *LevelPath, *GetNameSafe(PC));
        PC->ClientTravel(LevelPath, TRAVEL_Absolute);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: No player controller for ClientTravel, falling back to OpenLevel %s"), *LevelPath);
    UGameplayStatics::OpenLevel(World, FName(*LevelPath), true);
}

int32 UNetworkingManager::CountConnectedPlayers() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return CurrentPlayerCount;
    }

    if (const AGameStateBase* GameState = World->GetGameState())
    {
        return FMath::Max(1, GameState->PlayerArray.Num());
    }

    int32 PlayerCount = 0;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        if (It->IsValid())
        {
            ++PlayerCount;
        }
    }

    return FMath::Max(PlayerCount, CurrentPlayerCount);
}

void UNetworkingManager::PopulateFriendsCacheFromOnlineSubsystem()
{
    if (!OnlineSubsystem)
    {
        return;
    }

    IOnlineFriendsPtr Friends = OnlineSubsystem->GetFriendsInterface();
    if (!Friends.IsValid())
    {
        return;
    }

    const FString ListName = EFriendsLists::ToString(EFriendsLists::Default);
    TArray<TSharedRef<FOnlineFriend>> OnlineFriends;
    if (!Friends->GetFriendsList(0, ListName, OnlineFriends))
    {
        Friends->ReadFriendsList(0, ListName);
        return;
    }

    FriendsCache.Empty();
    for (const TSharedRef<FOnlineFriend>& OnlineFriend : OnlineFriends)
    {
        FFriendData FriendData;
        FriendData.UserId = OnlineFriend->GetUserId()->ToString();
        FriendData.DisplayName = OnlineFriend->GetDisplayName();

        switch (OnlineFriend->GetInviteStatus())
        {
            case EInviteStatus::Accepted:
                FriendData.FriendshipState = EFriendshipState::Friends;
                break;
            case EInviteStatus::PendingInbound:
            case EInviteStatus::PendingOutbound:
                FriendData.FriendshipState = EFriendshipState::Pending;
                break;
            case EInviteStatus::Blocked:
                FriendData.FriendshipState = EFriendshipState::Blocked;
                break;
            default:
                FriendData.FriendshipState = EFriendshipState::None;
                break;
        }

        const FOnlineUserPresence& Presence = OnlineFriend->GetPresence();
        FriendData.bIsOnline = Presence.bIsOnline;
        FriendData.CurrentGame = Presence.bIsPlayingThisGame ? TEXT("Hell Run") : TEXT("");
        FriendData.CurrentStatus = Presence.Status.StatusStr;

        FriendsCache.Add(FriendData.UserId, FriendData);
    }
}

FString UNetworkingManager::GetDeathmatchTravelURL(bool bIncludeListenOption) const
{
    FString TravelURL = NormalizeMapPackagePath(LobbyMatchSettings.MapPath.IsEmpty()
        ? FString(TEXT("/Game/FirstPerson/Lvl_FirstPerson"))
        : LobbyMatchSettings.MapPath);

    if (bIncludeListenOption)
    {
        TravelURL += TEXT("?listen");
    }
    TravelURL += TEXT("?game=/Script/Hell_Run.FFAGameMode");
    TravelURL += FString::Printf(TEXT("?MatchMode=%s"), *GetMatchModeOptionString());
    TravelURL += FString::Printf(TEXT("?Teams=%d"), FMath::Clamp(LobbyMatchSettings.NumTeams, 1, 4));
    TravelURL += FString::Printf(TEXT("?KillLimit=%d"), FMath::Max(0, LobbyMatchSettings.KillLimit));
    TravelURL += FString::Printf(TEXT("?TeamScoreLimit=%d"), FMath::Max(0, LobbyMatchSettings.TeamScoreLimit));
    TravelURL += FString::Printf(TEXT("?TimeLimit=%d"), FMath::Max(0, LobbyMatchSettings.TimeLimitSeconds));
    TravelURL += FString::Printf(TEXT("?RespawnDelay=%d"), FMath::Max(0, LobbyMatchSettings.RespawnDelaySeconds));
    return TravelURL;
}

FString UNetworkingManager::NormalizeMapPackagePath(const FString& MapPath) const
{
    FString NormalizedPath = MapPath;
    NormalizedPath.TrimStartAndEndInline();

    int32 QueryIndex = INDEX_NONE;
    if (NormalizedPath.FindChar(TEXT('?'), QueryIndex))
    {
        NormalizedPath = NormalizedPath.Left(QueryIndex);
    }

    const int32 LastSlashIndex = NormalizedPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    const int32 DotIndex = NormalizedPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (DotIndex != INDEX_NONE && DotIndex > LastSlashIndex)
    {
        NormalizedPath = NormalizedPath.Left(DotIndex);
    }

    return NormalizedPath;
}

FString UNetworkingManager::GetMatchModeOptionString() const
{
    switch (LobbyMatchSettings.MatchMode)
    {
        case ENetworkMatchMode::Coop:
            return TEXT("Coop");
        case ENetworkMatchMode::Deathmatch:
            return TEXT("Deathmatch");
        case ENetworkMatchMode::TeamDeathmatch:
        default:
            return TEXT("TDM");
    }
}

void UNetworkingManager::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(bActiveSessionIsLAN ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online))
    {
        Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
    }

    if (!bWasSuccessful)
    {
        if (bActiveSessionIsLAN)
        {
            ShowNetworkDebugMessage(FString::Printf(TEXT("LAN Host: session '%s' failed to create"), *SessionName.ToString()), FColor::Red);
        }
        OnSessionError.Broadcast(TEXT("Session creation failed"));
        SetConnectionState(ENetworkConnectionState::Error);
        return;
    }

    CurrentSessionId = SessionName.ToString();
    CurrentPlayerCount = 1;
    if (bActiveSessionIsLAN)
    {
        ShowNetworkDebugMessage(FString::Printf(TEXT("LAN Host: session '%s' created"), *CurrentSessionId), FColor::Green);
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        SetConnectionState(ENetworkConnectionState::Hosting);
        OnSessionCreated.Broadcast(CurrentSessionId);
        return;
    }

    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        if (TravelHostToLobby())
        {
            CurrentConnectionState = ENetworkConnectionState::Hosting;
            return;
        }

        SetConnectionState(ENetworkConnectionState::Hosting);
        OnSessionCreated.Broadcast(CurrentSessionId);
    }));
}

void UNetworkingManager::OnFindSessionsComplete(bool bWasSuccessful)
{
    const bool bWasLANQuery = CurrentSessionSearch.IsValid() && CurrentSessionSearch->bIsLanQuery;
    if (IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(bWasLANQuery ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online))
    {
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
    }
    bFindSessionsInProgress = false;

    SetConnectionState(ENetworkConnectionState::Disconnected);

    if (!bWasSuccessful || !CurrentSessionSearch.IsValid())
    {
        bQuickJoinAfterFind = false;
        bQuickJoinRandomAfterFind = false;
        bQuickJoinCampaignOnly = false;
        OnSessionError.Broadcast(TEXT("Session search failed"));
        return;
    }

    // Log detailed info about search
    IOnlineSubsystem* ActiveSubsystem = GetOnlineSubsystemForMode(CurrentSessionSearch->bIsLanQuery ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online);
    const FName SubsystemName = ActiveSubsystem ? ActiveSubsystem->GetSubsystemName() : NAME_None;
    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: FindSessions complete on %s - bIsLanQuery=%s, Results=%d, bWasSuccessful=%s"),
        *SubsystemName.ToString(),
        CurrentSessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
        CurrentSessionSearch->SearchResults.Num(),
        bWasSuccessful ? TEXT("true") : TEXT("false"));

    if (CurrentSessionSearch->bIsLanQuery && CurrentSessionSearch->SearchResults.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: LAN search found no advertised sessions on %s."), *SubsystemName.ToString());
    }

    const FString QuickJoinLabel = GetQuickJoinLabel(CurrentSessionSearch->bIsLanQuery, bQuickJoinRandomAfterFind);
    ShowNetworkDebugMessage(FString::Printf(TEXT("%s: search complete: %d result(s)"),
        *QuickJoinLabel,
        CurrentSessionSearch->SearchResults.Num()),
        CurrentSessionSearch->SearchResults.Num() > 0 ? FColor::Green : FColor::Yellow);

    FString FirstJoinableSessionId;
    TArray<FString> JoinableSessionIds;

    for (int32 Index = 0; Index < CurrentSessionSearch->SearchResults.Num(); ++Index)
    {
        const FOnlineSessionSearchResult& Result = CurrentSessionSearch->SearchResults[Index];
        if (!Result.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: Skipping invalid session search result at index %d"), Index);
            continue;
        }

        if (!LooksLikeHellRunSession(Result))
        {
            FString FoundGameId;
            const bool bHasGameId = Result.Session.SessionSettings.Get(FName(HellRunGameIdKey), FoundGameId);
            UE_LOG(LogTemp, Verbose, TEXT("NetworkingManager: Skipping non-HellRun session result at index %d (GAME_ID=%s)"),
                Index,
                bHasGameId ? *FoundGameId : TEXT("<missing>"));
            continue;
        }

        const FString ResultId = FString::FromInt(Index);

        FString ServerName;
        if (!Result.Session.SessionSettings.Get(FName("SERVER_NAME"), ServerName) || ServerName.IsEmpty())
        {
            ServerName = FString::Printf(TEXT("Hell Run Match %d"), Index + 1);
        }

        FString MatchMode;
        Result.Session.SessionSettings.Get(FName("MATCH_MODE"), MatchMode);
        if (!IsSupportedMatchModeValue(MatchMode))
        {
            UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: Skipping session %d with unsupported match mode '%s'"), Index, *MatchMode);
            continue;
        }

        FString MapPath;
        Result.Session.SessionSettings.Get(SETTING_MAPNAME, MapPath);

        FString CampaignId;
        Result.Session.SessionSettings.Get(FName("CAMPAIGN_ID"), CampaignId);
        const bool bMatchesQuickJoin = !bQuickJoinCampaignOnly
            || !CampaignId.IsEmpty();

        FString LobbyMapPath;
        Result.Session.SessionSettings.Get(FName("LOBBY_MAP"), LobbyMapPath);

        FString LobbyGameModePath;
        Result.Session.SessionSettings.Get(FName("LOBBY_GAME_MODE"), LobbyGameModePath);

        int32 NumTeams = 2;
        Result.Session.SessionSettings.Get(FName("NUM_TEAMS"), NumTeams);

        int32 KillLimit = 20;
        Result.Session.SessionSettings.Get(FName("KILL_LIMIT"), KillLimit);

        int32 TeamScoreLimit = 20;
        Result.Session.SessionSettings.Get(FName("TEAM_SCORE_LIMIT"), TeamScoreLimit);

        int32 TimeLimitSeconds = 600;
        Result.Session.SessionSettings.Get(FName("TIME_LIMIT"), TimeLimitSeconds);

        int32 RespawnDelaySeconds = 3;
        Result.Session.SessionSettings.Get(FName("RESPAWN_DELAY"), RespawnDelaySeconds);

        FString Mutators;
        Result.Session.SessionSettings.Get(FName("MUTATORS"), Mutators);

        SearchResultsById.Add(ResultId, Result);

        int32 MaxSlots = Result.Session.SessionSettings.NumPublicConnections;
        if (MaxSlots <= 0)
        {
            MaxSlots = 10;
            UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: Session %d has NumPublicConnections=%d, defaulting to %d"),
                Index, Result.Session.SessionSettings.NumPublicConnections, MaxSlots);
        }

        const int32 OpenSlots = Result.Session.NumOpenPublicConnections;
        const int32 CurrentPlayers = FMath::Max(0, MaxSlots - OpenSlots);
        const bool bIsJoinable = OpenSlots > 0;
        if (bMatchesQuickJoin && FirstJoinableSessionId.IsEmpty()
            && OpenSlots > 0)
        {
            FirstJoinableSessionId = ResultId;
        }
        if (bIsJoinable)
        {
            JoinableSessionIds.Add(ResultId);
        }

        FHellRunSessionInfo SessionInfo;
        SessionInfo.SessionId = ResultId;
        SessionInfo.SessionName = ServerName;
        SessionInfo.MapPath = MapPath;
        SessionInfo.LobbyMapPath = LobbyMapPath;
        SessionInfo.MatchMode = MatchMode;
        SessionInfo.CurrentPlayers = CurrentPlayers;
        SessionInfo.MaxPlayers = MaxSlots;
        SessionInfo.OpenPublicConnections = OpenSlots;
        SessionInfo.PingInMs = Result.PingInMs;
        SessionInfo.bIsLAN = Result.Session.SessionSettings.bIsLANMatch;
        SessionInfo.bIsJoinable = bIsJoinable;
        SessionInfo.MatchSettings.LobbyMapPath = LobbyMapPath;
        SessionInfo.MatchSettings.MapPath = MapPath;
        SessionInfo.MatchSettings.LobbyGameModePath = LobbyGameModePath;
        SessionInfo.MatchSettings.MaxPlayers = MaxSlots;
        SessionInfo.MatchSettings.NumTeams = NumTeams;
        SessionInfo.MatchSettings.KillLimit = KillLimit;
        SessionInfo.MatchSettings.TeamScoreLimit = TeamScoreLimit;
        SessionInfo.MatchSettings.TimeLimitSeconds = TimeLimitSeconds;
        SessionInfo.MatchSettings.RespawnDelaySeconds = RespawnDelaySeconds;
        TArray<FString> MutatorIds;
        Mutators.ParseIntoArray(MutatorIds, TEXT(","), true);
        for (const FString& MutatorId : MutatorIds)
        {
            FHellRunMutatorSetting MutatorSetting;
            MutatorSetting.MutatorId = FName(*MutatorId);
            MutatorSetting.DisplayName = MutatorId;
            MutatorSetting.bEnabled = true;
            SessionInfo.MatchSettings.Mutators.Add(MutatorSetting);
        }
        if (MatchMode.Equals(TEXT("Coop"), ESearchCase::IgnoreCase))
        {
            SessionInfo.MatchSettings.MatchMode = ENetworkMatchMode::Coop;
        }
        else if (MatchMode.Equals(TEXT("Deathmatch"), ESearchCase::IgnoreCase))
        {
            SessionInfo.MatchSettings.MatchMode = ENetworkMatchMode::Deathmatch;
        }
        else
        {
            SessionInfo.MatchSettings.MatchMode = ENetworkMatchMode::TeamDeathmatch;
        }
        CachedFoundSessions.Add(SessionInfo);

        OnSessionFound.Broadcast(ResultId, CurrentPlayers);

        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Found session id=%s name=%s mode=%s players=%d/%d"),
            *ResultId, *ServerName, *MatchMode, CurrentPlayers, MaxSlots);
    }

    if (bQuickJoinAfterFind)
    {
        bQuickJoinAfterFind = false;

        const bool bWasCampaignQuickJoin = bQuickJoinCampaignOnly;
        bQuickJoinCampaignOnly = false;

        if (!FirstJoinableSessionId.IsEmpty())
        {
            ShowNetworkDebugMessage(FString::Printf(TEXT("%s: found joinable session %s, joining"), *GetQuickJoinLabel(CurrentSessionSearch->bIsLanQuery, false), *FirstJoinableSessionId), FColor::Green);
            JoinSession_Implementation(FirstJoinableSessionId);
        }
        else
        {
            ShowNetworkDebugMessage(FString::Printf(TEXT("%s: no advertised sessions found"), *GetQuickJoinLabel(CurrentSessionSearch->bIsLanQuery, false)), FColor::Red);
            OnSessionError.Broadcast(bWasCampaignQuickJoin
                ? TEXT("No open campaign sessions found")
                : TEXT("No open sessions found"));
            SetConnectionState(ENetworkConnectionState::Disconnected);
        }
    }

    if (bQuickJoinRandomAfterFind)
    {
        bQuickJoinRandomAfterFind = false;

        if (JoinableSessionIds.Num() > 0)
        {
            const FString SelectedSessionId = JoinableSessionIds[FMath::RandRange(0, JoinableSessionIds.Num() - 1)];
            ShowNetworkDebugMessage(FString::Printf(TEXT("%s: found joinable session %s, joining"), *GetQuickJoinLabel(CurrentSessionSearch->bIsLanQuery, true), *SelectedSessionId), FColor::Green);
            JoinSession_Implementation(SelectedSessionId);
        }
        else
        {
            ShowNetworkDebugMessage(FString::Printf(TEXT("%s: no advertised sessions found"), *GetQuickJoinLabel(CurrentSessionSearch->bIsLanQuery, true)), FColor::Red);
            OnSessionError.Broadcast(TEXT("No open sessions found"));
            SetConnectionState(ENetworkConnectionState::Disconnected);
        }
    }
}

void UNetworkingManager::OnFindFriendSessionComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& SearchResults)
{
    if (IOnlineSessionPtr Sessions = GetSessionInterface())
    {
        Sessions->ClearOnFindFriendSessionCompleteDelegate_Handle(LocalUserNum, FindFriendSessionCompleteHandle);
    }
    FindFriendSessionCompleteHandle.Reset();

    if (!bWasSuccessful || SearchResults.Num() == 0)
    {
        OnSessionError.Broadcast(TEXT("No joinable Steam friend session found"));
        SetConnectionState(ENetworkConnectionState::Disconnected);
        return;
    }

    for (const FOnlineSessionSearchResult& SearchResult : SearchResults)
    {
        if (SearchResult.IsValid() && SearchResult.Session.NumOpenPublicConnections > 0)
        {
            const FString FriendSessionId = FString::Printf(TEXT("SteamFriend_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
            UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Joining Steam friend session %s"), *FriendSessionId);
            if (!JoinSearchResult(SearchResult, FriendSessionId))
            {
                OnSessionError.Broadcast(TEXT("Failed to join Steam friend session"));
                SetConnectionState(ENetworkConnectionState::Error);
            }
            return;
        }
    }

    OnSessionError.Broadcast(TEXT("Steam friend has no open session slots"));
    SetConnectionState(ENetworkConnectionState::Disconnected);
}

void UNetworkingManager::OnSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
    if (!bWasSuccessful || !InviteResult.IsValid())
    {
        OnSessionError.Broadcast(TEXT("Steam invite accept failed"));
        return;
    }

    const FString InviteSessionId = FString::Printf(TEXT("SteamInvite_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Steam invite accepted by controller %d, joining %s"), ControllerId, *InviteSessionId);

    if (!JoinSearchResult(InviteResult, InviteSessionId))
    {
        OnSessionError.Broadcast(TEXT("Failed to join accepted Steam invite"));
        SetConnectionState(ENetworkConnectionState::Error);
    }
}

void UNetworkingManager::OnSessionInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId, const FOnlineSessionSearchResult& InviteResult)
{
    const FString InviteId = FString::Printf(TEXT("SteamInvite_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    PendingInviteResultsById.Add(InviteId, InviteResult);

    FGameInviteData NewInvite;
    NewInvite.InviteId = InviteId;
    NewInvite.FromUserId = FromId.ToString();
    NewInvite.FromDisplayName = FromId.ToString();
    NewInvite.SessionId = InviteId;
    NewInvite.Status = EGameInviteStatus::Pending;
    NewInvite.CreatedTime = FDateTime::Now();
    NewInvite.Message = FString::Printf(TEXT("Steam invite from %s"), *NewInvite.FromDisplayName);
    InvitesCache.Add(InviteId, NewInvite);

    UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Steam invite received from %s for app %s as invite %s"),
        *NewInvite.FromUserId, *AppId, *InviteId);
    OnGameInviteReceived.Broadcast(NewInvite.FromUserId, InviteId);
}

void UNetworkingManager::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(bActiveSessionIsLAN ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online))
    {
        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);

        FString ConnectString;
        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: JoinSession complete for %s with result %d"),
            *SessionName.ToString(), static_cast<int32>(Result));

        if (Result == EOnJoinSessionCompleteResult::Success && Sessions->GetResolvedConnectString(SessionName, ConnectString))
        {
            UE_LOG(LogTemp, Log, TEXT("NetworkingManager: JoinSession resolved connect string %s"), *ConnectString);
            UE_LOG(LogTemp, Log, TEXT("NetworkingManager: JoinSession connect type = %s"),
                IsSteamConnectString(ConnectString) ? TEXT("SteamP2P") : TEXT("IP"));
            ConfigureGameNetDriver(IsSteamConnectString(ConnectString));
            CurrentPlayerCount = 1;
            SetConnectionState(ENetworkConnectionState::Connected);

            if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
            {
                UE_LOG(LogTemp, Log, TEXT("NetworkingManager: ClientTravel via %s using %s net driver expectation"),
                    *GetNameSafe(PC),
                    IsSteamConnectString(ConnectString) ? TEXT("SteamNetDriver") : TEXT("IpNetDriver"));
                PC->ClientTravel(ConnectString, TRAVEL_Absolute);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: JoinSession has no player controller available for ClientTravel"));
            }
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("NetworkingManager: JoinSession did not resolve a connect string for %s"),
            *SessionName.ToString());
    }

    OnSessionError.Broadcast(TEXT("Join session failed"));
    SetConnectionState(ENetworkConnectionState::Error);
}

void UNetworkingManager::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (IOnlineSessionPtr Sessions = GetSessionInterfaceForMode(bActiveSessionIsLAN ? ENetworkSessionMode::LAN : ENetworkSessionMode::Online))
    {
        Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
    }

    CurrentSessionId = TEXT("");
    CurrentPlayerCount = 0;
    ActiveSessionName = NAME_None;
    bActiveSessionIsLAN = false;
    SetConnectionState(ENetworkConnectionState::Disconnected);

    if (bCreateSessionAfterDestroy)
    {
        const FString SessionNameToCreate = PendingSessionName;
        const int32 MaxPlayersToCreate = PendingMaxPlayers;
        const bool bIsLANToCreate = bPendingSessionIsLAN;

        bCreateSessionAfterDestroy = false;
        PendingSessionName.Empty();
        PendingMaxPlayers = 4;
        bPendingSessionIsLAN = false;

        CreateSession_Implementation(SessionNameToCreate, MaxPlayersToCreate, bIsLANToCreate);
        return;
    }

    if (bJoinSessionAfterDestroy)
    {
        const FString SessionIdToJoin = PendingJoinSessionId;

        bJoinSessionAfterDestroy = false;
        PendingJoinSessionId.Empty();

        UE_LOG(LogTemp, Log, TEXT("NetworkingManager: Previous session destroyed, retrying join for session %s."), *SessionIdToJoin);
        JoinSession_Implementation(SessionIdToJoin);
        return;
    }

    OpenPendingLevelAfterDestroy();
}

void UNetworkingManager::AddChatMessage(const FChatMessage& Message)
{
    TArray<FChatMessage>* TargetHistory = nullptr;

    switch (Message.MessageType)
    {
        case EChatMessageType::Global:
            TargetHistory = &GlobalChatHistory;
            break;
        case EChatMessageType::Team:
            TargetHistory = &TeamChatHistory;
            break;
        case EChatMessageType::Proximity:
            TargetHistory = &ProximityChatHistory;
            break;
        case EChatMessageType::Whisper:
            TargetHistory = &WhisperChatHistory;
            break;
        case EChatMessageType::System:
            TargetHistory = &SystemChatHistory;
            break;
    }

    if (TargetHistory)
    {
        TargetHistory->Add(Message);

        // Keep history size manageable (max 100 per type)
        if (TargetHistory->Num() > 100)
        {
            TargetHistory->RemoveAt(0);
        }
    }
}

void UNetworkingManager::UpdateProximityPlayers()
{
    // This would normally iterate through all players in the world
    // and check their distance from the local player
}

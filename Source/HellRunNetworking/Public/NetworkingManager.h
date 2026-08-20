#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/NetworkingInterface.h"
#include "Interfaces/SocialInterface.h"
#include "Interfaces/ChatInterface.h"
#include "NetworkingTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "NetworkingManager.generated.h"

class IOnlineSubsystem;
class FOnlineSessionSearch;
class IOnlineSession;
class UWorld;

typedef TSharedPtr<IOnlineSession, ESPMode::ThreadSafe> IOnlineSessionPtr;

/**
 * ANetworkingManager
 * 
 * Central networking manager that implements all networking interfaces.
 * Handles session management, social features, and proximity chat.
 * 
 * Can be extended for custom implementations or backend-specific behavior.
 */
UCLASS()
class HELLRUNNETWORKING_API UNetworkingManager : public UGameInstanceSubsystem, 
    public INetworkingInterface, 
    public ISocialInterface, 
    public IChatInterface
{
    GENERATED_BODY()

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === INetworkingInterface Implementation ===

    virtual void CreateSession_Implementation(const FString& SessionName, int32 MaxPlayers, bool bIsLAN) override;
    virtual void FindSessions_Implementation(int32 MaxSearchResults) override;
    virtual void JoinSession_Implementation(const FString& SessionId) override;
    virtual void StartSession_Implementation() override;
    virtual void DestroySession_Implementation() override;

    UFUNCTION(BlueprintCallable, Category="Networking|Lobby")
    void SetLobbyMatchSettings(const FLobbyMatchSettings& NewSettings);

    UFUNCTION(BlueprintPure, Category="Networking|Lobby")
    FLobbyMatchSettings GetLobbyMatchSettings() const { return LobbyMatchSettings; }

    UFUNCTION(BlueprintCallable, Category="Networking|Mode")
    void SetNetworkSessionMode(ENetworkSessionMode NewMode);

    UFUNCTION(BlueprintPure, Category="Networking|Mode")
    ENetworkSessionMode GetNetworkSessionMode() const { return PreferredSessionMode; }

    UFUNCTION(BlueprintPure, Category="Networking|Mode")
    bool IsLANMode() const { return PreferredSessionMode == ENetworkSessionMode::LAN; }

    UFUNCTION(BlueprintCallable, Category="Networking|Sessions")
    void QuickJoinFirstSession(int32 MaxSearchResults = 20);

    /** Find and join the first session explicitly advertising a campaign. */
    UFUNCTION(BlueprintCallable, Category="Networking|Sessions")
    void QuickJoinCampaignSession(int32 MaxSearchResults = 20);

    /** Mark the active hosted lobby as a campaign before campaign travel. */
    UFUNCTION(BlueprintCallable, Category="Networking|Sessions")
    bool AdvertiseCampaignSession(const FString& CampaignId, const FString& FirstMapPath);

    /** Adds campaign metadata to the next session at creation time. */
    UFUNCTION(BlueprintCallable, Category="Networking|Sessions")
    void ConfigurePendingCampaignSession(const FString& CampaignId, const FString& FirstMapPath);

    UFUNCTION(BlueprintCallable, Category="Networking|Sessions")
    void QuickJoinRandomSession(int32 MaxSearchResults = 20);

    UFUNCTION(BlueprintPure, Category="Networking|Sessions")
    void GetFoundSessions(TArray<FHellRunSessionInfo>& OutSessions) const;

    UFUNCTION(BlueprintCallable, Category="Networking|Sessions")
    void LeaveMatchAndOpenLevel(const FString& LevelPath = TEXT("/Game/Levels/GameFlow/MainMenu"));

    void LeaveMatchAndOpenLevelFromWorld(UWorld* TravelWorld, const FString& LevelPath = TEXT("/Game/Levels/GameFlow/MainMenu"));

    UFUNCTION(BlueprintCallable, Category="Networking|Party")
    void JoinParty(const FString& PartyId);

    UFUNCTION(BlueprintCallable, Category="Networking|Party")
    void LeaveParty();

    UFUNCTION(BlueprintCallable, Category="Networking|Steam")
    bool OpenSteamInviteOverlay();

    UFUNCTION(BlueprintCallable, Category="Networking|Steam")
    bool InviteSteamFriend(const FString& FriendUserId);

    UFUNCTION(BlueprintCallable, Category="Networking|Steam")
    bool JoinSteamFriend(const FString& FriendUserId);

    virtual ENetworkConnectionState GetConnectionState_Implementation() const override;
    virtual bool IsConnected_Implementation() const override;
    virtual bool IsHosting_Implementation() const override;
    virtual int32 GetPlayerCount_Implementation() const override;
    virtual FString GetCurrentSessionId_Implementation() const override;

    // === ISocialInterface Implementation ===

    virtual void SendFriendRequest_Implementation(const FString& UserId) override;
    virtual void AcceptFriendRequest_Implementation(const FString& UserId) override;
    virtual void DeclineFriendRequest_Implementation(const FString& UserId) override;
    virtual void RemoveFriend_Implementation(const FString& UserId) override;
    virtual void BlockPlayer_Implementation(const FString& UserId) override;
    virtual void UnblockPlayer_Implementation(const FString& UserId) override;
    virtual void GetFriendsList_Implementation(TArray<FFriendData>& OutFriends) override;
    virtual EFriendshipState GetFriendshipStatus_Implementation(const FString& UserId) const override;

    virtual void SendGameInvite_Implementation(const FString& UserId, const FString& SessionId, const FString& Message) override;
    virtual void AcceptGameInvite_Implementation(const FString& InviteId) override;
    virtual void DeclineGameInvite_Implementation(const FString& InviteId) override;
    virtual void GetPendingInvites_Implementation(TArray<FGameInviteData>& OutInvites) override;

    virtual void SetPlayerStatus_Implementation(const FString& Status) override;
    virtual void UpdateGameInfo_Implementation(const FString& GameName, const FString& SessionId) override;

    // === IChatInterface Implementation ===

    virtual void SendChatMessage_Implementation(const FString& Message, EChatMessageType MessageType) override;
    virtual void GetChatHistory_Implementation(EChatMessageType MessageType, TArray<FChatMessage>& OutMessages) override;
    virtual void ClearChatHistory_Implementation(EChatMessageType MessageType) override;

    virtual void StartVoiceTransmission_Implementation() override;
    virtual void StopVoiceTransmission_Implementation() override;
    virtual bool IsTransmitting_Implementation() const override;

    virtual void MutePlayer_Implementation(const FString& UserId) override;
    virtual void UnmutePlayer_Implementation(const FString& UserId) override;
    virtual void MuteAll_Implementation() override;
    virtual void UnmuteAll_Implementation() override;
    virtual bool IsPlayerMuted_Implementation(const FString& UserId) const override;

    virtual void SetProximityChatRange_Implementation(float NewRange) override;
    virtual float GetProximityChatRange_Implementation() const override;
    virtual void GetPlayersInProximity_Implementation(TArray<FProximityVoiceInfo>& OutPlayers) override;

    // === Blueprint Delegates ===

    UPROPERTY(BlueprintAssignable, Category="Networking|Events")
    FOnConnectionStateChanged OnConnectionStateChanged;

    UPROPERTY(BlueprintAssignable, Category="Networking|Events")
    FOnSessionCreated OnSessionCreated;

    UPROPERTY(BlueprintAssignable, Category="Networking|Events")
    FOnSessionFound OnSessionFound;

    UPROPERTY(BlueprintAssignable, Category="Networking|Events")
    FOnSessionError OnSessionError;

    UPROPERTY(BlueprintAssignable, Category="Social|Events")
    FOnSocialFriendRequestReceived OnFriendRequestReceived;

    UPROPERTY(BlueprintAssignable, Category="Social|Events")
    FOnSocialGameInviteReceived OnGameInviteReceived;

    UPROPERTY(BlueprintAssignable, Category="Chat|Events")
    FOnChatMessageReceived OnChatMessageReceived;

    UPROPERTY(BlueprintAssignable, Category="Chat|Events")
    FOnVoiceStateChanged OnVoiceStateChanged;

    UPROPERTY(BlueprintAssignable, Category="Chat|Events")
    FOnPlayerMuted OnPlayerMuted;

protected:

    // Connection state
    UPROPERTY()
    ENetworkConnectionState CurrentConnectionState = ENetworkConnectionState::Disconnected;

    UPROPERTY()
    FString CurrentSessionId;

    UPROPERTY()
    int32 CurrentPlayerCount = 0;

    UPROPERTY()
    int32 MaxPlayerCount = 4;

    UPROPERTY()
    FLobbyMatchSettings LobbyMatchSettings;

    UPROPERTY()
    FString PendingCampaignId;

    UPROPERTY()
    ENetworkSessionMode PreferredSessionMode = ENetworkSessionMode::Online;

    UPROPERTY()
    TArray<FHellRunSessionInfo> CachedFoundSessions;

    // Social data
    UPROPERTY()
    TMap<FString, FFriendData> FriendsCache;

    UPROPERTY()
    TMap<FString, FGameInviteData> InvitesCache;

    // Chat data
    UPROPERTY()
    TArray<FChatMessage> GlobalChatHistory;

    UPROPERTY()
    TArray<FChatMessage> TeamChatHistory;

    UPROPERTY()
    TArray<FChatMessage> ProximityChatHistory;

    UPROPERTY()
    TArray<FChatMessage> WhisperChatHistory;

    UPROPERTY()
    TArray<FChatMessage> SystemChatHistory;

    // Voice state
    UPROPERTY()
    EVoiceState CurrentVoiceState = EVoiceState::Idle;

    UPROPERTY()
    TSet<FString> MutedPlayers;

    UPROPERTY()
    bool bAllMuted = false;

    UPROPERTY()
    float ProximityChatRange = 5000.0f;

    UPROPERTY()
    TMap<FString, FProximityVoiceInfo> PlayersInProximity;

    IOnlineSubsystem* OnlineSubsystem = nullptr;
    TSharedPtr<FOnlineSessionSearch> CurrentSessionSearch;
    TMap<FString, FOnlineSessionSearchResult> SearchResultsById;
    TMap<FString, FOnlineSessionSearchResult> PendingInviteResultsById;
    FName ActiveSessionName = NAME_None;
    bool bActiveSessionIsLAN = false;

    FDelegateHandle CreateSessionCompleteHandle;
    FDelegateHandle FindSessionsCompleteHandle;
    FDelegateHandle FindFriendSessionCompleteHandle;
    FDelegateHandle JoinSessionCompleteHandle;
    FDelegateHandle DestroySessionCompleteHandle;
    FDelegateHandle SessionUserInviteAcceptedHandle;
    FDelegateHandle SessionInviteReceivedHandle;
    FTimerHandle LeaveMatchFallbackTimer;

    bool bCreateSessionAfterDestroy = false;
    bool bJoinSessionAfterDestroy = false;
    bool bQuickJoinAfterFind = false;
    bool bQuickJoinRandomAfterFind = false;
    bool bQuickJoinCampaignOnly = false;
    bool bFindSessionsInProgress = false;
    bool bOpenLevelAfterDestroy = false;
    FString PendingSessionName;
    FString PendingJoinSessionId;
    FString PendingOpenLevelPath;
    TWeakObjectPtr<UWorld> PendingOpenLevelWorld;
    int32 PendingMaxPlayers = 4;
    bool bPendingSessionIsLAN = false;

    // Helper methods
    IOnlineSubsystem* GetOnlineSubsystemForMode(ENetworkSessionMode Mode) const;
    IOnlineSubsystem* GetActiveOnlineSubsystem() const;
    IOnlineSessionPtr GetSessionInterfaceForMode(ENetworkSessionMode Mode) const;
    IOnlineSessionPtr GetSessionInterface() const;
    bool FindExistingSession(FName& OutSessionName, bool& bOutIsLAN, IOnlineSessionPtr* OutSessions = nullptr) const;
    FString GetLobbyTravelURL(bool bIncludeListenOption = true) const;
    FString GetLobbyTravelOptions(bool bIncludeListenOption = true) const;
    FString GetDeathmatchTravelURL(bool bIncludeListenOption = true) const;
    FString GetMatchModeOptionString() const;
    FString NormalizeMapPackagePath(const FString& MapPath) const;
    FName FindActiveSessionName() const;
    bool TravelHostToLobby();
    bool JoinSearchResult(const FOnlineSessionSearchResult& SearchResult, const FString& SourceSessionId);
    bool ShouldSearchLAN() const;
    bool ShouldForceLANForSession(bool bRequestedLAN) const;
    void BeginQuickJoinFirstSession(int32 MaxSearchResults, bool bCampaignOnly);
    void OpenPendingLevelAfterDestroy();
    int32 CountConnectedPlayers() const;
    void PopulateFriendsCacheFromOnlineSubsystem();
    void SetConnectionState(ENetworkConnectionState NewState);
    void AddChatMessage(const FChatMessage& Message);
    void UpdateProximityPlayers();

    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnFindFriendSessionComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& SearchResults);
    void OnSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
    void OnSessionInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId, const FOnlineSessionSearchResult& InviteResult);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
};

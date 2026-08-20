#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NetworkingTypes.h"
#include "NetworkingInterface.generated.h"

/**
 * INetworkingInterface
 * 
 * Core interface for networking functionality. Defines all session and connection methods.
 * Implementations can override these to support different backend services (EOS, Steam, etc.)
 */
UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class UNetworkingInterface : public UInterface
{
    GENERATED_UINTERFACE_BODY()
};

class HELLRUNNETWORKING_API INetworkingInterface
{
    GENERATED_IINTERFACE_BODY()

public:

    // === Session Management ===

    /**
     * Create a new multiplayer session
     * @param SessionName Name of the session
     * @param MaxPlayers Maximum number of players
     * @param bIsLAN Whether this is a LAN session
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|Sessions")
    void CreateSession(const FString& SessionName, int32 MaxPlayers, bool bIsLAN);

    /**
     * Find available game sessions
     * @param MaxSearchResults Maximum number of results to return
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|Sessions")
    void FindSessions(int32 MaxSearchResults);

    /**
     * Join an existing session
     * @param SessionId Session identifier to join
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|Sessions")
    void JoinSession(const FString& SessionId);

    /**
     * Start the session (begin gameplay)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|Sessions")
    void StartSession();

    /**
     * Destroy current session
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|Sessions")
    void DestroySession();

    // === State Queries ===

    /**
     * Get current connection state
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|State")
    ENetworkConnectionState GetConnectionState() const;

    /**
     * Check if connected to a session
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|State")
    bool IsConnected() const;

    /**
     * Check if hosting a session
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|State")
    bool IsHosting() const;

    /**
     * Get current player count in session
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|State")
    int32 GetPlayerCount() const;

    /**
     * Get current session identifier
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Networking|State")
    FString GetCurrentSessionId() const;
};

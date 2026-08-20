#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NetworkingTypes.h"
#include "ChatInterface.generated.h"

/**
 * IChatInterface
 * 
 * Interface for text and voice chat functionality with proximity-based mechanics.
 * Implementations provide chat messaging and voice transmission.
 */
UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class UChatInterface : public UInterface
{
    GENERATED_UINTERFACE_BODY()
};

class HELLRUNNETWORKING_API IChatInterface
{
    GENERATED_IINTERFACE_BODY()

public:

    // === Text Chat ===

    /**
     * Send a text chat message
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Text")
    void SendChatMessage(const FString& Message, EChatMessageType MessageType);

    /**
     * Get chat history for a message type
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Text")
    void GetChatHistory(EChatMessageType MessageType, TArray<FChatMessage>& OutMessages);

    /**
     * Clear chat history
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Text")
    void ClearChatHistory(EChatMessageType MessageType);

    // === Voice Chat ===

    /**
     * Start voice transmission (Push To Talk)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Voice")
    void StartVoiceTransmission();

    /**
     * Stop voice transmission
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Voice")
    void StopVoiceTransmission();

    /**
     * Check if currently transmitting voice
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Voice")
    bool IsTransmitting() const;

    // === Player Muting ===

    /**
     * Mute a specific player's voice
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Mute")
    void MutePlayer(const FString& UserId);

    /**
     * Unmute a specific player's voice
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Mute")
    void UnmutePlayer(const FString& UserId);

    /**
     * Mute all players
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Mute")
    void MuteAll();

    /**
     * Unmute all players
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Mute")
    void UnmuteAll();

    /**
     * Check if a player is muted
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Mute")
    bool IsPlayerMuted(const FString& UserId) const;

    // === Proximity Management ===

    /**
     * Set the proximity chat range
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Proximity")
    void SetProximityChatRange(float NewRange);

    /**
     * Get current proximity chat range
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Proximity")
    float GetProximityChatRange() const;

    /**
     * Get players in proximity
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Chat|Proximity")
    void GetPlayersInProximity(TArray<FProximityVoiceInfo>& OutPlayers);
};

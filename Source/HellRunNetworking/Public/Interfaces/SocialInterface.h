#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NetworkingTypes.h"
#include "SocialInterface.generated.h"

/**
 * ISocialInterface
 * 
 * Interface for social features including friends, invites, and player relationships.
 * Implementations provide friend management and game invite functionality.
 */
UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class USocialInterface : public UInterface
{
    GENERATED_UINTERFACE_BODY()
};

class HELLRUNNETWORKING_API ISocialInterface
{
    GENERATED_IINTERFACE_BODY()

public:

    // === Friend Management ===

    /**
     * Send a friend request to a player
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Friends")
    void SendFriendRequest(const FString& UserId);

    /**
     * Accept a friend request
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Friends")
    void AcceptFriendRequest(const FString& UserId);

    /**
     * Decline a friend request
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Friends")
    void DeclineFriendRequest(const FString& UserId);

    /**
     * Remove a friend
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Friends")
    void RemoveFriend(const FString& UserId);

    /**
     * Block a player
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Friends")
    void BlockPlayer(const FString& UserId);

    /**
     * Unblock a player
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Friends")
    void UnblockPlayer(const FString& UserId);

    /**
     * Get list of friends
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Friends")
    void GetFriendsList(TArray<FFriendData>& OutFriends);

    /**
     * Get friendship status with a player
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Friends")
    EFriendshipState GetFriendshipStatus(const FString& UserId) const;

    // === Game Invites ===

    /**
     * Send a game invite to a player
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Invites")
    void SendGameInvite(const FString& UserId, const FString& SessionId, const FString& Message);

    /**
     * Accept a game invite
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Invites")
    void AcceptGameInvite(const FString& InviteId);

    /**
     * Decline a game invite
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Invites")
    void DeclineGameInvite(const FString& InviteId);

    /**
     * Get pending game invites
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Invites")
    void GetPendingInvites(TArray<FGameInviteData>& OutInvites);

    // === Status ===

    /**
     * Set player status message
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Status")
    void SetPlayerStatus(const FString& Status);

    /**
     * Update what game the player is in
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Social|Status")
    void UpdateGameInfo(const FString& GameName, const FString& SessionId);
};

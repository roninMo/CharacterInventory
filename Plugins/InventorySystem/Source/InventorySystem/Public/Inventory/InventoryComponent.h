// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryInformation.h"
#include "InventoryInterface.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(InventoryLog, Log, All);

#define OP__IndividualInventory EInventoryOperation::OP_IndividualInventory
#define OP__InventoryToContainer EInventoryOperation::OP_InventoryToContainer
#define OP__ContainerToInventory EInventoryOperation::OP_ContainerToInventory
#define OP__InventoryToInventory EInventoryOperation::OP_InventoryToInventory


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryAddtionFailureDelegate, const FName&, Id, TScriptInterface<IInventoryItemInterface>, SpawnedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryAddtionSuccessDelegate, const F_Item&, ItemData, TScriptInterface<IInventoryItemInterface>, SpawnedItem);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInventoryItemTransferFailureDelegate, const FGuid&, Id, const TScriptInterface<IInventoryInterface>, OtherInventory, const bool, bFromThisInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInventoryItemTransferSuccessDelegate, const FGuid&, Id, const TScriptInterface<IInventoryInterface>, OtherInventory, const bool, bFromThisInventory);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryItemRemovalFailureDelegate, const FGuid&, Id, TScriptInterface<IInventoryItemInterface>, SpawnedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryItemRemovalSuccessDelegate, const F_Item&, ItemData, TScriptInterface<IInventoryItemInterface>, SpawnedItem);



UCLASS( Blueprintable, ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent) )
class INVENTORYSYSTEM_API UInventoryComponent : public UActorComponent, public IInventoryInterface
{
	GENERATED_BODY()
	// TODO: Technically this doesn't account for dedicated servers yet, however there shouldn't be any problems

protected:
	/** I've divided the inventory into maps for quick retrieval, however you're able to build with arrays and other things if you want to adjust the logic */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FGuid, F_Item> QuestItems;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FGuid, F_Item> CommonItems;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FGuid, F_Item> Weapons;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FGuid, F_Item> Armors;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FGuid, F_Item> Materials;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FGuid, F_Item> Notes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) UDataTable* ItemDatabase;

	/** References and stored information */
	UPROPERTY(BlueprintReadWrite) TObjectPtr<ACharacter> Character;

	/** Other */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDebugInventory;

	
protected:
	UInventoryComponent();
	virtual void BeginPlay() override;
	
	
//----------------------------------------------------------------------------------//
// Item Addition																	//
//----------------------------------------------------------------------------------//
public:
	/**
	 * Sends the information to the server to add an item to the inventory, and handles each of the different scenarios for this action. \n\n
	 * There's multiple delegate functions in response to each scenario, and if an error occurs while updating the inventory there's safeguards in place to revert the logic \n\n
	 *
	 * Order of operations is TryAddItem:
	 *		- AddItemPendingClientLogic
	 *		- Server_TryAddItem -> HandleAddItem
	 *			- Client_AddItemResponse
	 *				- HandleItemAdditionFail
	 *				- HandleItemAdditionSuccess
	 * 
	 * @param Id										The database id for this item. (If the item isn't already spawned in the world, retrieve the object from the database)
	 * @param InventoryItemInterface					The reference to the actor spawned in the world, if there is one (and you want it to be deleted upon completion).
	 * @param Type										The item type (used for item allocation)
	 * @returns		True if the item was found in the database and successfully added to the inventory.
	 * 
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * @note For handling the ui, I'd add delegates on the response functions for update notifications on the player's inventory
	 */
	virtual bool TryAddItem_Implementation(const FName Id, UObject* InventoryItemInterface, const EItemType Type) override;

	
protected:
	/**
	 * What should happen assuming the item is added
	 * @note If the item isn't successfully added then @ref HandleItemAdditionFail should be called, otherwise @ref HandleItemAdditionSuccess is called
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * */
	virtual void AddItemPendingClientLogic_Implementation(const FName Id, UObject* InventoryItemInterface, const EItemType Type) override;
	
	/** Server/Client procedure calls are not valid on interfaces, these need to be handled in the actual implementation */
	UFUNCTION(Server, Reliable) virtual void Server_TryAddItem(FName Id, UObject* InventoryInterface, const EItemType Type);
	UFUNCTION(Client, Reliable) virtual void Client_AddItemResponse(const bool bSuccess, FName Id, UObject* InventoryInterface, const EItemType Type);
	
	/**
	 * The actual logic that handles adding the item to an inventory component
	 * @return The id of the newly created item
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual FGuid HandleAddItem_Implementation(const FName Id, UObject* InventoryItemInterface, const EItemType Type) override;
	
	/**
	 * If the item was not added to the inventory
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual void HandleItemAdditionFail_Implementation(const FName Id, UObject* InventoryItemInterface, const EItemType Type) override;

	/** Delegate function for when an item failed to be added to the inventory. Helpful for ui elements to keep track of inventory updates */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Operations") FInventoryAddtionFailureDelegate OnInventoryItemAdditionFailure;
	
	/**
	 * If the item was successfully added to the inventory
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual void HandleItemAdditionSuccess_Implementation(const FGuid& Id, UObject* InventoryItemInterface, const EItemType Type) override;

	/** Delegate function for when an item is successfully added to the inventory. Helpful for ui elements to keep track of inventory updates */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Operations") FInventoryAddtionSuccessDelegate OnInventoryItemAdditionSuccess;
	
	
//----------------------------------------------------------------------------------//
// Item Transfer																	//
//----------------------------------------------------------------------------------//
public:
	/**
	 * Sends the information to the server to transfer an item from the one inventory to another, and handles each of the different scenarios for this action. \n\n
	 * There's multiple delegate functions in response to each scenario, and if an error occurs while updating the inventory there's safeguards in place to revert the logic \n\n
	 *
	 * Order of operations is TryTransferItem ->
	 *		- TransferItemPendingClientLogic
	 *		- Server_TryTransferItem -> HandleTransferItem
	 *			- Client_TransferItemResponse
	 *				- HandleTransferItemFail
	 *				- HandleTransferItemSuccess
	 *
	 * @note For handling the ui, I'd add delegates on the response functions for update notifications on the player's inventory
	 * 
	 * @param Id										The unique id of the inventory item.
	 * @param OtherInventoryInterface					The reference to the other inventory component
	 * @param Type										The item type (used for item allocation)
	 * @returns		True if the item was successfully transferred to the other inventory.
	 * 
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * @note For handling the ui, I'd add delegates on the response functions for update notifications on the player's inventory
	 */
	virtual bool TryTransferItem_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type) override;

	
protected:
	/**
	 * What should happen assuming the item is transferred
	 * @note If the item isn't successfully transferred then @ref HandleTransferItemAdditionFail should be called, otherwise @ref HandleTransferItemAdditionSuccess is called
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * */
	virtual void TransferItemPendingClientLogic_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type) override;
	
	/** Server/Client procedure calls are not valid on interfaces, these need to be handled in the actual implementation */
	UFUNCTION(Server, Reliable) virtual void Server_TryTransferItem(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type);
	UFUNCTION(Client, Reliable) virtual void Client_TransferItemResponse(const bool bSuccess, const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type, const bool bFromThisInventory);
	
	/**
	 * The actual logic that handles transferring the item to the other inventory component
	 * @return True if it was able to transfer the item
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual bool HandleTransferItem_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type, bool& bFromThisInventory) override;
	
	/**
	 * If the item was not transferred to the other inventory 
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual void HandleTransferItemFail_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, bool bFromThisInventory) override;

	/** Delegate function for when an item failed to be added to the inventory. Helpful for ui elements to keep track of inventory updates */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Operations") FInventoryItemTransferFailureDelegate OnInventoryItemTransferFailure;
	
	/**
	 * If the item was successfully transferred to the other inventory 
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual void HandleTransferItemSuccess_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, bool bFromThisInventory) override;

	/** Delegate function for when an item is successfully added to the inventory. Helpful for ui elements to keep track of inventory updates */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Operations") FInventoryItemTransferSuccessDelegate OnInventoryItemTransferSuccess;
	
	
//----------------------------------------------------------------------------------//
// Remove Item																		//
//----------------------------------------------------------------------------------//
public:
	/**
	 * Sends the information to the server to remove an item from the inventory, and handles each of the different scenarios for this action (including whether to drop the item). \n\n
	 * There's multiple delegate functions in response to each scenario, and if an error occurs while updating the inventory there's safeguards in place to revert the logic \n\n
	 *
	 * Order of operations is TryRemoveItem ->
	 *		- RemoveItemPendingClientLogic
	 *		- Server_TryRemoveItem -> HandleRemoveItem
	 *			- Client_RemoveItemResponse
	 *				- HandleRemoveItemFail
	 *				- HandleRemoveItemSuccess
	 * 
	 * @param Id					The unique id of the inventory item.
	 * @param Type					The item type (used for item allocation)
	 * @param bDropItem				Whether the item should be spawned in the world when removed
	 * @returns		True if the item was found in the database and successfully added to the inventory.
	 * 
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * @note For handling the ui, I'd add delegates on the response functions for update notifications on the player's inventory
	 */
	virtual bool TryRemoveItem_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem) override;
	
	
protected:
	/**
	 * What should happen assuming the item is removed
	 * @note If the item isn't successfully removed then @ref HandleRemoveItemAdditionFail should be called, otherwise @ref HandleRemoveItemAdditionSuccess is called
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * */
	virtual void RemoveItemPendingClientLogic_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem) override;
	
	/** Server/Client procedure calls are not valid on interfaces, these need to be handled in the actual implementation */
	UFUNCTION(Server, Reliable) void Server_TryRemoveItem(const FGuid& Id, const EItemType Type, bool bDropItem);
	UFUNCTION(Client, Reliable) void Client_RemoveItemResponse(const bool bSuccess, const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem);
	
	/**
	 * The actual logic that handles removing the item from the inventory component
	 * @return True if it was able to remove the item
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual bool HandleRemoveItem_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, UObject*& SpawnedItem) override;
	
	/**
	 * If the item was not removed from the inventory 
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual void HandleRemoveItemFail_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem) override;

	/** Delegate function for when an item failed to be added to the inventory. Helpful for ui elements to keep track of inventory updates */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Operations") FInventoryItemRemovalFailureDelegate OnInventoryItemRemovalFailure;
	
	/**
	 * If the item was successfully removed from the inventory
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual void HandleRemoveItemSuccess_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem) override;
	
	/** Delegate function for when an item is successfully added to the inventory. Helpful for ui elements to keep track of inventory updates */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Operations") FInventoryItemRemovalSuccessDelegate OnInventoryItemRemovalSuccess;
	
	
//----------------------------------------------------------------------------------//
// Utility																			//
//----------------------------------------------------------------------------------//
public:
	/**
	 * Returns a copy of an item from the player's inventory. This function shouldn't be called directly, and should only be called on the server.
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place (Server side logic between two inventory component interfaces)
	 */
	virtual F_Item InternalGetInventoryItem_Implementation(const FGuid& Id, EItemType InventorySectionToSearch = EItemType::Inv_None) override;
		
	/**
	 * Removes an item from the player's inventory. This function shouldn't be called directly, and should only be called on the server.
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * @note these are only used for specific cases where there isn't a traditional way of editing the inventory (Server side logic between two inventory component interfaces)
	 */
	virtual void InternalRemoveInventoryItem_Implementation(const FGuid& Id, EItemType InventorySectionToSearch = EItemType::Inv_None) override;
	
	/**
	 * Adds an item from the player's inventory. This function shouldn't be called directly, and should only be called on the server.
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place (Server side logic between two inventory component interfaces)
	 */
	virtual void InternalAddInventoryItem_Implementation(const F_Item& Item) override;
	
	
protected:
	/**
	 * Returns an item from one of the lists in this component.
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual bool GetItem_Implementation(F_Item& ReturnedItem, FGuid Id, EItemType InventorySectionToSearch = EItemType::Inv_None) override;

	/** Returns the inventory list specific to the item's type */
	virtual TMap<FGuid, F_Item>& GetInventoryList(EItemType InventorySectionToSearch);
	
	/**
	 *	Returns an item from the database
	 *  @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	virtual bool GetDataBaseItem_Implementation(FName Id, F_Item& Item) override;

	/** Checks if the character is valid and if not, gets the character component and returns true */
	UFUNCTION(BlueprintCallable) virtual bool GetCharacter();
	
	/** Creates the inventory item object for adding things to the inventory. If you want to subclass the inventory object, use this function */
	virtual F_Item* CreateInventoryObject() const override;
	
	/**
	 * Spawn an item into the world
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * */
	virtual TScriptInterface<IInventoryItemInterface> SpawnWorldItem_Implementation(const F_Item& Item) override;


	/** Printing inventory information -> @ref ListInventory, ListSavedCharacterInformation  */
	UFUNCTION(BlueprintCallable) virtual void ListInventoryItem(const F_Item& Item);
	UFUNCTION(BlueprintCallable) virtual void ListInventoryMap(const TMap<FGuid, F_Item>& Map, FString ListName);
	UFUNCTION(BlueprintCallable) virtual void ListInventory();

	UFUNCTION(BlueprintCallable) virtual void ListSavedItem(const FS_Item& SavedItem);
	UFUNCTION(BlueprintCallable) virtual void ListSavedItems(const TArray<FS_Item>& List, FString ListName);
	UFUNCTION(BlueprintCallable) virtual void ListSavedWeaponInformation(const FS_WeaponInformation& SavedWeapon);
	UFUNCTION(BlueprintCallable) virtual void ListSavedWeapons(const TArray<FS_WeaponInformation>& List);
	UFUNCTION(BlueprintCallable) virtual void ListSavedCharacterInformation(const FS_CharacterInformation& Data, FString Message);

		
};

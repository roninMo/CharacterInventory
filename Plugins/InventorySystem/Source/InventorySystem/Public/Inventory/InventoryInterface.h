// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryInformation.h"
#include "UObject/Interface.h"
#include "InventoryInterface.generated.h"

class IInventoryItemInterface;


// This class does not need to be modified.
UINTERFACE(Blueprintable, BlueprintType)
class UInventoryInterface : public UInterface
{
	GENERATED_BODY()
};




/**
 * An interface for creating inventory components. The base class already handles everything, this is just for customization purposes. This allows for both blueprint and code, with blueprint taking priority
 * There's a lot of blueprint dynamic logic that allows you to have both blueprint and code in place, and I'd also recommend having delegates during the response calls for handling other component logic
 */
class INVENTORYSYSTEM_API IInventoryInterface
{
	GENERATED_BODY()
	

//----------------------------------------------------------------------------------//
// Item Addition																	//
//----------------------------------------------------------------------------------//
public:
	/**
	 * Sends the information to the server to add an item to the inventory, and handles each of the different scenarios for this action. \n\n
	 * There's multiple delegate functions in response to each scenario, and if an error occurs while updating the inventory there's safeguards in place to revert the logic \n\n
	 *
	 * Either add an item by creating a unique id and using the database id, or add an item from an already spawned item in the world
	 *
	 * Order of operations is TryAddItem:
	 *		- AddItemPendingClientLogic
	 *		- Server_TryAddItem -> HandleAddItem
	 *			- Client_AddItemResponse
	 *				- HandleItemAdditionFail
	 *				- HandleItemAdditionSuccess
	 * 
	 * @param DatabaseId								The id for this item in the inventory
	 * @param InventoryItemInterface					The reference to the actor spawned in the world, if there is one (and you want it to be deleted upon completion).
	 * @param Type										The item type (used for item allocation)
	 * @returns		True if the item was found in the database and successfully added to the inventory.
	 * 
	 * @note For handling the ui, I'd add delegates on the response functions for update notifications on the player's inventory
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory", meta = (DisplayName = "Try Add Item"))
	bool TryAddItem(const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);
	virtual bool TryAddItem_Implementation(const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);

	
protected:
	/**
	 * What should happen assuming the item is added
	 * 
	 * @note If the item isn't successfully added then @ref HandleItemAdditionFail should be called, otherwise @ref HandleItemAdditionSuccess is called
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Pending Add Item (Client Logic)"))
	void AddItemPendingClientLogic(const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);
	virtual void AddItemPendingClientLogic_Implementation(const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);
	
	/** Server/Client procedure calls are not valid on interfaces, these need to be handled in the actual implementation */
	// UFUNCTION(Server, Reliable) void Server_TryAddItem(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);
	// UFUNCTION(Client, Reliable) void Client_AddItemResponse(const bool bSuccess, const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);
	
	/**
	 * The actual logic that handles adding the item to an inventory component
	 * 
	 * @return The id of the newly created item
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Handle Add Item"))
	F_Item HandleAddItem(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);
	virtual F_Item HandleAddItem_Implementation(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);
	
	/**
	 * If the item was not added to the inventory
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Handle Add Item (Failed)"))
	void HandleItemAdditionFail(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);
	virtual void HandleItemAdditionFail_Implementation(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);

	/**
	 * If the item was successfully added to the inventory
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Handle Add Item (Succeeded)"))
	void HandleItemAdditionSuccess(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);
	virtual void HandleItemAdditionSuccess_Implementation(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type);


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
	 * @note For handling the ui, I'd add delegates on the response functions for update notifications on the player's inventory
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory", meta = (DisplayName = "Try Transfer Item"))
	bool TryTransferItem(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type);
	virtual bool TryTransferItem_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type);

	
protected:
	/**
	 * What should happen assuming the item is transferred
	 *
	 * @note If the item isn't successfully transferred then @ref HandleTransferItemAdditionFail should be called, otherwise @ref HandleTransferItemAdditionSuccess is called
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Pending Transfer Item (Client Logic)"))
	void TransferItemPendingClientLogic(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type);
	virtual void TransferItemPendingClientLogic_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type);
	
	/** Server/Client procedure calls are not valid on interfaces, these need to be handled in the actual implementation */
	// UFUNCTION(Server, Reliable) void Server_TryTransferItem(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type);
	// UFUNCTION(Client, Reliable) void Client_TransferItemResponse(const bool bSuccess, const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type);
	
	/**
	 * The actual logic that handles transferring the item to the other inventory component
	 * 
	 * @return True if it was able to transfer the item
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Handle Transfer Item"))
	bool HandleTransferItem(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type, bool& bFromThisInventory);
	virtual bool HandleTransferItem_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type, bool& bFromThisInventory);
	
	/**
	 * If the item was not transferred to the other inventory
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Handle Transfer Item (Failed"))
	void HandleTransferItemFail(const FGuid& Id, UObject* OtherInventoryInterface, bool bFromThisInventory);
	virtual void HandleTransferItemFail_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, bool bFromThisInventory);

	/**
	 * If the item was successfully transferred to the other inventory
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Handle Transfer Item (Succeeded)"))
	void HandleTransferItemSuccess(const FGuid& Id, UObject* OtherInventoryInterface, bool bFromThisInventory);
	virtual void HandleTransferItemSuccess_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, bool bFromThisInventory);


public:
	/**
	 * Utility function for handling updating the inventory information on other clients during an item transfer.
	 * This should handle the appropriate remote procedure logic so that the other inventory's client information is updated
	 * 
	 * @param Id										The unique id of the inventory item.
	 * @param DatabaseId								The id for this item in the inventory
	 * @param Type										The item type (used for item allocation)
	 * @param bAddItem									Whether the item is being added or removed from the inventory
	 * 
	 * @remarks Client logic doesn't have any problems when invoking the handle logic on the client response functions, it's just problematic when there's multiple Clients (During a transfer)
	 */
	UFUNCTION() virtual void HandleTransferItemForOtherInventoryClientLogic(const FGuid& Id, const FName DatabaseId, const EItemType Type, const bool bAddItem);

	
	
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
	 * @note For handling the ui, I'd add delegates on the response functions for update notifications on the player's inventory
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory", meta = (DisplayName = "Try Remove Item"))
	bool TryRemoveItem(const FGuid& Id, const EItemType Type, bool bDropItem);
	virtual bool TryRemoveItem_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem);
	
	
protected:
	/**
	 * What should happen assuming the item is removed
	 *
	 * @note If the item isn't successfully removed then @ref HandleRemoveItemAdditionFail should be called, otherwise @ref HandleRemoveItemAdditionSuccess is called
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Pending Remove Item (Client Logic)"))
	void RemoveItemPendingClientLogic(const FGuid& Id, const EItemType Type, bool bDropItem);
	virtual void RemoveItemPendingClientLogic_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem);
	
	/** Server/Client procedure calls are not valid on interfaces, these need to be handled in the actual implementation */
	// UFUNCTION(Server, Reliable) void Server_TryRemoveItem(const FGuid& Id, const EItemType Type, bool bDropItem);
	// UFUNCTION(Client, Reliable) void Client_RemoveItemResponse(const bool bSuccess, const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem);
	
	/**
	 * The actual logic that handles removing the item from the inventory component
	 * 
	 * @return True if it was able to remove the item
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Handle Remove Item"))
	bool HandleRemoveItem(const FGuid& Id, const EItemType Type, bool bDropItem, UObject*& SpawnedItem);
	virtual bool HandleRemoveItem_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, UObject*& SpawnedItem);
	
	/**
	 * If the item was not removed from the inventory
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Handle Remove Item (Failed)"))
	void HandleRemoveItemFail(const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem);
	virtual void HandleRemoveItemFail_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem);

	/**
	 * If the item was successfully removed from the inventory
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Operations", meta = (DisplayName = "Handle Remove Item (Succeeded)"))
	void HandleRemoveItemSuccess(const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem);
	virtual void HandleRemoveItemSuccess_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem);
	
	
//----------------------------------------------------------------------------------//
// Utility																			//
//----------------------------------------------------------------------------------//
public:
	/**
	 * Returns a copy of an item from the player's inventory. This function shouldn't be called directly, and should only be called on the server.
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place (Server side logic between two inventory component interfaces)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	F_Item InternalGetInventoryItem(const FGuid& Id, EItemType InventorySectionToSearch = EItemType::Inv_None);
	virtual F_Item InternalGetInventoryItem_Implementation(const FGuid& Id, EItemType InventorySectionToSearch = EItemType::Inv_None);
	
	/**
	 * Adds an item from the player's inventory. This function shouldn't be called directly, and should only be called on the server.
	 *
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place (Server side logic between two inventory component interfaces)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void InternalAddInventoryItem(const F_Item& Item);
	virtual void InternalAddInventoryItem_Implementation(const F_Item& Item);
		
	/**
	 * Removes an item from the player's inventory. This function shouldn't be called directly, and should only be called on the server.
	 * 
	 * @remark Blueprints do not need to handle this logic unless they want to override the logic already in place
	 * 
	 * @remark these are only used for specific cases where there isn't a traditional way of editing the inventory (Server side logic between two inventory component interfaces)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void InternalRemoveInventoryItem(const FGuid& Id, EItemType InventorySectionToSearch = EItemType::Inv_None);
	virtual void InternalRemoveInventoryItem_Implementation(const FGuid& Id, EItemType InventorySectionToSearch = EItemType::Inv_None);
	
	
public:
	/**
	 * Returns an item from one of the lists in this component.
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory", meta = (DisplayName = "Get Item from Inventory"))
	bool GetItem(UPARAM(ref) F_Item& ReturnedItem, FGuid Id, EItemType InventorySectionToSearch = EItemType::Inv_None);
	virtual bool GetItem_Implementation(F_Item& ReturnedItem, FGuid Id, EItemType InventorySectionToSearch = EItemType::Inv_None);
	
	/** Returns the character's id (a combination of both the NetId and the PlatformId) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Utilities")
	FString GetPlayerId() const;
	virtual FString GetPlayerId_Implementation() const;
	
	
protected:
	/**
	 *	Returns an item from the database
	 *	
	 *  @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Utilities", meta = (DisplayName = "Get Database Item"))
	bool GetDataBaseItem(FName Id, F_Item& Item);
	virtual bool GetDataBaseItem_Implementation(FName Id, F_Item& Item);
	
	/**
	 * Creates the inventory item object for adding things to the inventory.
	 * If you want to subclass the inventory object, use this function
	 * 
	 * @remarks If you want to subclass the Item object, use this function. And if you create any Items, do it with this function
	 */
	virtual F_Item* CreateInventoryObject() const;
	
	/**
	 * Spawns an inventory item in the world
	 * 
	 * @remarks Blueprints do not need to handle this logic unless they want to override the logic already in place
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Utilities", meta = (DisplayName = "Spawn Item"))
	TScriptInterface<IInventoryItemInterface> SpawnWorldItem(const F_Item& Item, const FTransform& Location);
	virtual TScriptInterface<IInventoryItemInterface> SpawnWorldItem_Implementation(const F_Item& Item, const FTransform& Location);

	
};

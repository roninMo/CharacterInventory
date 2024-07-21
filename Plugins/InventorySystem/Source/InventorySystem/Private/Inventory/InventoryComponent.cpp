// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/InventoryComponent.h"

#include "GameFramework/Character.h"
#include "Inventory/InventoryInterface.h"
#include "Item/InventoryItemInterface.h"
#include "Item/ItemBase.h"
#include "Engine/PackageMapClient.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(InventoryLog);


UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}


void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// Save the net and platform id for determining the character (on both server and client)
	SetPlayerId();
}


void UInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateInventoryAfterRetrievingSaveInformation();
}


#pragma region Inventory retrieval logic
#pragma region Add Item
bool UInventoryComponent::TryAddItem_Implementation(const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type)
{
	if (!GetCharacter() || DatabaseId.IsNone()) return false;

	// TODO: check if this is valid in production. If not, add logic to refactor distributing this from the server - Unable to resolve default guid from client: ObjectName: BaseItem_0, ObjOuter: /Game/UEDPIE_0_Level.Level:PersistentLevel 
	FGuid Id = FGuid::NewGuid();
	const TScriptInterface<IInventoryItemInterface> InventoryItem = InventoryItemInterface;
	if (InventoryItem.GetInterface() && InventoryItem->Execute_GetId(InventoryItem.GetObject()).IsValid())
	{
		Id = InventoryItem->Execute_GetId(InventoryItem.GetObject());
	}

	// If the server calls the function, just handle it and send the updated information to the client. Otherwise handle sending the information to the server and then back to the client
	if (Character->IsLocallyControlled())
	{
		Server_TryAddItem(Id, DatabaseId, InventoryItemInterface, Type);
		Execute_AddItemPendingClientLogic(this, DatabaseId, InventoryItemInterface, Type);
		return true; // Just return true by default and let the client rpc response handle everything else
	}
	else if (Character->HasAuthority())
	{
		const F_Item ItemId = Execute_HandleAddItem(this, Id, DatabaseId, InventoryItemInterface, Type);
		Client_AddItemResponse(ItemId.IsValid() ? true : false, Id, DatabaseId, InventoryItemInterface, Type);
		return true;
	}

	return false;
}


void UInventoryComponent::Server_TryAddItem_Implementation(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type)
{
	bool bSuccessfullyAddedItem;
	const TScriptInterface<IInventoryItemInterface> InventoryItem = InventoryItemInterface;
	
	// Adding an item from the world
	if (InventoryItem.GetInterface())
	{
		// Don't allow players to interfere with items that are already being adjusted 
		if (!InventoryItem->Execute_IsSafeToAdjustItem(InventoryItem.GetObject()))
		{
			bSuccessfullyAddedItem = false;
		}
		else
		{
			const F_Item ItemId = Execute_HandleAddItem(this, Id, DatabaseId, InventoryItemInterface, Type);
			InventoryItem->Execute_SetPlayerPending(InventoryItem.GetObject(), Character);
			bSuccessfullyAddedItem = ItemId.IsValid();
		}
		
		// Remove the scope lock
		if (InventoryItem->Execute_GetPlayerPending(InventoryItem.GetObject()) == Character) InventoryItem->Execute_SetPlayerPending(InventoryItem.GetObject(), nullptr);
	}
	else
	// Adding an item by id
	{
		const F_Item ItemId = Execute_HandleAddItem(this, Id, DatabaseId, InventoryItemInterface, Type);
		bSuccessfullyAddedItem = ItemId.IsValid();
	}

	if (bDebugInventory_Server)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() added item {2}: {3} + {4}({5})", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), *Execute_GetPlayerId(this), bSuccessfullyAddedItem ? "succeeded" : "failed", DatabaseId,  *Id.ToString()
		);
	}
	
	Client_AddItemResponse(bSuccessfullyAddedItem, Id, DatabaseId, InventoryItemInterface, Type);
}


F_Item UInventoryComponent::HandleAddItem_Implementation(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type)
{
	const TScriptInterface<IInventoryItemInterface> InventoryInterface = InventoryItemInterface;
	
	F_Item Item = *CreateInventoryObject();
	if (InventoryInterface.GetInterface()) Item = InventoryInterface->Execute_GetItem(InventoryInterface.GetObject());
	if (!Item.IsValid())
	{
		Execute_GetDataBaseItem(this, DatabaseId, Item);
		Item.Id = Id;
	}

	if (bDebugInventory_Server || bDebugInventory_Client)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() InventoryAddition: {2} + {3}({4}) ",
			*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), *Execute_GetPlayerId(this), DatabaseId, *Id.ToString()
		);
	}
	
	if (Item.IsValid())
	{
		Execute_InternalAddInventoryItem(this, Item);
		return Item;
	}

	return FGuid();
}


void UInventoryComponent::Client_AddItemResponse_Implementation(const bool bSuccess, const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type)
{
	const TScriptInterface<IInventoryItemInterface> InventoryInterface = InventoryItemInterface;
	
	if (!bSuccess)
	{
		Execute_HandleItemAdditionFail(this, Id, DatabaseId, InventoryItemInterface, Type);
		OnInventoryItemAdditionFailure.Broadcast(Id, DatabaseId, InventoryInterface);
	}
	else
	{
		F_Item Item = F_Item();
		if (ROLE_AutonomousProxy == GetOwner()->GetLocalRole()) // TODO: are extra checks on clients necessary?
		{
			Item = Execute_HandleAddItem(this, Id, DatabaseId, InventoryItemInterface, Type);
		}
		else if (ROLE_Authority == GetOwner()->GetLocalRole())
		{
			Execute_GetItem(this, Item, Id, Type);
		}
		
		Execute_HandleItemAdditionSuccess(this, Item.Id, Item.ItemName, InventoryItemInterface, Type);
		OnInventoryItemAdditionSuccess.Broadcast(Item, InventoryInterface);
	}
	
	if (bDebugInventory_Client)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() AddItemResponse: {2}, {3} add item operation ->  {4}({5}) ",
			*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), bSuccess ? "succeeded" : "failed", *Execute_GetPlayerId(this), DatabaseId, *Id.ToString()
		);
	}
}


void UInventoryComponent::AddItemPendingClientLogic_Implementation(const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type)
{
	// Hide the item in the world but do not delete it until the process is complete and update the ui
	AActor* WorldItem = Cast<AActor>(InventoryItemInterface);
	if (WorldItem) WorldItem->SetActorHiddenInGame(true);
	
	// UpdateWidgetDisplay(Data);
}


void UInventoryComponent::HandleItemAdditionFail_Implementation(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type)
{
	// Un hide the item and add any other necessary logic and undo the ui
	AActor* WorldItem = Cast<AActor>(InventoryItemInterface);
	if (WorldItem) WorldItem->SetActorHiddenInGame(false);
	
	// UpdateWidgetDisplay(Data, true, true);
}


void UInventoryComponent::HandleItemAdditionSuccess_Implementation(const FGuid& Id, const FName DatabaseId, UObject* InventoryItemInterface, const EItemType Type)
{
	// delete the item
	AActor* WorldItem = Cast<AActor>(InventoryItemInterface);
	if (WorldItem) WorldItem->Destroy();
}
#pragma endregion 




#pragma region Transfer Item
bool UInventoryComponent::TryTransferItem_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type)
{
	if (!GetCharacter() || !Id.IsValid() || !OtherInventoryInterface) return false;

	// If the server calls the function, just handle it and send the updated information to the client. Otherwise handle sending the information to the server and then back to the client
	if (Character->IsLocallyControlled())
	{
		Server_TryTransferItem(Id, OtherInventoryInterface, Type);
		Execute_TransferItemPendingClientLogic(this, Id, OtherInventoryInterface, Type);
		return true; // Just return true by default and let the client rpc response handle everything else
	}
	else if (Character->HasAuthority())
	{
		bool bFromThisInventory; 
		const bool bSuccessfullyTransferredItem = Execute_HandleTransferItem(this, Id, OtherInventoryInterface, Type, bFromThisInventory);

		// The client's have trouble accessing other client's inventories (We're just recreating the item with the ItemId)
		const FName ItemId = GetItemId(Id, Type, OtherInventoryInterface);
		Client_TransferItemResponse(bSuccessfullyTransferredItem, Id, ItemId, OtherInventoryInterface, Type, bFromThisInventory);
		return true;
	}

	return false;
}


void UInventoryComponent::Server_TryTransferItem_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type)
{
	bool bFromThisInventory;
	const bool bSuccessfullyTransferredItem = Execute_HandleTransferItem(this, Id, OtherInventoryInterface, Type, bFromThisInventory);
	FName ItemId = GetItemId(Id, Type, OtherInventoryInterface);

	if (bDebugInventory_Server)
	{
		const TScriptInterface<IInventoryInterface> InventoryInterface = OtherInventoryInterface;
		FString OtherInventoryId = *GetNameSafe(OtherInventoryInterface);
		if (InventoryInterface.GetInterface()) OtherInventoryId = InventoryInterface->Execute_GetPlayerId(InventoryInterface.GetObject());
		
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() {2} transferred an item to {3}, ({4}) -> {5}({6})", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__),
			bFromThisInventory ? *Execute_GetPlayerId(this) : *OtherInventoryId,
			bFromThisInventory ? *OtherInventoryId : *Execute_GetPlayerId(this),
			bSuccessfullyTransferredItem ? "succeeded" : "failed",
			ItemId, *Id.ToString()
		);
	}
	
	// The client's have trouble accessing other client's inventories (We're just recreating the item with the ItemId)
	Client_TransferItemResponse(bSuccessfullyTransferredItem, Id, ItemId, OtherInventoryInterface, Type, bFromThisInventory);
}


bool UInventoryComponent::HandleTransferItem_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type, bool& bFromThisInventory)
{
	// Find the item, and then transfer it to the other inventory
	const TScriptInterface<IInventoryInterface> OtherInventory = OtherInventoryInterface;
	if (!Id.IsValid() || !OtherInventory.GetInterface()) return false;
	F_Item Item = *CreateInventoryObject();

	// Search for the item in the player's inventory
	Execute_GetItem(this, Item, Id, Type);
	if (Item.IsValid()) bFromThisInventory = true;
	else
	{
		OtherInventory->Execute_GetItem(OtherInventory.GetObject(), Item, Id, Type);
		bFromThisInventory = false;
	}

	if (!Item.IsValid())
	{
		if (bDebugInventory_Server || bDebugInventory_Client)
		{
			UE_LOGFMT(InventoryLog, Error, "({0}) {1}() (invalid/not found) id: {2} searched in {3} and {4}'s inventory", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
				*FString(__FUNCTION__), *Id.ToString(), *Execute_GetPlayerId(this), *OtherInventory->Execute_GetPlayerId(OtherInventory.GetObject())
			);
		}
		return false;
	}

	// Transfer the item
	if (bFromThisInventory)
	{
		Execute_InternalRemoveInventoryItem(this, Item.Id, Item.ItemType);
		OtherInventory->Execute_InternalAddInventoryItem(OtherInventory.GetObject(), Item);

		// Client logic
		OtherInventory->HandleTransferItemForOtherInventoryClientLogic(Item.Id, Item.ItemName, Item.ItemType, true);
	}
	else
	{
		Execute_InternalAddInventoryItem(this, Item);
		OtherInventory->Execute_InternalRemoveInventoryItem(OtherInventory.GetObject(), Item.Id, Item.ItemType);
		
		// Client logic
		OtherInventory->HandleTransferItemForOtherInventoryClientLogic(Item.Id, Item.ItemName, Item.ItemType, false);
	}

	if (bDebugInventory_Server || bDebugInventory_Client)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() InventoryTransfer: {2}({3}) from {4} to {5}'s inventory", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), Item.ItemName, *Id.ToString(),
			bFromThisInventory ? *Execute_GetPlayerId(this) : *OtherInventory->Execute_GetPlayerId(OtherInventory.GetObject()),
			bFromThisInventory ? *OtherInventory->Execute_GetPlayerId(OtherInventory.GetObject()) : *Execute_GetPlayerId(this)
		);
	}
	return true;
}


void UInventoryComponent::HandleTransferItemForOtherInventoryClientLogic(const FGuid& Id, const FName DatabaseId, const EItemType Type, const bool bAddItem) { Client_HandleTransferItemForOtherInventory(Id, DatabaseId, Type, bAddItem); }
void UInventoryComponent::Client_HandleTransferItemForOtherInventory_Implementation(const FGuid& Id, const FName DatabaseId, const EItemType Type, const bool bAddItem)
{
	if (bAddItem)
	{
		F_Item Item = *CreateInventoryObject();
		Execute_GetDataBaseItem(this, DatabaseId, Item);
		Item.Id = Id;
		Execute_InternalAddInventoryItem(this, Item);
	}
	else
	{
		Execute_InternalRemoveInventoryItem(this, Id, Type);
	}
}


void UInventoryComponent::Client_TransferItemResponse_Implementation(const bool bSuccess, const FGuid& Id, const FName DatabaseId, UObject* OtherInventoryInterface, const EItemType Type, const bool bFromThisInventory)
{
	const TScriptInterface<IInventoryInterface> OtherInventory = OtherInventoryInterface;
	if (!bSuccess)
	{
		Execute_HandleTransferItemFail(this, Id, OtherInventoryInterface, bFromThisInventory);
		OnInventoryItemTransferFailure.Broadcast(Id, OtherInventory, bFromThisInventory);
	}
	else
	{
		// Handle removing or adding the item to the inventory on this character if it was transferred to them
		if (ROLE_AutonomousProxy == GetOwner()->GetLocalRole()) // TODO: are extra checks on clients necessary?
		{
			// Add the item on the client
			if (!bFromThisInventory)
			{
				// Execute_HandleTransferItem(this, Id, OtherInventoryInterface, Type, bWasFromThisInventory);
				F_Item Item = *CreateInventoryObject();
				Execute_GetDataBaseItem(this, DatabaseId, Item);
				Item.Id = Id;
				
				if (Item.IsValid()) Execute_InternalAddInventoryItem(this, Item);
				else if (bDebugInventory_Client || bDebugInventory_Server)
				{
					UE_LOGFMT(InventoryLog, Error, "({0}) {1}() invalid/unable to create item: {2}({3}) for {4}'s inventory",
						*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), DatabaseId, *Id.ToString(), *Execute_GetPlayerId(this)
					);
				}
			}
			else
			{
				Execute_InternalRemoveInventoryItem(this, Id, Type);
			}
		}
		
		Execute_HandleTransferItemSuccess(this, Id, OtherInventoryInterface, bFromThisInventory);
		OnInventoryItemTransferSuccess.Broadcast(Id, OtherInventory, bFromThisInventory);
	}
	
	if (bDebugInventory_Client)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() TransferItemResponse: {2}, {3} transfer item operation to {4} ->  {5}({6})", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), bSuccess ? "succeeded" : "failed",
			bFromThisInventory ? *Execute_GetPlayerId(this) : *OtherInventory->Execute_GetPlayerId(OtherInventory.GetObject()),
			bFromThisInventory ? *OtherInventory->Execute_GetPlayerId(OtherInventory.GetObject()) : *Execute_GetPlayerId(this),
			DatabaseId, *Id.ToString()
		);
	}
}


void UInventoryComponent::TransferItemPendingClientLogic_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type)
{
	// Update the inventory widgets for both inventory components to show the item
	// UpdateWidgetDisplay(Data);
}

void UInventoryComponent::HandleTransferItemFail_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, bool bFromThisInventory)
{
	// Undo the updates to the inventory widgets
	// UpdateWidgetDisplay(Data);
}

void UInventoryComponent::HandleTransferItemSuccess_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, bool bFromThisInventory)
{
}
#pragma endregion 




#pragma region Remove Item
bool UInventoryComponent::TryRemoveItem_Implementation(const FGuid& Id, const EItemType Type, const bool bDropItem)
{
	if (!GetCharacter() || !Id.IsValid()) return false;

	// If the server calls the function, just handle it and send the updated information to the client. Otherwise handle sending the information to the server and then back to the client
	if (Character->IsLocallyControlled())
	{
		Server_TryRemoveItem(Id, Type, bDropItem);
		Execute_RemoveItemPendingClientLogic(this, Id, Type, bDropItem);
		return true; // Just return true by default and let the client rpc response handle everything else
	}
	else if (Character->HasAuthority())
	{
		UObject* SpawnedItem;
		FName ItemId = GetItemId(Id, Type);
		const bool bSuccessfullyRemovedItem = Execute_HandleRemoveItem(this, Id, Type, bDropItem, SpawnedItem);
		Client_RemoveItemResponse(bSuccessfullyRemovedItem, Id, ItemId, Type, bDropItem, SpawnedItem);
		return true;
	}

	return false;
}


void UInventoryComponent::Server_TryRemoveItem_Implementation(const FGuid& Id, const EItemType Type, const bool bDropItem)
{
	UObject* SpawnedItem;
	FName ItemId = GetItemId(Id, Type);
	const bool bSuccessfullyRemovedItem = Execute_HandleRemoveItem(this, Id, Type, bDropItem, SpawnedItem);
	
	if (bDebugInventory_Server)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() removed item {2}: {3} - {4}({5})",
			*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__),
			*Execute_GetPlayerId(this),
			bSuccessfullyRemovedItem ? "succeeded" : "failed",
			ItemId, *Id.ToString()
		);
	}
	
	Client_RemoveItemResponse(bSuccessfullyRemovedItem, Id, ItemId, Type, bDropItem, SpawnedItem);
}


bool UInventoryComponent::HandleRemoveItem_Implementation(const FGuid& Id, const EItemType Type, const bool bDropItem, UObject*& SpawnedItem)
{
	if (bDropItem)
	{
		F_Item Item = *CreateInventoryObject();
		Execute_GetItem(this, Item, Id, Type);
		
		if (!Item.IsValid())
		{
			if (bDebugInventory_Server || bDebugInventory_Client)
			{
				UE_LOGFMT(InventoryLog, Error, "({0}) {1}() (invalid/not found) item in {2}'s inventory, id: {3}",
					*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), *Execute_GetPlayerId(this), *Id.ToString()
				);
			}
			return false;
		}

		const TScriptInterface<IInventoryItemInterface> InventoryItem = Execute_SpawnWorldItem(this, Item, GetOwner()->GetActorTransform());
		SpawnedItem = InventoryItem ? InventoryItem.GetObject() : nullptr;
	}

	if (bDebugInventory_Server || bDebugInventory_Client)
	{
		FName ItemId = GetItemId(Id, Type);
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() InventoryRemoval: {2} - {3}({4}) ",
			*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__),
			*Execute_GetPlayerId(this),
			ItemId, *Id.ToString()
		);
	}
	
	Execute_InternalRemoveInventoryItem(this, Id, Type);
	return true;
}


void UInventoryComponent::Client_RemoveItemResponse_Implementation(const bool bSuccess, const FGuid& Id, const FName DatabaseId, const EItemType Type, const bool bDropItem, UObject* SpawnedItem)
{
	const TScriptInterface<IInventoryItemInterface> SpawnedItemInterface = SpawnedItem;
	
	if (!bSuccess)
	{
		Execute_HandleRemoveItemFail(this, Id, Type, bDropItem, SpawnedItem);
		OnInventoryItemRemovalFailure.Broadcast(Id, SpawnedItemInterface);
	}
	else
	{
		if (ROLE_AutonomousProxy == GetOwner()->GetLocalRole()) // TODO: are extra checks on clients necessary?
		{
			Execute_HandleRemoveItem(this, Id, Type, false, SpawnedItem);
		}
		
		Execute_HandleRemoveItemSuccess(this, Id, Type, bDropItem, SpawnedItem);
		OnInventoryItemRemovalSuccess.Broadcast(Id, SpawnedItemInterface);
	}
	
	if (bDebugInventory_Client)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() RemoveItemResponse: {2}, {3} remove item operation ->  {4}({5}) {6}", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), bSuccess ? "succeeded" : "failed",
			*Execute_GetPlayerId(this),
			DatabaseId, *Id.ToString(),
			bDropItem ? "(dropped)" : ""
		);
	}
}


void UInventoryComponent::RemoveItemPendingClientLogic_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem)
{
	// Update the inventory widgets for both inventory components to hide the item
	// UpdateWidgetDisplay(Data);
}

void UInventoryComponent::HandleRemoveItemFail_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem)
{
	// Undo the updates to the inventory widgets
	// UpdateWidgetDisplay(Data);
}

void UInventoryComponent::HandleRemoveItemSuccess_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, UObject* SpawnedItem)
{
}
#pragma endregion
#pragma endregion




#pragma region Saving and Loading
F_InventorySaveInformation UInventoryComponent::SaveInventoryInformation()
{
	TArray<FS_Item> InventoryItems;

	if (!QuestItems.IsEmpty()) for (auto &[Id, Item] : QuestItems) InventoryItems.Add(CreateSavedItem(Item));
	if (!CommonItems.IsEmpty()) for (auto &[Id, Item] : CommonItems) InventoryItems.Add(CreateSavedItem(Item));
	if (!Weapons.IsEmpty()) for (auto &[Id, Item] : Weapons) InventoryItems.Add(CreateSavedItem(Item));
	if (!Armors.IsEmpty()) for (auto &[Id, Item] : Armors) InventoryItems.Add(CreateSavedItem(Item));
	if (!Materials.IsEmpty()) for (auto &[Id, Item] : Materials) InventoryItems.Add(CreateSavedItem(Item));
	if (!Notes.IsEmpty()) for (auto &[Id, Item] : Notes) InventoryItems.Add(CreateSavedItem(Item));
	
	F_InventorySaveInformation SaveInformation;
	SaveInformation.InventoryItems = InventoryItems;

	return SaveInformation;

	/*
		UPlayerSaveInformation* SaveInformation = NewObject<UPlayerSaveInformation>();
		if (SaveInformation)
		{
			SaveInformation->CharacterInformation = SaveData;
			UGameplayStatics::SaveGameToSlot(SaveInformation, SaveData.PlatformId, SaveData.NetId);
			OnSaveData.Broadcast();
		}

		FString SlotName = SaveData.PlatformId; // + SaveData.NetId;
		UPlayerSaveInformation* SaveInformation = Cast<UPlayerSaveInformation>(UGameplayStatics::LoadGameFromSlot(SaveData.PlatformId, SaveData.NetId));
		if (SaveInformation) SaveData = SaveInformation->CharacterInformation;
	*/
}


void UInventoryComponent::LoadInventoryInformation(const F_InventorySaveInformation& SaveInformation)
{
	if (!GetCharacter() || !Character->HasAuthority()) return;
	if (bDebugSaveInformation)
	{
		UE_LOGFMT(InventoryLog, Warning, "{0} {1}() Loading the [{2}][{3}]'s inventory from saved information.",
			*UEnum::GetValueAsString(Character->GetLocalRole()), *FString(__FUNCTION__), NetId, PlatformId
		);
		ListSavedInventory(SaveInformation);
	}
	
	// Load inventory items @note We might get errors if we send over everything altogether, so this is divided into multiple functions
	TArray<FS_Item> InventoryItems;
	for (int i = 0; i < SaveInformation.InventoryItems.Num(); i++)
	{
		InventoryItems.Add(SaveInformation.InventoryItems[i]);
		if (InventoryItems.Num() >= 45)
		{
			Client_LoadSomeInventoryData(InventoryItems);
			InventoryItems.Empty();
		}
	}
	if (!InventoryItems.IsEmpty())
	{
		Client_LoadSomeInventoryData(InventoryItems);
	}
	
	Client_LoadSaveDataCompleted();
}


void UInventoryComponent::Client_BeginLoadingInventoryData_Implementation()
{
	ClientInventorySaveData.NetId = NetId;
	ClientInventorySaveData.PlatformId = PlatformId;

	SaveState = ESaveState::ESave_Pending;
	ClientInventorySaveData.InventoryItems.Empty();
}
void UInventoryComponent::Client_LoadSomeInventoryData_Implementation(const TArray<FS_Item>& Items)
{
	// Retrieve some of the items that were sent from the server
	for (FS_Item Item : Items)
	{
		ClientInventorySaveData.InventoryItems.Add(Item);
	}
}
void UInventoryComponent::Client_LoadSaveDataCompleted_Implementation()
{
	// Update the CurrentInventorySaveData value in preparation for when the inventory is updated
	CurrentInventorySaveData = ClientInventorySaveData;
	SaveState = ESaveState::ESave_SaveReady;

	// Clear out the pending values
	ClientInventorySaveData.NetId = NetId;
	ClientInventorySaveData.PlatformId = PlatformId;
	ClientInventorySaveData.InventoryItems.Empty();
}


bool UInventoryComponent::UpdateInventoryInformation(const F_InventorySaveInformation& SaveInformation)
{
	bool bSuccessfullySavedInventory = true;
	if (SaveInformation.InventoryItems.IsEmpty()) return false;

	if (bDebugSaveInformation)
	{
		UE_LOGFMT(InventoryLog, Warning, "{0} {1}() Loading the [{2}][{3}]'s inventory from saved information.",
			*UEnum::GetValueAsString(Character->GetLocalRole()), *FString(__FUNCTION__), NetId, PlatformId
		);
		ListSavedInventory(SaveInformation);
	}
	
	for (const FS_Item SavedItem : SaveInformation.InventoryItems)
	{
		F_Item Item;
		Execute_GetDataBaseItem(this, SavedItem.ItemName, Item);

		if (Item.IsValid())
		{
			TMap<FGuid, F_Item>& InventoryList = GetInventoryList(Item.ItemType);
			InventoryList.Add(Item.GetId(), Item);
		}
		else bSuccessfullySavedInventory = false;
	}

	if (bDebugSaveInformation)
	{
		UE_LOGFMT(InventoryLog, Warning, "{0} {1}() Loaded [{2}][{3}]'s inventory information.",
			*UEnum::GetValueAsString(Character->GetLocalRole()), *FString(__FUNCTION__), NetId, PlatformId
		);
		ListInventory();
	}

	// Let the client know the save information has been completed
	OnLoadSaveData.Broadcast(bSuccessfullySavedInventory);
	return bSuccessfullySavedInventory;
}


void UInventoryComponent::UpdateInventoryAfterRetrievingSaveInformation()
{
	// if (bCharacterReady && PlayerState && PlayerState->GetSaveState() == ESaveState::ESave_SaveReady) SaveInformation();
	
	if (SaveState == ESaveState::ESave_SaveReady)
	{
		SaveState = ESaveState::ESave_Saved;
		UpdateInventoryInformation(CurrentInventorySaveData);
	}
}


FS_Item UInventoryComponent::CreateSavedItem(const F_Item& Item) const
{
	if (!Item.IsValid()) return FS_Item();
	return FS_Item(Item.Id, Item.ItemName, Item.SortOrder);
}
#pragma endregion 



#pragma region Utility
F_Item UInventoryComponent::InternalGetInventoryItem_Implementation(const FGuid& Id, EItemType InventorySectionToSearch)
{
	F_Item Item = *CreateInventoryObject();

	// Search for the item in the inventory, and return it
	if (EItemType::Inv_None != InventorySectionToSearch)
	{
		TMap<FGuid, F_Item>& InventoryList = GetInventoryList(InventorySectionToSearch);
		if (InventoryList.Contains(Id))
		{
			Item = InventoryList[Id];
			return Item;
		}
	}

	// if nothing is specified then search through everything
	for (int i = 0; i < static_cast<int>(EItemType::Inv_MAX); i++)
	{
		const EItemType ItemType = static_cast<EItemType>(i);
		TMap<FGuid, F_Item>& InventoryList = GetInventoryList(ItemType);
		if (InventoryList.Contains(Id))
		{
			Item = InventoryList[Id];
			return Item;
		}
	}
	
	return Item;
}


void UInventoryComponent::InternalRemoveInventoryItem_Implementation(const FGuid& Id, const EItemType InventorySectionToSearch)
{
	TMap<FGuid, F_Item>& InventoryList = GetInventoryList(InventorySectionToSearch);
	if (InventoryList.Contains(Id))
	{
		InventoryList.Remove(Id);
	}
	// else
	// {
	// 	for (int i = 0; i < static_cast<int>(EItemType::Inv_MAX); i++)
	// 	{
	// 		const EItemType ItemType = static_cast<EItemType>(i);
	// 		InventoryList = GetInventoryList(ItemType);
	// 		if (InventoryList.Contains(Id))
	// 		{
	// 			InventoryList.Remove(Id);
	// 			return;
	// 		}
	// 	}
	// }
}


void UInventoryComponent::InternalAddInventoryItem_Implementation(const F_Item& Item)
{
	TMap<FGuid, F_Item>& InventoryList = GetInventoryList(Item.ItemType);
	InventoryList.Add(Item.Id, Item);
}


TMap<FGuid, F_Item>& UInventoryComponent::GetInventoryList(EItemType InventorySectionToSearch)
{
	if (EItemType::Inv_QuestItem == InventorySectionToSearch) return QuestItems;
	if (EItemType::Inv_Item == InventorySectionToSearch) return CommonItems;
	if (EItemType::Inv_Weapon == InventorySectionToSearch) return Weapons;
	if (EItemType::Inv_Armor == InventorySectionToSearch) return Armors;
	if (EItemType::Inv_Material == InventorySectionToSearch) return Materials;
	if (EItemType::Inv_Note == InventorySectionToSearch) return Notes;
	
	if (EItemType::Inv_Custom == InventorySectionToSearch)
	{
		// TODO: Custom item logic here (map enums to int values)
	}

	return CommonItems;
}



bool UInventoryComponent::GetItem_Implementation(F_Item& ReturnedItem, FGuid Id, EItemType InventorySectionToSearch)
{
	if (!Id.IsValid()) return false;

	// search for the item in the inventory
	ReturnedItem = Execute_InternalGetInventoryItem(this, Id, InventorySectionToSearch);
	if (ReturnedItem.IsValid()) return true;
	return false;
}


bool UInventoryComponent::GetDataBaseItem_Implementation(const FName Id, F_Item& Item)
{
	if (!ItemDatabase || Id.IsNone()) return false;
	if (const FInventory_ItemDatabase* ItemData = ItemDatabase->FindRow<FInventory_ItemDatabase>(Id, TEXT("Inventory Item Data Context")))
	{
		Item = ItemData->ItemInformation;
		Item.Id = FGuid::NewGuid();
	}

	return false;
}


F_Item* UInventoryComponent::CreateInventoryObject() const
{
	return new F_Item();
}

ESaveState UInventoryComponent::GetSaveState()
{
	return SaveState;
}

void UInventoryComponent::SetSaveState(const ESaveState State)
{
	SaveState = State;
}


bool UInventoryComponent::GetCharacter()
{
	if (Character) return true;

	Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		UE_LOGFMT(InventoryLog, Error, "{0} failed to get owning character for inventory component operations!", *GetName());
		return false;
	}

	return true;
}


TScriptInterface<IInventoryItemInterface> UInventoryComponent::SpawnWorldItem_Implementation(const F_Item& Item, const FTransform& Location)
{
	if (GetWorld() && Item.WorldClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.Owner = GetOwner();
		FTransform SpawnTransform = Location;
		FVector SpawnLocation = SpawnTransform.GetLocation();
		SpawnLocation.Z = SpawnLocation.Z + 34.0f;
		SpawnTransform.SetLocation(SpawnLocation);
	
		if (AItemBase* SpawnedItem = GetWorld()->SpawnActor<AItemBase>(Item.WorldClass, SpawnTransform, SpawnParameters))
		{
			const TScriptInterface<IInventoryItemInterface> WorldItem = SpawnedItem;
			SpawnedItem->Execute_SetItemInformationDatabase(SpawnedItem, ItemDatabase);
			SpawnedItem->Execute_SetItem(SpawnedItem, Item);
			SpawnedItem->Execute_SetId(SpawnedItem, Item.Id); // Persist the unique id of this inventory object
			return WorldItem;
		}
		else
		{
			if (bDebugInventory_Server || bDebugInventory_Client)
			{
				UE_LOGFMT(InventoryLog, Error, "({0}) {1}() failed to spawn item! {2}'s inventory, item: {3}",
					*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), *Execute_GetPlayerId(this), Item.ItemName
				);
			}
		}
	}

	return nullptr;
}


FName UInventoryComponent::GetItemId(const FGuid& Id, EItemType Type, UObject* OtherInventory)
{
	F_Item Item;
	Execute_GetItem(this, Item, Id, Type);

	if (Item.IsValid())
	{
		return Item.ItemName;
	}
	
	const TScriptInterface<IInventoryInterface> Inventory = OtherInventory;
	if (!Inventory.GetInterface())
	{
		return FName();
	}
	
	Inventory->Execute_GetItem(Inventory.GetObject(), Item, Id, Type);
	return Item.ItemName;
}


FString UInventoryComponent::GetPlayerId_Implementation() const
{
	return "[" + FString::FromInt(NetId) + "][" + PlatformId + "]";
}

void UInventoryComponent::SetPlayerId()
{
	// Save the net and platform id for determining the character (on both server and client)
	const UNetDriver* NetDriver = GetWorld()->GetNetDriver();
	PlatformId = FGenericPlatformMisc::GetLoginId();
	if (NetDriver && NetDriver->GuidCache.Get())
	{
		FNetGUIDCache* NetworkGuids = NetDriver->GuidCache.Get();
		if (NetworkGuids->NetGUIDLookup.Contains(this)) NetId = NetworkGuids->NetGUIDLookup[this].Value;
		// UE_LOGFMT(LogTemp, Log, "{0} network guid: {1}, net id: {2}", *GetName(), *NetworkGuids->NetGUIDLookup[this].ToString(), SaveData.NetId);
	}
}
#pragma endregion




#pragma region Print Inventory
void UInventoryComponent::ListInventory()
{
	if (!GetCharacter()) return;
	
	TArray<FGuid> ClientItems;
	for (auto &[Id, Item] : QuestItems) ClientItems.Add(Id);
	for (auto &[Id, Item] : CommonItems) ClientItems.Add(Id);
	for (auto &[Id, Item] : Weapons) ClientItems.Add(Id);
	for (auto &[Id, Item] : Armors) ClientItems.Add(Id);
	for (auto &[Id, Item] : Materials) ClientItems.Add(Id);
	for (auto &[Id, Item] : Notes) ClientItems.Add(Id);
	Server_ListInventory(ClientItems);
}


void UInventoryComponent::Server_ListInventory_Implementation(const TArray<FGuid>& ItemList)
{
	if (!GetCharacter()) return;
	
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//----------------------------------------------------------------------------------------------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "// {0}: [{1}][{2}] {3}'s Inventory", *UEnum::GetValueAsString(Character->GetLocalRole()), NetId, PlatformId, *GetNameSafe(Character));
	UE_LOGFMT(InventoryLog, Log, "//----------------------------------------------------------------------------------------------------------------------------------/");

	const TArray<FGuid> ClientItems = ItemList;
	if (!Weapons.IsEmpty()) ListInventoryMap(Weapons, ClientItems, FString("Armaments"));
	if (!Armors.IsEmpty()) ListInventoryMap(Armors, ClientItems, FString("Armors"));
	if (!CommonItems.IsEmpty()) ListInventoryMap(CommonItems, ClientItems, FString("Common Items"));
	if (!QuestItems.IsEmpty()) ListInventoryMap(QuestItems, ClientItems, FString("Quest Items"));
	if (!Materials.IsEmpty()) ListInventoryMap(Materials, ClientItems, FString("Materials"));
	if (!Notes.IsEmpty()) ListInventoryMap(Notes, ClientItems, FString("Notes"));
	
	UE_LOGFMT(InventoryLog, Log, "//----------------------------------------------------------------------------------------------------------------------------------//");
	UE_LOGFMT(InventoryLog, Log, " ");
}


void UInventoryComponent::ListInventoryMap(const TMap<FGuid, F_Item>& Map, const TArray<FGuid>& ClientItems, FString ListName)
{
	if (!GetCharacter()) return;

	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, "// {0} ", ListName);
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, " ");
	for (auto &[Id, Item] : Map) ListInventoryItem(Item, ClientItems.Contains(Id));
}


void UInventoryComponent::ListInventoryItem(const F_Item& Item, bool bClientContainsItem)
{
	if (!GetCharacter() || !Item.IsValid())
	{
		UE_LOGFMT(InventoryLog, Error, "{0}: Print Item was called with an invalid item. {1} {2}()", *UEnum::GetValueAsString(Character->GetLocalRole()), *Execute_GetPlayerId(this), *FString(__FUNCTION__));
		return;
	}

	UE_LOGFMT(InventoryLog, Log, "|--------------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "| ({0}) {1}, OnClient: {2}", Item.SortOrder, *Item.DisplayName, bClientContainsItem);
	UE_LOGFMT(InventoryLog, Log, "|-------------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "| Id: {0} ->  {1}", Item.ItemName, *Item.Id.ToString());
	UE_LOGFMT(InventoryLog, Log, "| Type: {0}", *UEnum::GetValueAsString(Item.ItemType));
	UE_LOGFMT(InventoryLog, Log, "| Description: {0}", *Item.Description);
	UE_LOGFMT(InventoryLog, Log, "| ActualClass: {0}", Item.ActualClass ? *Item.ActualClass->GetName() : *FString("null"));
	UE_LOGFMT(InventoryLog, Log, "| WorldClass: {0}", Item.WorldClass ? *Item.WorldClass->GetName() : *FString("null"));
	UE_LOGFMT(InventoryLog, Log, "|--------------------------------------------//");
	UE_LOGFMT(InventoryLog, Log, " ");
}




#pragma region Saved Items
void UInventoryComponent::ListSavedInventory(const F_InventorySaveInformation& Data)
{
	if (!GetCharacter()) return;
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//----------------------------------------------------------------------------------------------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "// {0}: [{1}][{2}] {3}'s Inventory", *UEnum::GetValueAsString(Character->GetLocalRole()), NetId, PlatformId, *GetNameSafe(Character));
	UE_LOGFMT(InventoryLog, Log, "//----------------------------------------------------------------------------------------------------------------------------------/");
	ListSavedItems(Data.InventoryItems, "Saved Items");
	UE_LOGFMT(InventoryLog, Log, "//----------------------------------------------------------------------------------------------------------------------------------//");
	UE_LOGFMT(InventoryLog, Log, " ");
}


void UInventoryComponent::ListSavedItems(const TArray<FS_Item>& List, FString ListName)
{
	if (!GetCharacter()) return;

	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, "// {0} ", ListName);
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, " ");
	for (FS_Item SavedItem : List)
	{
		ListSavedItem(SavedItem);
	}
}


void UInventoryComponent::ListSavedItem(const FS_Item& SavedItem)
{
	if (!GetCharacter() || !SavedItem.IsValid())
	{
		UE_LOGFMT(InventoryLog, Error, "{0}: ListSavedItem was called with an invalid item. {1} {2}()", *UEnum::GetValueAsString(Character->GetLocalRole()), *Execute_GetPlayerId(this), *FString(__FUNCTION__));
		return;
	}

	UE_LOGFMT(InventoryLog, Log, "({0}) {1}({2})", SavedItem.SortOrder, SavedItem.ItemName, SavedItem.Id.ToString());
}
#pragma endregion 
#pragma endregion 

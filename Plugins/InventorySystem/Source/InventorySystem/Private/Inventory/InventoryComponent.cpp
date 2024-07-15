// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/InventoryComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Inventory/InventoryInterface.h"
#include "Item/BaseItem.h"
#include "Item/InventoryItemInterface.h"
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
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() added item {2}: {3}, databaseId: {4}, id: {5}", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), *GetNameSafe(GetOwner()), bSuccessfullyAddedItem ? "succeeded" : "failed", DatabaseId,  *Id.ToString()
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
			*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), *GetNameSafe(GetOwner()), DatabaseId, *Id.ToString()
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
			*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), bSuccess ? "succeeded" : "failed", *GetNameSafe(GetOwner()), DatabaseId, *Id.ToString()
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
	// Unhide the item and add any other necessary logic and undo the ui
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
		Client_TransferItemResponse(bSuccessfullyTransferredItem, Id, OtherInventoryInterface, Type, bFromThisInventory);
		return true;
	}

	return false;
}


void UInventoryComponent::Server_TryTransferItem_Implementation(const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type)
{
	bool bFromThisInventory;
	const bool bSuccessfullyTransferredItem = Execute_HandleTransferItem(this, Id, OtherInventoryInterface, Type, bFromThisInventory);
	Client_TransferItemResponse(bSuccessfullyTransferredItem, Id, OtherInventoryInterface, Type, bFromThisInventory);
	
	if (bDebugInventory_Server)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() {2} transferred an item to {3}, id: {4}", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), bSuccessfullyTransferredItem ? "succeeded" : "failed",
			bFromThisInventory ? *GetNameSafe(GetOwner()) : *GetNameSafe(OtherInventoryInterface),
			bFromThisInventory ? *GetNameSafe(OtherInventoryInterface) : *GetNameSafe(GetOwner()),
			*Id.ToString()
		);
	}
}


void UInventoryComponent::Client_TransferItemResponse_Implementation(const bool bSuccess, const FGuid& Id, UObject* OtherInventoryInterface, const EItemType Type, const bool bFromThisInventory)
{
	const TScriptInterface<IInventoryInterface> OtherInventory = OtherInventoryInterface;
	if (!bSuccess)
	{
		Execute_HandleTransferItemFail(this, Id, OtherInventoryInterface, bFromThisInventory);
		OnInventoryItemTransferFailure.Broadcast(Id, OtherInventory, bFromThisInventory);
	}
	else
	{
		bool bWasFromThisInventory = bFromThisInventory;
		if (ROLE_AutonomousProxy == GetOwner()->GetLocalRole()) // TODO: are extra checks on clients necessary?
		{
			Execute_HandleTransferItem(this, Id, OtherInventoryInterface, Type, bWasFromThisInventory);
		}
		
		Execute_HandleTransferItemSuccess(this, Id, OtherInventoryInterface, bFromThisInventory);
		OnInventoryItemTransferSuccess.Broadcast(Id, OtherInventory, bFromThisInventory);
	}
	
	if (bDebugInventory_Client)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() TransferItemResponse: {2}, {3} transfer item operation ->  {4}, id: {5} ", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), bSuccess ? "succeeded" : "failed",
			bFromThisInventory ? *GetNameSafe(GetOwner()) : *GetNameSafe(OtherInventoryInterface),
			bFromThisInventory ? *GetNameSafe(OtherInventoryInterface) : *GetNameSafe(GetOwner()),
			*Id.ToString()
		);
	}
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
		bFromThisInventory = false;
		Item = OtherInventory->Execute_InternalGetInventoryItem(OtherInventory.GetObject(), Id, Type);
	}

	if (!Item.IsValid())
	{
		if (bDebugInventory_Server || bDebugInventory_Client)
		{
			UE_LOGFMT(InventoryLog, Error, "({0}) {1}() (invalid/not found) item: {2} searched in {3}'s inventory", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
				*FString(__FUNCTION__), *Id.ToString(), bFromThisInventory ? *GetNameSafe(GetOwner()) : *GetNameSafe(OtherInventoryInterface)
			);
		}
		return false;
	}

	// Transfer the item
	if (bFromThisInventory)
	{
		Execute_InternalRemoveInventoryItem(this, Item.Id, Item.ItemType);
		OtherInventory->Execute_InternalAddInventoryItem(OtherInventory.GetObject(), Item);
	}
	else
	{
		Execute_InternalAddInventoryItem(this, Item);
		OtherInventory->Execute_InternalRemoveInventoryItem(OtherInventory.GetObject(), Item.Id, Item.ItemType);
	}

	if (bDebugInventory_Server || bDebugInventory_Client)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() InventoryTransfer: {2} from {3} to {4}'s inventory", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), *Id.ToString(),
			bFromThisInventory ? *GetNameSafe(GetOwner()) : *GetNameSafe(OtherInventoryInterface),
			bFromThisInventory ? *GetNameSafe(OtherInventoryInterface) : *GetNameSafe(GetOwner())
		);
	}
	return true;
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
		const bool bSuccessfullyRemovedItem = Execute_HandleRemoveItem(this, Id, Type, bDropItem, SpawnedItem);
		Client_RemoveItemResponse(bSuccessfullyRemovedItem, Id, Type, bDropItem, SpawnedItem);
		return true;
	}

	return false;
}


void UInventoryComponent::Server_TryRemoveItem_Implementation(const FGuid& Id, const EItemType Type, const bool bDropItem)
{
	UObject* SpawnedItem;
	const bool bSuccessfullyRemovedItem = Execute_HandleRemoveItem(this, Id, Type, bDropItem, SpawnedItem);
	Client_RemoveItemResponse(bSuccessfullyRemovedItem, Id, Type, bDropItem, SpawnedItem);
	
	if (bDebugInventory_Server)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() removed item {2}: {3}, id: {4}", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), *GetNameSafe(GetOwner()), bSuccessfullyRemovedItem ? "succeeded" : "failed",  *Id.ToString()
		);
	}
}


void UInventoryComponent::Client_RemoveItemResponse_Implementation(const bool bSuccess, const FGuid& Id, const EItemType Type, const bool bDropItem, UObject* SpawnedItem)
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
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() RemoveItemResponse: {2}, {3} remove item operation ->  {4}{5} ", *UEnum::GetValueAsString(GetOwner()->GetLocalRole()),
			*FString(__FUNCTION__), bSuccess ? "succeeded" : "failed", *GetNameSafe(GetOwner()), *Id.ToString(), bDropItem ? "(dropped)" : ""
		);
	}
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
					*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), *GetNameSafe(GetOwner()), *Id.ToString()
				);
			}
			return false;
		}

		const TScriptInterface<IInventoryItemInterface> InventoryItem = Execute_SpawnWorldItem(this, Item, GetOwner()->GetActorTransform());
		SpawnedItem = InventoryItem ? InventoryItem.GetObject() : nullptr;
	}

	if (bDebugInventory_Server || bDebugInventory_Client)
	{
		UE_LOGFMT(InventoryLog, Log, "({0}) {1}() InventoryRemoval: {2} - {3} ",
			*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), *GetNameSafe(GetOwner()), *Id.ToString()
		);
	}
	
	Execute_InternalRemoveInventoryItem(this, Id, Type);
	return true;
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


bool UInventoryComponent::GetItem_Implementation(F_Item& ReturnedItem, FGuid Id, EItemType InventorySectionToSearch)
{
	if (!Id.IsValid()) return false;

	// search for the item in the inventory
	ReturnedItem = Execute_InternalGetInventoryItem(this, Id, InventorySectionToSearch);
	if (ReturnedItem.IsValid()) return true;
	return false;
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

		if (ABaseItem* SpawnedItem = GetWorld()->SpawnActor<ABaseItem>(Item.WorldClass, SpawnTransform, SpawnParameters))
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
					*UEnum::GetValueAsString(GetOwner()->GetLocalRole()), *FString(__FUNCTION__), *GetNameSafe(GetOwner()), Item.ItemName
				);
			}
		}
	}

	return nullptr;
}
#pragma endregion




#pragma region Print Inventory
void UInventoryComponent::ListInventoryItem(const F_Item& Item)
{
	if (!GetCharacter() || !Item.IsValid())
	{
		UE_LOGFMT(InventoryLog, Error, "{0}: Print Item was called with an invalid item. {1} {2}()", *UEnum::GetValueAsString(Character->GetLocalRole()), *GetNameSafe(Character), *FString(__FUNCTION__));
		return;
	}

	UE_LOGFMT(InventoryLog, Log, "|------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "| ({0}) {1}", Item.SortOrder, *Item.DisplayName);
	UE_LOGFMT(InventoryLog, Log, "|-----------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "| DatabaseId: {0}", Item.ItemName);
	UE_LOGFMT(InventoryLog, Log, "| Id: {0}", *Item.Id.ToString());
	UE_LOGFMT(InventoryLog, Log, "| Type: {0}", *UEnum::GetValueAsString(Item.ItemType));
	UE_LOGFMT(InventoryLog, Log, "| Description: {0}", *Item.Description);
	UE_LOGFMT(InventoryLog, Log, "| ActualClass: {0}", Item.ActualClass ? *Item.ActualClass->GetName() : *FString("null"));
	UE_LOGFMT(InventoryLog, Log, "| WorldClass: {0}", Item.WorldClass ? *Item.WorldClass->GetName() : *FString("null"));
	UE_LOGFMT(InventoryLog, Log, "|------------------------------------//");
	UE_LOGFMT(InventoryLog, Log, " ");
}


void UInventoryComponent::ListInventoryMap(const TMap<FGuid, F_Item>& Map, FString ListName)
{
	if (!GetCharacter()) return;

	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, "// {0} ", ListName);
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, " ");
	for (auto &[Id, Item] : Map)
	{
		ListInventoryItem(Item);
	}
}


void UInventoryComponent::ListInventory()
{
	if (!GetCharacter()) return;
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//---------------------------------------------------------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "// ({0}) {1}'s Inventory", *UEnum::GetValueAsString(Character->GetLocalRole()), *GetNameSafe(Character));
	UE_LOGFMT(InventoryLog, Log, "//--------------------------------------------------------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, " ");

	ListInventoryMap(Weapons, FString("Armaments"));
	ListInventoryMap(Armors, FString("Armors"));
	ListInventoryMap(CommonItems, FString("Common Items"));
	ListInventoryMap(QuestItems, FString("Quest Items"));

	/*
	if (GetCombatBase())
	{
		TMap<FGuid, F_Item> LeftHandArmaments;
		LeftHandArmaments.Add(CombatBase->LeftHandArmament_Slot_One.Id, CombatBase->LeftHandArmament_Slot_One);
		LeftHandArmaments.Add(CombatBase->LeftHandArmament_Slot_Two.Id, CombatBase->LeftHandArmament_Slot_Two);
		LeftHandArmaments.Add(CombatBase->LeftHandArmament_Slot_Three.Id, CombatBase->LeftHandArmament_Slot_Three);
		
		TMap<FGuid, F_Item> RightHandArmaments;
		RightHandArmaments.Add(CombatBase->RightHandArmament_Slot_One.Id, CombatBase->RightHandArmament_Slot_One);
		RightHandArmaments.Add(CombatBase->RightHandArmament_Slot_Two.Id, CombatBase->RightHandArmament_Slot_Two);
		RightHandArmaments.Add(CombatBase->RightHandArmament_Slot_Three.Id, CombatBase->RightHandArmament_Slot_Three);
		
		ListInventoryMap(LeftHandArmaments, FString("Left Hand Equip Slots"));
		ListInventoryMap(RightHandArmaments, FString("Right Hand Equip Slots"));
	}
	*/
	
	UE_LOGFMT(InventoryLog, Log, "//---------------------------------------------------------------------------------------//");
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, " ");
}


void UInventoryComponent::ListSavedItem(const FS_Item& SavedItem)
{
	if (!GetCharacter() || !SavedItem.IsValid())
	{
		UE_LOGFMT(InventoryLog, Error, "{0}: ListSavedItem was called with an invalid item. {1} {2}()", *UEnum::GetValueAsString(Character->GetLocalRole()), *GetNameSafe(Character), *FString(__FUNCTION__));
		return;
	}

	UE_LOGFMT(InventoryLog, Log, "({0}) DatabaseId: {1}, Id: {2}", SavedItem.SortOrder, SavedItem.ItemName, SavedItem.Id.ToString());
}


void UInventoryComponent::ListSavedItems(const TArray<FS_Item>& List, FString ListName)
{
	if (!GetCharacter()) return;

	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, "// {0} ", ListName);
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, " ");
	for (FS_Item SavedItem : List)
	{
		ListSavedItem(SavedItem);
	}
}


void UInventoryComponent::ListSavedWeaponInformation(const FS_WeaponInformation& SavedWeapon)
{
	if (!GetCharacter() || !SavedWeapon.Id.IsValid())
	{
		UE_LOGFMT(InventoryLog, Error, "{0}: ListSavedWeaponInformation was called with an invalid weapon. {1} {2}()", *UEnum::GetValueAsString(Character->GetLocalRole()), *GetNameSafe(Character), *FString(__FUNCTION__));
		return;
	}

	if (!Weapons.Contains(SavedWeapon.Id))
	{
		UE_LOGFMT(InventoryLog, Error, "{0}: {1} -> Did not find the weapon in the player's inventroy! {2}", *UEnum::GetValueAsString(Character->GetLocalRole()), *GetNameSafe(Character), *SavedWeapon.Id.ToString());
		return;
	}
	
	UE_LOGFMT(InventoryLog, Log, "{1} Id: {1}, Level: {2}", *Weapons[SavedWeapon.Id].DisplayName, SavedWeapon.Id.ToString(), SavedWeapon.Level);
}


void UInventoryComponent::ListSavedWeapons(const TArray<FS_WeaponInformation>& List)
{
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, "// Saved Weapon Information ");
	UE_LOGFMT(InventoryLog, Log, "//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/");
	UE_LOGFMT(InventoryLog, Log, " ");
	for (FS_WeaponInformation SavedWeapon : List)
	{
		ListSavedWeaponInformation(SavedWeapon);
	}
}


void UInventoryComponent::ListSavedCharacterInformation(const FS_CharacterInformation& Data, FString Message)
{
	if (!GetCharacter()) return;
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//---------------------------------------------------------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "// ({0}) {1} Save/Load data ->  {2}", *UEnum::GetValueAsString(Character->GetLocalRole()), *GetNameSafe(Character), *Message);
	UE_LOGFMT(InventoryLog, Log, "//--------------------------------------------------------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "NetId: {0}", Data.NetId);
	UE_LOGFMT(InventoryLog, Log, "PlatformId: {0}", Data.PlatformId);
	UE_LOGFMT(InventoryLog, Log, " ");

	
	// UE_LOGFMT(InventoryLog, Log, "|------------------------------------------/");
	// UE_LOGFMT(InventoryLog, Log, "| Weapons");
	// UE_LOGFMT(InventoryLog, Log, "|-----------------------------------------/");
	// if (Data.LeftHandWeapons.Num() > 0)
	// {
	// 	ListSavedItem(FS_Item(Data.LeftHandWeapons[0].Id, Data.LeftHandWeapons[0].SortOrder, Data.LeftHandWeapons[0].ItemId, Data.LeftHandWeapons[0].RowName));
	// 	if (Data.LeftHandWeapons.Num() > 1) ListSavedItem(FS_Item(Data.LeftHandWeapons[1].Id, Data.LeftHandWeapons[1].SortOrder, Data.LeftHandWeapons[1].ItemId, Data.LeftHandWeapons[1].RowName));
	// 	if (Data.LeftHandWeapons.Num() > 2) ListSavedItem(FS_Item(Data.LeftHandWeapons[2].Id, Data.LeftHandWeapons[2].SortOrder, Data.LeftHandWeapons[2].ItemId, Data.LeftHandWeapons[2].RowName));
	// }
	// if (Data.RightHandWeapons.Num() > 0)
	// {
	// 	ListSavedItem(FS_Item(Data.RightHandWeapons[0].Id, Data.RightHandWeapons[0].SortOrder, Data.RightHandWeapons[0].ItemId, Data.RightHandWeapons[0].RowName));
	// 	if (Data.RightHandWeapons.Num() > 1) ListSavedItem(FS_Item(Data.RightHandWeapons[1].Id, Data.RightHandWeapons[1].SortOrder, Data.RightHandWeapons[1].ItemId, Data.RightHandWeapons[1].RowName));
	// 	if (Data.RightHandWeapons.Num() > 2) ListSavedItem(FS_Item(Data.RightHandWeapons[2].Id, Data.RightHandWeapons[2].SortOrder, Data.RightHandWeapons[2].ItemId, Data.RightHandWeapons[2].RowName));
	// }
	// UE_LOGFMT(InventoryLog, Log, " ");
	//
	// UE_LOGFMT(InventoryLog, Log, "|------------------------------------------/");
	// UE_LOGFMT(InventoryLog, Log, "| Armor");
	// UE_LOGFMT(InventoryLog, Log, "|-----------------------------------------/");
	// if (Data.Gauntlets.IsValid()) ListSavedItem(FS_Item(Data.Gauntlets.Id, Data.Gauntlets.SortOrder, Data.Gauntlets.ItemId, Data.Gauntlets.RowName));
	// if (Data.Leggings.IsValid()) ListSavedItem(FS_Item(Data.Leggings.Id, Data.Leggings.SortOrder, Data.Leggings.ItemId, Data.Leggings.RowName));
	// if (Data.Helm.IsValid()) ListSavedItem(FS_Item(Data.Helm.Id, Data.Helm.SortOrder, Data.Helm.ItemId, Data.Helm.RowName));
	// if (Data.Chest.IsValid()) ListSavedItem(FS_Item(Data.Chest.Id, Data.Chest.SortOrder, Data.Chest.ItemId, Data.Chest.RowName));
	// UE_LOGFMT(InventoryLog, Log, " ");
	
	ListSavedItems(Data.InventoryItems, "Saved Items");
	// ListSavedWeapons(Data.WeaponInformation);
	UE_LOGFMT(InventoryLog, Log, "//---------------------------------------------------------------------------------------//");
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, " ");
}
#pragma endregion 

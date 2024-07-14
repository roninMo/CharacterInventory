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
bool UInventoryComponent::TryAddItem_Implementation(const FName Id, TScriptInterface<IInventoryItemInterface> InventoryItem, const EItemType Type)
{
	if (!GetCharacter() || !Id.IsNone()) return false;

	// If the server calls the function, just handle it and send the updated information to the client. Otherwise handle sending the information to the server and then back to the client
	if (Character->IsLocallyControlled())
	{
		AddItemPendingClientLogic(Id, InventoryItem, Type);
		Server_TryAddItem(Id, InventoryItem, Type);
		return true; // Just return true by default and let the client rpc response handle everything else
	}
	else if (Character->HasAuthority())
	{
		const FGuid ItemId = HandleAddItem(Id, InventoryItem, Type);
		Client_AddItemResponse(ItemId.IsValid() ? true : false, Id, InventoryItem, Type);
		return true;
	}

	return false;
}


void UInventoryComponent::Server_TryAddItem_Implementation(FName Id, TScriptInterface<IInventoryItemInterface> InventoryItem, const EItemType Type)
{
	bool bSuccessfullyAddedItem;
	
	// Adding an item by id
	if (!InventoryItem) bSuccessfullyAddedItem = HandleAddItem(Id, InventoryItem, Type) ? true : false;
	else
	// Adding an item from the world
	{
		// Don't allow players to interfere with items that are already being adjusted 
		if (!InventoryItem->IsSafeToAdjustItem()) bSuccessfullyAddedItem = false;
		else
		{
			InventoryItem->SetPlayerPending(Character);
			bSuccessfullyAddedItem = HandleAddItem(Id, InventoryItem, Type) ? true : false;
		}
	}
	
	// Remove the scope lock
	if (InventoryItem->GetPlayerPending() == Character) InventoryItem->SetPlayerPending(nullptr);
	Client_AddItemResponse(bSuccessfullyAddedItem, Id, InventoryItem, Type);
}
void UInventoryComponent::Client_AddItemResponse_Implementation(const bool bSuccess, FName Id, TScriptInterface<IInventoryItemInterface> InventoryItem, const EItemType Type)
{
	if (!bSuccess)
	{
		HandleItemAdditionFail(Id, InventoryItem, Type);
		OnInventoryItemAdditionFailure.Broadcast(Id, InventoryItem);
	}
	else
	{
		const F_Item Item = HandleAddItem(Id, InventoryItem, Type); // TODO: are extra checks on clients necessary?
		HandleItemAdditionSuccess(Item.Id, InventoryItem, Type);
		OnInventoryItemAdditionSuccess.Broadcast(Item, InventoryItem);
	}
}
FGuid UInventoryComponent::HandleAddItem_Implementation(const FName Id, TScriptInterface<IInventoryItemInterface> InventoryItem, const EItemType Type)
{
	F_Item Item;
	if (InventoryItem) Item = InventoryItem->GetItem();
	if (!Item.IsValid()) GetDataBaseItem(Id, Item);

	if (Item.IsValid())
	{
		TMap<FGuid, F_Item>& InventoryList = GetInventoryList(Type);
		InventoryList.Add(Item.Id, Item);
		return Item.Id;
	}

	return FGuid();
}


void UInventoryComponent::AddItemPendingClientLogic_Implementation(const FName Id, TScriptInterface<IInventoryItemInterface> InventoryItem, const EItemType Type)
{
	// Hide the item in the world but do not delete it until the process is complete and update the ui
	if (InventoryItem && Cast<AActor>(InventoryItem.GetObject()))
	{
		AActor* WorldItem = Cast<AActor>(InventoryItem.GetObject());
		WorldItem->SetActorHiddenInGame(true);
	}
	
	// UpdateWidgetDisplay(Data);
}

void UInventoryComponent::HandleItemAdditionFail_Implementation(const FName Id, TScriptInterface<IInventoryItemInterface> InventoryItem, const EItemType Type)
{
	// Unhide the item and add any other necessary logic and undo the ui
	if (InventoryItem && Cast<AActor>(InventoryItem.GetObject()))
	{
		AActor* WorldItem = Cast<AActor>(InventoryItem.GetObject());
		WorldItem->SetActorHiddenInGame(false);
	}
	
	// UpdateWidgetDisplay(Data, true, true);
}

void UInventoryComponent::HandleItemAdditionSuccess_Implementation(const FGuid& Id, TScriptInterface<IInventoryItemInterface> InventoryItem, const EItemType Type)
{
	// delete the item
	if (InventoryItem && Cast<AActor>(InventoryItem.GetObject()))
	{
		AActor* WorldItem = Cast<AActor>(InventoryItem.GetObject());
		WorldItem->Destroy();
	}
}
#pragma endregion 




#pragma region Transfer Item
bool UInventoryComponent::TryTransferItem_Implementation(const FGuid& Id, TScriptInterface<IInventoryInterface> OtherInventory, const EItemType Type)
{
	if (!GetCharacter() || !Id.IsValid() || !OtherInventory) return false;

	// If the server calls the function, just handle it and send the updated information to the client. Otherwise handle sending the information to the server and then back to the client
	if (Character->IsLocallyControlled())
	{
		TransferItemPendingClientLogic(Id, OtherInventory, Type);
		Server_TryTransferItem(Id, OtherInventory, Type);
		return true; // Just return true by default and let the client rpc response handle everything else
	}
	else if (Character->HasAuthority())
	{
		bool bFromThisInventory; 
		const bool bSuccessfullyTransferredItem = HandleTransferItem(Id, OtherInventory, Type, bFromThisInventory);
		Client_TransferItemResponse(bSuccessfullyTransferredItem, Id, OtherInventory, Type, bFromThisInventory);
		return true;
	}

	return false;
}



void UInventoryComponent::Server_TryTransferItem_Implementation(const FGuid& Id, TScriptInterface<IInventoryInterface> OtherInventory, const EItemType Type)
{
	bool bFromThisInventory;
	const bool bSuccessfullyTransferredItem = HandleTransferItem(Id, OtherInventory, Type, bFromThisInventory);
	Client_TransferItemResponse(bSuccessfullyTransferredItem, Id, OtherInventory, Type, bFromThisInventory);
}
void UInventoryComponent::Client_TransferItemResponse_Implementation(const bool bSuccess, const FGuid& Id, TScriptInterface<IInventoryInterface> OtherInventory, const EItemType Type, const bool& bFromThisInventory)
{
	if (!bSuccess)
	{
		HandleTransferItemFail(Id, OtherInventory, bFromThisInventory);
		OnInventoryItemTransferFailure.Broadcast(Id, OtherInventory, bFromThisInventory);
	}
	else
	{
		bool bWasFromThisInventory = bFromThisInventory;
		HandleTransferItem(Id, OtherInventory, Type, bWasFromThisInventory); // TODO: are extra checks on clients necessary?
		HandleTransferItemSuccess(Id, OtherInventory, bFromThisInventory);
		OnInventoryItemTransferSuccess.Broadcast(Id, OtherInventory, bFromThisInventory);
	}
}
bool UInventoryComponent::HandleTransferItem_Implementation(const FGuid& Id, TScriptInterface<IInventoryInterface> OtherInventory, const EItemType Type, bool& bFromThisInventory)
{
	// Find the item, and then transfer it to the other inventory
	if (!Id.IsValid() || !OtherInventory) return false;
	F_Item Item;

	// Search for the item in the player's inventory
	GetItem(Item, Id);
	if (Item.IsValid()) bFromThisInventory = true;
	else
	{
		bFromThisInventory = false;
		Item = OtherInventory->InternalGetInventoryItem(Id);
	}

	if (!Item.IsValid())
	{
		return false;
	}

	// Transfer the item
	if (bFromThisInventory)
	{
		InternalRemoveInventoryItem(Item.Id, Item.ItemType);
		OtherInventory->InternalAddInventoryItem(Item);
	}
	else
	{
		InternalAddInventoryItem(Item);
		OtherInventory->InternalRemoveInventoryItem(Item.Id, Item.ItemType);
	}

	return true;
}


void UInventoryComponent::TransferItemPendingClientLogic_Implementation(const FGuid& Id, TScriptInterface<IInventoryInterface> OtherInventory, const EItemType Type)
{
	// Update the inventory widgets for both inventory components to show the item
	// UpdateWidgetDisplay(Data);
}

void UInventoryComponent::HandleTransferItemFail_Implementation(const FGuid& Id, TScriptInterface<IInventoryInterface> OtherInventory, bool bFromThisInventory)
{
	// Undo the updates to the inventory widgets
	// UpdateWidgetDisplay(Data);
}

void UInventoryComponent::HandleTransferItemSuccess_Implementation(const FGuid& Id, TScriptInterface<IInventoryInterface> OtherInventory, bool bFromThisInventory)
{
}
#pragma endregion 




#pragma region Remove Item
bool UInventoryComponent::TryRemoveItem_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem)
{
	if (!GetCharacter() || !Id.IsValid()) return false;

	// If the server calls the function, just handle it and send the updated information to the client. Otherwise handle sending the information to the server and then back to the client
	if (Character->IsLocallyControlled())
	{
		RemoveItemPendingClientLogic(Id, Type, bDropItem);
		Server_TryRemoveItem(Id, Type, bDropItem);
		return true; // Just return true by default and let the client rpc response handle everything else
	}
	else if (Character->HasAuthority())
	{
		TScriptInterface<IInventoryItemInterface> SpawnedItem;
		const bool bSuccessfullyRemovedItem = HandleRemoveItem(Id, Type, bDropItem, SpawnedItem);
		Client_RemoveItemResponse(bSuccessfullyRemovedItem, Id, Type, bDropItem, SpawnedItem);
		return true;
	}

	return false;
}


void UInventoryComponent::Server_TryRemoveItem_Implementation(const FGuid& Id, const EItemType Type, const bool bDropItem)
{
	TScriptInterface<IInventoryItemInterface> SpawnedItem;
	const bool bSuccessfullyRemovedItem = HandleRemoveItem(Id, Type, bDropItem, SpawnedItem);
	Client_RemoveItemResponse(bSuccessfullyRemovedItem, Id, Type, bDropItem, SpawnedItem);
	
}
void UInventoryComponent::Client_RemoveItemResponse_Implementation(const bool bSuccess, const FGuid& Id, const EItemType Type, bool bDropItem, TScriptInterface<IInventoryItemInterface> SpawnedItem)
{
	if (!bSuccess)
	{
		HandleRemoveItemFail(Id, Type, bDropItem, SpawnedItem);
		OnInventoryItemRemovalFailure.Broadcast(Id, SpawnedItem);
	}
	else
	{
		HandleRemoveItemSuccess(Id, Type, bDropItem, SpawnedItem);
		OnInventoryItemRemovalSuccess.Broadcast(Id, SpawnedItem);
	}
}
bool UInventoryComponent::HandleRemoveItem_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, TScriptInterface<IInventoryItemInterface>& SpawnedItem)
{
	if (bDropItem)
	{
		F_Item Item;
		GetItem(Item, Id, Type);
		
		if (!Item.IsValid())
		{
			return false;
		}

		SpawnedItem = SpawnWorldItem(Item);
	}

	InternalRemoveInventoryItem(Id, Type);
	return true;
}


void UInventoryComponent::RemoveItemPendingClientLogic_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem)
{
	// Update the inventory widgets for both inventory components to hide the item
	// UpdateWidgetDisplay(Data);
}

void UInventoryComponent::HandleRemoveItemFail_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, TScriptInterface<IInventoryItemInterface> SpawnedItem)
{
	// Undo the updates to the inventory widgets
	// UpdateWidgetDisplay(Data);
}

void UInventoryComponent::HandleRemoveItemSuccess_Implementation(const FGuid& Id, const EItemType Type, bool bDropItem, TScriptInterface<IInventoryItemInterface> SpawnedItem)
{
}
#pragma endregion 
#pragma endregion




#pragma region Utility
F_Item UInventoryComponent::InternalGetInventoryItem_Implementation(const FGuid& Id, EItemType InventorySectionToSearch)
{
	F_Item Item;

	// Search for the item in the inventory, and return it
	GetItem(Item, Id, InventorySectionToSearch);
	return Item;
}


void UInventoryComponent::InternalRemoveInventoryItem_Implementation(const FGuid& Id, EItemType InventorySectionToSearch)
{
	TMap<FGuid, F_Item>& InventoryList = GetInventoryList(InventorySectionToSearch);
	if (InventoryList.Contains(Id)) InventoryList.Remove(Id);
}


void UInventoryComponent::InternalAddInventoryItem_Implementation(const F_Item& Item)
{
	TMap<FGuid, F_Item>& InventoryList = GetInventoryList(Item.ItemType);
	InventoryList.Add(Item.Id, Item);
}


bool UInventoryComponent::GetItem_Implementation(F_Item& ReturnedItem, FGuid Id, EItemType InventorySectionToSearch)
{
	if (!Id.IsValid()) return false;
	if (EItemType::Inv_None != InventorySectionToSearch)
	{
		TMap<FGuid, F_Item>& InventoryList = GetInventoryList(InventorySectionToSearch);
		if (InventoryList.Contains(Id))
		{
			ReturnedItem = InventoryList[Id];
			return true;
		}
	}

	// if nothing is specified then search through everything
	for (int i = 0; i < static_cast<int>(EItemType::Inv_MAX); i++)
	{
		const EItemType ItemType = static_cast<EItemType>(i);
		TMap<FGuid, F_Item>& InventoryList = GetInventoryList(ItemType);
		if (InventoryList.Contains(Id))
		{
			ReturnedItem = InventoryList[Id];
			return true;
		}
	}

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


bool UInventoryComponent::GetDataBaseItem_Implementation(FName Id, F_Item& Item)
{
	if (!ItemDatabase || Id.IsNone()) return false;
	
	F_Table_ItemData* ItemData = ItemDatabase->FindRow<F_Table_ItemData>(Id, TEXT("Inventory Item Data Context"));
	if (ItemData)
	{
		Item.Id = FGuid::NewGuid();
		Item = ItemData->ItemInformation;
	}

	return false;
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


TScriptInterface<IInventoryItemInterface> UInventoryComponent::SpawnWorldItem_Implementation(const F_Item& Item)
{
	if (GetWorld() && Item.WorldClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.Owner = GetOwner();
		FTransform SpawnTransform = Character->GetActorTransform();
		FVector SpawnLocation = SpawnTransform.GetLocation();
		SpawnLocation.Z = SpawnLocation.Z + (Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 43.0f);
		SpawnTransform.SetLocation(SpawnLocation);
		
		ABaseItem* SpawnedItem = GetWorld()->SpawnActor<ABaseItem>(Item.WorldClass, SpawnTransform, SpawnParameters);
		if (SpawnedItem)
		{
			const TScriptInterface<IInventoryItemInterface> WorldItem = SpawnedItem;
			SpawnedItem->SetItem(Item);
			SpawnedItem->SetId(Item.Id); // Persist the unique id of this inventory object
			return WorldItem;
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
	UE_LOGFMT(InventoryLog, Log, "// {0}: Starting to print {1}'s inventory. {2}()", *UEnum::GetValueAsString(Character->GetLocalRole()), *GetNameSafe(Character), *FString(__FUNCTION__));
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, " ");
	UE_LOGFMT(InventoryLog, Log, "//---------------------------------------------------------------------------------------------/");
	UE_LOGFMT(InventoryLog, Log, "// {0}'s Inventory", *GetNameSafe(Character));
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
	UE_LOGFMT(InventoryLog, Log, "// {0} {1} Save/Load data ->  {2}", *UEnum::GetValueAsString(Character->GetLocalRole()), *GetNameSafe(Character), *Message);
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



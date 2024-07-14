// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/BaseItem.h"

#include "Inventory/InventoryComponent.h"
#include "Logging/StructuredLog.h"

// Sets default values
ABaseItem::ABaseItem()
{
	// Network values
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	AActor::SetReplicateMovement(true);

	// Item information
	Item.Id = FGuid();
	Item.SortOrder = -1;
	Item.ItemName = NAME_None;
	Item.DisplayName = "Rock";

	Item.Description = "This world item's values haven't been set yet!";
	Item.InteractText = "Press E to pickup";
	Item.ItemType = EItemType::Inv_None;
	Item.Image = nullptr;

	Item.ActualClass = nullptr;
	Item.WorldClass = nullptr;
	Item.GlobalInformation = nullptr;
}


void ABaseItem::InitializeItemGlobals()
{
	// Ex item globals function.
	if (!Item.GlobalInformation) return;
	F_Item GlobalItem;
	
	// const UData_DefaultItemInformation* Information = Cast<UData_CharacterGlobals>(Data);
	// ItemHighlightColor = Information->ItemHighlightColor;
}


void ABaseItem::BeginPlay()
{
	Super::BeginPlay();
	InitializeItemGlobals();

	if (ItemInformationTable)
	{
		RetrieveItemFromDataTable(TableId, Item);
	}
	else UE_LOG(LogTemp, Error, TEXT("%s() %s: item information data table, set this up in the blueprint!"), *FString(__FUNCTION__), *GetName());
	CreateIdIfNull();
}


void ABaseItem::CreateIdIfNull()
{
	if (Item.Id == FGuid())
	{
		Item.Id = FGuid().NewGuid();
	}
}


void ABaseItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}




#pragma region Inventory item functions
F_Item ABaseItem::GetItem_Implementation() const				{ return Item; }
const EItemType ABaseItem::GetItemType_Implementation() const	{ return Item.ItemType; }
const FGuid ABaseItem::GetId_Implementation() const				{ return Item.Id; }
const FName ABaseItem::GetItemName_Implementation() const		{ return Item.ItemName; }
void ABaseItem::SetItem_Implementation(const F_Item Data)		{ Item = Data; }
void ABaseItem::SetId_Implementation(const FGuid& Id)			{ Item.Id = Id; }
bool ABaseItem::IsSafeToAdjustItem_Implementation() const { return PendingPlayer == nullptr; }
void ABaseItem::SetPlayerPending_Implementation(ACharacter* Player) { PendingPlayer = Player; }
ACharacter* ABaseItem::GetPlayerPending_Implementation() { return PendingPlayer; }


bool ABaseItem::RetrieveItemFromDataTable(const FName Id, F_Item& ItemData)
{
	if (ItemInformationTable)
	{
		const FString RowContext(TEXT("Item Information Context"));
		if (const F_Table_ItemData* Data = ItemInformationTable->FindRow<F_Table_ItemData>(Id, RowContext))
		{
			ItemData = Data->ItemInformation;
			return true;
		}
		
		UE_LOGFMT(InventoryLog, Error, "{1} Did not find the item to create {2}! {3}()", *GetName(), *Id.ToString(), *FString(__FUNCTION__));
	}

	return false;
}
#pragma endregion 




#pragma region Utility
ACharacter* ABaseItem::GetPlayerPending() const
{
	return PendingPlayer;
}


void ABaseItem::SetPlayerPending(ACharacter* Player)
{
	PendingPlayer = Player;
}
#pragma endregion 

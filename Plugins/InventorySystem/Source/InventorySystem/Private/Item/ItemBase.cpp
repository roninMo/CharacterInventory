// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/ItemBase.h"

#include "Inventory/InventoryComponent.h"
#include "Logging/StructuredLog.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AItemBase::AItemBase()
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


void AItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AItemBase, Item, COND_InitialOrOwner);
}


void AItemBase::InitializeItemGlobals()
{
	// Ex item globals function.
	if (!Item.GlobalInformation) return;
	F_Item GlobalItem;
	
	// const UData_DefaultItemInformation* Information = Cast<UData_CharacterGlobals>(Data);
	// ItemHighlightColor = Information->ItemHighlightColor;
}


void AItemBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeItemGlobals();

	// Handle this during spawn
	// if (ItemInformationTable && !Item.IsValid())
	// {
	// 	RetrieveItemFromDataTable(TableId, Item);
	// }
	
	// CreateIdIfNull();
}


void AItemBase::CreateIdIfNull()
{
	if (Item.Id == FGuid())
	{
		Item.Id = FGuid().NewGuid();
	}
}


void AItemBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}


#pragma region Inventory item functions
F_Item AItemBase::GetItem_Implementation() const				{ return Item; }
const EItemType AItemBase::GetItemType_Implementation() const	{ return Item.ItemType; }
const FGuid AItemBase::GetId_Implementation() const				{ return Item.Id; }
const FName AItemBase::GetItemName_Implementation() const		{ return Item.ItemName; }
void AItemBase::SetItem_Implementation(const F_Item Data)		{ Item = Data; }
void AItemBase::SetId_Implementation(const FGuid& Id)			{ Item.Id = Id; }
bool AItemBase::IsSafeToAdjustItem_Implementation() const { return PendingPlayer == nullptr; }
void AItemBase::SetPlayerPending_Implementation(ACharacter* Player) { PendingPlayer = Player; }
ACharacter* AItemBase::GetPlayerPending_Implementation() { return PendingPlayer; }
void AItemBase::SetItemInformationDatabase_Implementation(UDataTable* Database) { ItemInformationTable = Database; }


bool AItemBase::RetrieveItemFromDataTable(const FName Id, F_Item& ItemData)
{
	if (ItemInformationTable)
	{
		const FString RowContext(TEXT("Item Information Context"));
		if (const FInventory_ItemDatabase* Data = ItemInformationTable->FindRow<FInventory_ItemDatabase>(Id, RowContext))
		{
			ItemData = Data->ItemInformation;
			return true;
		}
		
		UE_LOGFMT(InventoryLog, Error, "{0}() {1} Did not find the item {2} to create!", *FString(__FUNCTION__), *GetName(), Id);
	}

	return false;
}

#pragma endregion 

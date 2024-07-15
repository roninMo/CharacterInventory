// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/InventoryItemInterface.h"

// Add default functionality here for any IInventoryItemInterface functions that are not pure virtual.
F_Item IInventoryItemInterface::GetItem_Implementation() const
{
	return F_Item();
}

const EItemType IInventoryItemInterface::GetItemType_Implementation() const
{
	return EItemType::Inv_None;
}

const FGuid IInventoryItemInterface::GetId_Implementation() const
{
	return FGuid();
}

const FName IInventoryItemInterface::GetItemName_Implementation() const
{
	return NAME_None;
}

void IInventoryItemInterface::SetItem_Implementation(const F_Item Data)
{
}

void IInventoryItemInterface::SetId_Implementation(const FGuid& Id)
{
}

bool IInventoryItemInterface::IsSafeToAdjustItem_Implementation() const
{
	return true;
}

void IInventoryItemInterface::SetPlayerPending_Implementation(ACharacter* Player)
{
}

ACharacter* IInventoryItemInterface::GetPlayerPending_Implementation()
{
	return nullptr;
}

void IInventoryItemInterface::SetItemInformationDatabase_Implementation(UDataTable* Database)
{
}

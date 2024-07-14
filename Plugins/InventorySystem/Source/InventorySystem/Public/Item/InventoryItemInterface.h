// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryInformation.h"
#include "UObject/Interface.h"
#include "InventoryItemInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(Blueprintable)
class UInventoryItemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * An interface for creating inventory items. The base class already handles everything, this is just for customization purposes. This allows for both blueprint and code, with blueprint taking priority
 */
class INVENTORYSYSTEM_API IInventoryItemInterface
{
	GENERATED_BODY()
	
public:
	/** Returns the item's information */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Item") 
	F_Item GetItem() const;
	virtual F_Item GetItem_Implementation() const;
	
	/** Returns the type of item */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Item") 
	const EItemType GetItemType() const;
	virtual const EItemType GetItemType_Implementation() const;
	
	
	/** Returns the id of this instance of an item */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Item") 
	const FGuid GetId() const;
	virtual const FGuid GetId_Implementation() const;
	
	
	/** Returns the database name of this item */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Item") 
	const FName GetItemName() const;
	virtual const FName GetItemName_Implementation() const;


	/** Function for editing the inventory item */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Item") 
	void SetItem(const F_Item ItemData);
	virtual void SetItem_Implementation(const F_Item Data);


	/** Adjusts the unique id for the inventory object version of this item (If not already set) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Item") 
	void SetId(const FGuid& Id);
	virtual void SetId_Implementation(const FGuid& Id);

	/**
	 * Server side function that acts like a threadlock to prevent multiple from trying to perform operations on an item at the same time
	 * @remarks Do not override this unless you're refactoring
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Item|Networking")
	bool IsSafeToAdjustItem() const;
	virtual bool IsSafeToAdjustItem_Implementation() const;
	
	/**
	 * Sets whether a player is already attempting to interact with this item 
	 * @remarks Do not override this unless you're refactoring
	 * @note this should determine whether it's safe to adjust an item (only use on the server)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Item|Networking")
	void SetPlayerPending(ACharacter* Player);
	virtual void SetPlayerPending_Implementation(ACharacter* Player);

	/**
	 * Gets whether a player is already attempting to interact with this item 
	 * @remarks Do not override this unless you're refactoring
	 * @note this should determine whether it's safe to adjust an item (only use on the server)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|Item|Networking")
	ACharacter* GetPlayerPending();
	virtual ACharacter* GetPlayerPending_Implementation();

	
};

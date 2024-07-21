// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemInterface.h"
#include "GameFramework/Actor.h"
#include "ItemBase.generated.h"

UCLASS( Blueprintable, ClassGroup=(Inventory) )
class CHARACTERINVENTORY_API AItemBase : public AActor, public IInventoryItemInterface
{
	GENERATED_BODY()
	
protected:
	/** Information for access this item's data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Data Configuration") UDataTable* ItemInformationTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Data Configuration") UDataAsset* GlobalItemInformation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Data Configuration") FName TableId;
	UPROPERTY(BlueprintReadWrite, Category = "Item|Information") F_Item Item;
	
	/**
	 * Set to true if a player has accessed this item and is performing some action that should prevent other players from doing the same thing
	 * @note Should be used on the server only
	 */
	UPROPERTY(Transient, BlueprintReadWrite) ACharacter* PendingPlayer = nullptr;

	/** Other */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Debugging") bool bDebugItemRetrieval;

	
public:	
	AItemBase();

	/** Function for dynamically adding default information to inventory items */
	virtual void InitializeItemGlobals();
	
//----------------------------------------------------------------------------------------------------------//
// Inventory item interface	(blueprint functions take priority, this just allows customization				//
//----------------------------------------------------------------------------------------------------------//
	/** Returns the item's information */
	virtual F_Item GetItem_Implementation() const override;
	
	/** Returns the type of item */
	virtual const EItemType GetItemType_Implementation() const override;
	
	/** Returns the id of this instance of an item */
	virtual const FGuid GetId_Implementation() const override;
	
	/** Returns the database name of this item */
	virtual const FName GetItemName_Implementation() const override;
	
	/** Function for editing the inventory item */
	virtual void SetItem_Implementation(const F_Item Data) override;
	
	/** Adjusts the unique id for the inventory object version of this item (If not already set) */
	virtual void SetId_Implementation(const FGuid& Id) override;
	
	/**
	 * Server side function that acts like a threadlock to prevent multiple from trying to perform operations on an item at the same time
	 * @remarks Do not override this unless you're refactoring
	 */
	virtual bool IsSafeToAdjustItem_Implementation() const override;
	
	/**
	 * Sets whether a player is already attempting to interact with this item 
	 * @remarks Do not override this unless you're refactoring
	 * @note this should determine whether it's safe to adjust an item (only use on the server)
	 */
	virtual void SetPlayerPending_Implementation(ACharacter* Player) override;
	
	/**
	 * Gets whether a player is already attempting to interact with this item 
	 * @remarks Do not override this unless you're refactoring
	 * @note this should determine whether it's safe to adjust an item (only use on the server)
	 */
	virtual ACharacter* GetPlayerPending_Implementation() override;
	
	
//----------------------------------------------------------------------------------------------------------//
// Utility																									//
//----------------------------------------------------------------------------------------------------------//
	/** Adds the item information database */
	virtual void SetItemInformationDatabase_Implementation(UDataTable* Database) override;
	
	/** Retrieves an item from the data table. Returns null if the item was not found */
	UFUNCTION(BlueprintCallable) virtual bool RetrieveItemFromDataTable(FName Id, F_Item& ItemData);
	
	
protected:	
	virtual void BeginPlay() override;
	virtual void CreateIdIfNull();
	virtual void Tick(float DeltaSeconds) override;
	
	
};

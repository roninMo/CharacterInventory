#pragma once


#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "InventoryInformation.generated.h"

class IInventoryItemInterface;
class UItemGlobals;
class ABaseItem;


/**
 *	The specific type of item
 */
UENUM(BlueprintType)
enum class EItemType : uint8
{
	Inv_Item		                    UMETA(DisplayName = "Item"),
	Inv_Armor		                    UMETA(DisplayName = "Armor"),
	Inv_Note							UMETA(DisplayName = "Note"),
	Inv_QuestItem		                UMETA(DisplayName = "Quest Item"),
	Inv_Weapon							UMETA(DisplayName = "Weapon"),
	Inv_Material						UMETA(DisplayName = "Material"),
	Inv_Custom							UMETA(DisplayName = "Custom"),
	Inv_None                            UMETA(DisplayName = "None"),
	Inv_MAX
};




/**
 *	The operation that's being done on the inventory(s)
 */
UENUM(BlueprintType)
enum class EInventoryOperation : uint8
{
	/* Player adding or editing an inventory without interacting with another inventory component */
	OP_IndividualInventory		            UMETA(DisplayName = "Same Inventory Edit"),
	
	/* Storing an item in a container */
	OP_InventoryToContainer				UMETA(DisplayName = "Storing Inventory in Container Edit"),

	/* Taking an item from a container and adding it to the player's inventory */
	OP_ContainerToInventory             UMETA(DisplayName = "Taking Container Inventory Edit"),

	/* Multiple inventories editing each other */
	OP_InventoryToInventory             UMETA(DisplayName = "Multiple Player Inventories Edit"),
	
	OP_None                             UMETA(DisplayName = "None")
};




/**
 *	 Information specific to an item for displaying in the inventory and spawning them in the world
 *	 Also contains the information to access and construct the specific item that the character has taken
 */
USTRUCT(BlueprintType)
struct F_Item
{
	GENERATED_USTRUCT_BODY()
		F_Item(
            FGuid Id = FGuid(),
            int32 SortOrder = -1,
			const FName ItemName = "",
			const FString& DisplayName = "",
            
			const FString& Description = "",
			const FString& InteractText = "",
			UTexture2D* Image = nullptr,
			const EItemType ItemType = EItemType::Inv_None,

			const TSubclassOf<UObject> ActualClass = nullptr,
			const TSubclassOf<AActor> WorldClass = nullptr,
			UDataAsset* GlobalInformation = nullptr
		) :
	
		Id(Id),
		SortOrder(SortOrder),
		ItemName(ItemName),
		DisplayName(DisplayName),
	
		Description(Description),
		InteractText(InteractText),
		ItemType(ItemType),
		Image(Image),
	
		ActualClass(ActualClass),
		WorldClass(WorldClass),
		GlobalInformation(GlobalInformation)
	{}

public:
	virtual ~F_Item() {}
	
	/** The unique id for this item. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite) FGuid Id;
	
	/** The sort order for the inventory item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int SortOrder;

	/** The database name reference of the item. This is used for retrieving the item from the database */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemName;

	/** The display name of the item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString InteractText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EItemType ItemType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) UTexture2D* Image;

	/**
	 * The actual class of this item. This could be from a weapon to an activatable item, and it's information is mapped through the item type.
	 * @remarks This should reference the inventory interface or use the ItemBase class
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<UObject> ActualClass;
	
	/**
	 * The class of this item that's spawned in the world that the character interacts with.
	 * @remarks This should reference the inventory interface or use the ItemBase class
	 * 
	 */	
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AActor> WorldClass;
	
	/** Global data for items that's added to the object from the blueprint */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) UDataAsset* GlobalInformation;

	
	/** Convenience function to access the item type without creating another value */
	virtual EItemType GetItemType() const
	{
		return this->ItemType;
	}

	/** Convenience function to access the id without creating another value */
	virtual FGuid GetId() const
	{
		return this->Id;
	}

	/** Convenience function to access the database item id without creating another value */
	virtual FName GetDatabaseId() const 
	{
		return this->ItemName;
	}

	/** Is this a valid item? */
	virtual bool IsValid() const
	{
		return this->Id.IsValid() && this->GetDatabaseId().IsValid();
	}
};



/**
 * Global item information for customizing different items with default information. This class is just a reference, either way you could use this for initializing things for items in general
 */
UCLASS()
class INVENTORYSYSTEM_API UItemGlobals : public UDataAsset
{
	GENERATED_BODY()
	
public:

	
};




/**
 * This is the data table to hold all the item information for the game.
 * All objects derive from this information
 * @note This is the raw item information, everything in the game has been configured to use @ref F_InventoryItem because it holds information unique to that instance
 * @ref I would add the unique information using another object that's linked to the item, for performance reasons. Otherwise you'll have to refactor networking code and that impacts performance 
 */
USTRUCT(BlueprintType)
struct FInventory_ItemDatabase : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) F_Item ItemInformation;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DevDescription;
};




/**
 * The raw information passed to the server for capturing and saving inventory information
 */
USTRUCT(BlueprintType)
struct FS_Item
{
	GENERATED_USTRUCT_BODY()
		FS_Item(
			const FGuid& Id = FGuid(),
			const FName& ItemName = FName(),
			const int32 SortOrder = -1
			// Just divide the unique information pertaining to individual things like weapons to their own objects that contain levels and other values, and save that information alongside the inventory information
		) :
		Id(Id),
		ItemName(ItemName),
		SortOrder(SortOrder)
	{}

	virtual bool IsValid() const
	{
		return !this->ItemName.IsNone();
	}
	

public:
	virtual ~FS_Item() {}
	UPROPERTY(BlueprintReadWrite) FGuid Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SortOrder;
};






/**
 * The character's saved inventory information
 */
USTRUCT(BlueprintType)
struct F_InventorySaveInformation
{
	GENERATED_USTRUCT_BODY()
		F_InventorySaveInformation(
			const int32 NetId = 0,
			const FString& PlatformId = FString(),
			const TArray<FS_Item>& InventoryItems = {}
		) :
	
		NetId(NetId),
		PlatformId(PlatformId),
		InventoryItems(InventoryItems)
	{}

public:
	virtual ~F_InventorySaveInformation() {}
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NetId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlatformId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FS_Item> InventoryItems;
};






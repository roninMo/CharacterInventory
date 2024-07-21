// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryInformation.h"
#include "GameFramework/SaveGame.h"
#include "InventorySaveGameObject.generated.h"

/**
 * 
 */
UCLASS()
class INVENTORYSYSTEM_API UInventorySaveGameObject : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory") F_InventorySaveInformation InventorySaveInformation;
	
	
};

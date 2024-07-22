# Overview

The `Camera System` helps with logic for handling smooth transitions between different camera behaviors, and has built in logic for first, third, and target lock logic with smooth movement and easy to implement customization. 

An `Inventory System` for player's for storing and retrieving different inventory items in multiplayer with error handling in a safe and efficient way that even allows for customization, and works out of the box. All you need to do is add the component to the character, and store and retrieve values from it. There's also logic for saving information, just search through the function list in the blueprint.

![InventorySystem_Image](https://github.com/user-attachments/assets/7ba2ef76-e5fa-4cda-ba7e-5e3b0a9a726f)

`Hopefully this saves you time and effort while you're developing things`




<br><br/>
# Setup
Add the `Inventory` to the character's components, and everything's ready to go. You'll also need to create a data table to store the item information, and add that reference to the inventory component. The tutorial teaches you how to handle this, and there's also an example in blueprints with reference to the primary functions

![InventoryTutorial_7](https://github.com/user-attachments/assets/30d2b933-bfc9-485e-8a50-effeedb19544)



<br><br/>
# Tutorial 


 - Add the item data table
 - Inventory Items Interface and base class
 - 


<br><br/>
## Create a character
Begin by creating a character! (Everything in the inventory is handled in a component, we just need a character)

![InventoryTutorial_0](https://github.com/user-attachments/assets/e0c0a113-20d5-4cad-8e19-44682ec77d2d)




<br><br/>
## Add the Inventory Component
Add the inventory component by searching for `Inventory`, and add it to the character.

![InventoryTutorial_1](https://github.com/user-attachments/assets/1ffa812b-23fc-4855-9f36-e6c167bce5b5)




<br><br />
## Inventory Function List
There's a lot of functions on the inventory component, for storing/retrieving, networking, and error handling things so I'll just reference the functions you'll use, and then a list of what everything does after. I don't advise customizing things unless you know what you're doing, either way, here we go

![InventoryTutorial_2](https://github.com/user-attachments/assets/e21ea21a-f6cf-40d5-8477-a2bca7b8ffc5)




<br><br />
### Primary Functions
For storing and retrieving items, there's `GetItem()`, `TryAddItem`, `TryRemoveItem()`, `TryTransferItem()`. These all can be called from the client or the server, and are backwards compatible and have error handling that you can add your Hud and other things to for handling inventory adjustments.

![InventoryTutorial_4](https://github.com/user-attachments/assets/7d07e51a-d8e7-4a31-b772-95db81883f84)

Here's a list of the callback functions for storing and retrieving inventory items. These are for handling the different scenarios for inventory edits

![InventoryTutorial_3](https://github.com/user-attachments/assets/b0981f2f-9e10-4aed-846d-97fc167b6c9a)


#### GetItem()
`GetItem()` or `Get Item From Inventory` returns an item from the inventory. You just need to pass it the id of the item, and it retrieves it for you. If you add the item category it speeds up the search, and if it doesn't find anything it just returns null

#### TryAddItem()
`TryAddItem()` adds an item to the player's inventory. If it fails, `On Inventory Item Addition Failed` is invoked, otherwise `On Inventory Item Addition Success` and that helps with handling things like updating the hud. There's also `PendingAddItemLogic`, which helps with hiding the item until the server actually stores the item.

#### TryRemoveItem()
`TryAddItem()` removes an item from the player's inventory. If dropItem is true, it also spawns the item. If it fails, `On Inventory Item Removal Failed` is invoked, otherwise `On Inventory Item  Success` and that helps with handling things like updating the hud. There's also `PendingRemoveItemLogic`, which helps with updating the hud momentarily to make networking seamless. If you want to use your own Item class or logic for handling spawning items from the inventory, customize the InventoryComponent's `SpawnItem()` function.

#### TryTransferItem()
`TryTransferItem()` transfers an item from one inventory to another. Just give it the item id and the other inventory, and it handles everything else (including handling both to and from scenarios). If it fails, `On Inventory Item Transfer Failure` is invoked, otherwise `On Inventory Item Transfer Success` and that helps with error handling and other things.


### Customization
If you want to edit any of these functions (I don't advise this everything already works perfectly), search through the Inventory Operations (And the code) to adjust things. Customizing the `InventoryComponent` is tough because there's remote procedurce calls in code, however all of the actual logic for inventory edits is with the `Handle` functions. `GetInventoryList` returns the different containers for inventory if you want to add additional inventory, and if you want to edit the inventory object, `CreateInventoryObject` (This is how you should also create an inventory object) is used to create inventory objects.


### Values and Function List
#### Values
| CameraStyle 							| Description						|
| ---									                         | -----------						|
| CameraStyle							                   | The current style of the camera that determines the behavior. The default styles are `Fixed`, `Spectator`, `FirstPerson`, `ThirdPerson`, `TargetLocking`, and `Aiming`. You can also add your own in the BasePlayerCameraManager class |
| CameraOrientation 					              | These are based on the client, but need to be replicated for late joining clients, so we're using both RPC's and replication to achieve this |
| CameraOrientation Transition Speed 	 | The camera orientation transition speed |
| Target Lock Transition Speed 			     | Controls how quickly the camera transitions between targets. `ACharacterCameraLogic`'s **TargetLockTransitionSpeed** value adjusts this |
| Camera Offset First Person 			       | The first person camera's location |
| Camera Offset Center 					           | The third person camera's default location |
| Camera Offset Left 					             | The third person camera's left side location |
| Camera Offset Right 					            | The third person camera's right side location |
| Camera Lag 							                   | The camera lag of the arm. @remarks This overrides the value of the camera arm's lag |
| Target Arm Length 					              | The target arm length of the camera arm. @remarks This overrides the value of the camera arm's target arm length |
| Input Pressed Replication Interval 	 | The interval for when the player is allowed to transition between camera styles. This is used for network purposes |
	
#### Functions
| CameraStyle 							| Description						|
| ---									                         | -----------						|
| CameraStyle							                   | The current style of the camera that determines the behavior. The default styles are `Fixed`, `Spectator`, `FirstPerson`, `ThirdPerson`, `TargetLocking`, and `Aiming`. You can also add your own in the BasePlayerCameraManager class |
| CameraOrientation 					              | These are based on the client, but need to be replicated for late joining clients, so we're using both RPC's and replication to achieve this |
| CameraOrientation Transition Speed 	 | The camera orientation transition speed |
| Target Lock Transition Speed 			     | Controls how quickly the camera transitions between targets. `ACharacterCameraLogic`'s **TargetLockTransitionSpeed** value adjusts this |
| Camera Offset First Person 			       | The first person camera's location |
| Camera Offset Center 					           | The third person camera's default location |
| Camera Offset Left 					             | The third person camera's left side location |
| Camera Offset Right 					            | The third person camera's right side location |
| Camera Lag 							                   | The camera lag of the arm. @remarks This overrides the value of the camera arm's lag |
| Target Arm Length 					              | The target arm length of the camera arm. @remarks This overrides the value of the camera arm's target arm length |
| Input Pressed Replication Interval 	 | The interval for when the player is allowed to transition between camera styles. This is used for network purposes |
	


<br><br />
## Inventory Items
Inventory items are a reference to an item's information, with customization and efficiency in mind to help with building the inventory. The inventory component retrieves it's information from the inventory item database, and that has information for the inventory hud, and references to the inventory item's components. There's examples of how to create everything 
already, this is just for reference

![InventoryTutorial_5](https://github.com/user-attachments/assets/3b78cdd4-f91a-480d-929c-84edc1473cde)


### Creating an Inventory Item
The inventory component uses the `IInventoryItemInterface` to determine whether an item is valid, and this project comes with the ItemBase class that it uses for spawning items. It has built in logic for inventory item information and logic, and I'd just use that instead of recreating different logic. The only things that you need to do when you create an inventory item from the `ItemBase` class is add a reference to the Inventory Items database, add it's id, and call the `RetrieveItemFromDataTable()` during BeginPlay. That way if you adjust anything you don't have to handle every instance, and just the original logic

![InventoryTutorial_6](https://github.com/user-attachments/assets/2bcd3124-dad1-4d70-9dd9-82a4f163c346)




## Happy Coding


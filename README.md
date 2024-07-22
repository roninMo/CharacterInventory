# Overview

The `Camera System` helps with logic for handling smooth transitions between different camera behaviors, and has built in logic for first, third, and target lock logic with smooth movement and easy to implement customization. 

An `Inventory System` for player's for storing and retrieving different inventory items in multiplayer with error handling in a safe and efficient way that even allows for customization, and works out of the box. All you need to do is add the component to the character, and store and retrieve values from it. There's also logic for saving information, just search through the function list in the blueprint.

![InventorySystem_Image](/images/InventorySystem_Image.png)

`Hopefully this saves you time and effort while you're developing things`




<br><br/>
# Setup
Add the `Inventory` to the character's components, and everything's ready to go. You'll also need to create a data table to store the item information, and add that reference to the inventory component. The tutorial teaches you how to handle this, and there's also an example in blueprints with reference to the primary functions

![InventorySystem_Image](/images/InventoryTutorial_7.png)




<br><br/>
# Tutorial 


 - Add the item data table
 - Inventory Items Interface and base class
 - 


<br><br/>
## Create a character
Begin by creating a character! (Everything in the inventory is handled in a component, we just need a character)

![InventorySystem_Image](/images/InventoryTutorial_0.png)




<br><br/>
## Add the Inventory Component
Add the inventory component by searching for `Inventory`, and add it to the character.

![InventorySystem_Image](/images/InventoryTutorial_1.png)




<br><br />
## Inventory Function List
There's a lot of functions on the inventory component, for storing/retrieving, networking, and error handling things so I'll just reference the functions you'll use, and then a list of what everything does after. I don't advise customizing things unless you know what you're doing, either way, here we go

![InventorySystem_Image](/images/InventoryTutorial_2.png)




<br><br />
### Primary Functions
For storing and retrieving items, there's `GetItem()`, `TryAddItem`, `TryRemoveItem()`, `TryTransferItem()`. These all can be called from the client or the server, and are backwards compatible and have error handling that you can add your Hud and other things to for handling inventory adjustments.

![InventorySystem_Image](/images/InventoryTutorial_4.png)

Here's a list of the callback functions for storing and retrieving inventory items. These are for handling the different scenarios for inventory edits

![InventorySystem_Image](/images/InventoryTutorial_3.png)


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
Here's a list of the `InventoryComponent`'s values and functions

#### Values

| Values 							| Description						|
| ---								| -----------						|
| Quest Items							| Inventory Container for quest items |
| Common Items							| Inventory Container for items |
| Weapons							| Inventory Container for weapons |
| Armors							| Inventory Container for armors |
| Materials							| Inventory Container for materials |
| Notes								| Inventory Container for notes |
| Item Database							| A reference to the item data table that contains the items for your game |
| NetId								| The client's Net Id (This is unique for every client) |
| Platform Id							| The machines Platforms Id (This is unique for every machine) |
| Character							| A reference to the character |
| Debug Save Information					| Debugs loading and saving the inventory information |
| Debug Inventory (Client)					| Debugs inventory during client logic |
| Debug Inventory (Server)					| Debugs inventory during server logic |


#### Functions
There's too many functions to talk about, just search through the list and reference the code for understanding things. For the primary functions, there's generally the intial function logic, and then a `Handle` function for handling the actual logic, `Pending` for the client logic during the action, and delegates for the different scenarios for safeguarding and error handling. If you want to custmoize logic, you just need to update the handle and utility functions so that it creates and retrieves the information without any errors. Either way everything is already set so there's no need for customization unless you're adjusting things! 




<br><br />
## Inventory Items
Inventory items are a reference to an item's information, with customization and efficiency in mind to help with building the inventory. The inventory component retrieves it's information from the inventory item database, and that has information for the inventory hud, and references to the inventory item's components. There's examples of how to create everything 
already, this is just for reference

![InventorySystem_Image](/images/InventoryTutorial_5.png)


### Creating an Inventory Item
The inventory component uses the `IInventoryItemInterface` to determine whether an item is valid, and this project comes with the ItemBase class that it uses for spawning items. It has built in logic for inventory item information and logic, and I'd just use that instead of recreating different logic. The only things that you need to do when you create an inventory item from the `ItemBase` class is add a reference to the Inventory Items database, add it's id, and call the `RetrieveItemFromDataTable()` during BeginPlay. That way if you adjust anything you don't have to handle every instance, and just the original logic

![InventorySystem_Image](/images/InventoryTutorial_6.png)





## Happy Coding


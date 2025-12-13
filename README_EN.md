# mod-treasure  

### 🇨🇿 [Czech version](README_CS.md)

## Description (EN)
This module allows you to:  
- Create 3 types of treasure chests (basic, rare, epic)  
- Configure their respawn time and language (Czech/English)  
- Add custom items to the loot with chance and quantity range (min-max)  
- Manage chests with in-game commands  

### Installation / Requirements
The module includes an autoupdater, so there’s no need to manually import any .sql files.  
For the autoupdater to function correctly, it is necessary to ensure that the database user from `(WorldDatabaseInfo) – "127.0.0.1;3306;acore;acore;acore_world"`  
has permissions for the new `customs` database as well:

```
GRANT CREATE ON *.* TO 'acore'@'127.0.0.1';
GRANT ALL PRIVILEGES ON customs.* TO 'acore'@'127.0.0.1';
FLUSH PRIVILEGES;
```  

**Optional:**
- Add this line to worldserver.conf:  
  Logger.gv.customs=3,Console Server

##

### ⚠️ Warning – Reserved IDs
The module uses custom **entry** and **loot entry** in the `gameobject_template` and `gameobject_loot_template` tables.  
Make sure these IDs are not already used by other content in `acore_world`:

- **Czech chest variants (`gameobject_template.entry`):**
  - `990200` – Prostá truhla s pokladem
  - `990201` – Vzácná truhla s pokladem
  - `990202` – Epická truhla s pokladem

- **English chest variants (`gameobject_template.entry`):**
  - `990210` – Plain Treasure Chest
  - `990211` – Rare Treasure Chest
  - `990212` – Epic Treasure Chest

- **Loot entries (`gameobject_loot_template.Entry`):**
  - `990200` – Basic loot
  - `990201` – Rare loot
  - `990202` – Epic loot

If your database already contains records with these IDs, adjust the numbers in the module and SQL to another free range.

**Notes:**
- After the **first chest addition**, a **server restart** is required for them to appear in the world. Any further additions will be visible immediately.

- After **adding or editing loot** in a chest, a **server restart** is also required for the new content to start appearing.

### Commands
.treasure add basic
➝ Creates a plain chest (basic) at the player’s current location
Example: .treasure add basic

.treasure add rare
➝ Creates a rare chest (rare) at the player’s current location
Example: .treasure add rare

.treasure add epic
➝ Creates an epic chest (epic) at the player’s current location
Example: .treasure add epic

.treasure list basic/rare/epic
➝ Lists all chests of given quality from the customs database
Example: .treasure list basic

.treasure remove <ID>
➝ Removes a chest by ID from customs and gameobject
Example: .treasure remove 5

.treasure additem <itemId> <min-max/count> <chance> <basic/rare/epic>
➝ Adds an item to the loot of given quality with specified range and chance
Example (range): .treasure additem 17 1-5 0.5 basic
Example (fixed count): .treasure additem 18 3 100 epic

.treasure tp <ID>
➝ Teleports player to the chest with given ID from customs
Example: .treasure tp 7



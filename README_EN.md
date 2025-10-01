# mod-treasure  

### 🇨🇿 [Česká verze](README_CS.md)

## Description (EN)
- This module allows you to:

- Create 3 types of treasure chests (basic, rare, epic)

- Configure their respawn time and language (Czech/English)

- Add custom items to the loot with chance and quantity range (min-max)

- Manage chests with in-game commands

### Requirements
Before use, ensure that the database user from WorldDatabaseInfo (default acore) also has access to the new customs schema:

```sql
GRANT ALL PRIVILEGES ON customs.* TO 'acore'@'localhost';
FLUSH PRIVILEGES;
```

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
- On startup, the module **automatically creates any missing** records in `gameobject_loot_template` (a placeholder or based on `Treasure.DefaultItem.*` in `.conf`).  
  It **does not overwrite** existing loot (it uses *INSERT IGNORE*).  
  A separate SQL seed for loot is **not required**; if you still include one, keep it **idempotent** (no `DELETE`; use `INSERT IGNORE` / `ON DUPLICATE KEY UPDATE`).

- After the **first chest addition**, a **server restart** is required for them to appear in the world. Any further additions will be visible immediately.

- After **adding or editing loot** in a chest, a **server restart** is also required for the new content to start appearing.

- Optional SQL `sql/optional/world/base/sack_loot_edit.sql` **modifies the loot of the original items**  
  **Small Sack of Coins (11966)** and **Fat Sack of Coins (11937)** so that they no longer drop from regular coffers  
  and become exclusive to the treasure chests created by this module.

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

## License
This module is licensed under the [GNU General Public License v2.0 (GPL-2.0)](LICENSE).


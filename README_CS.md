# mod-treasure  

### 🇬🇧 [English version](README_EN.md)

## Popis (CZ)  
Tento modul umožňuje:  
- Vytvářet 3 druhy truhel s pokladem (`basic`, `rare`, `epic`)  
- Nastavit jejich respawn čas a jazyk (čeština/angličtina)  
- Přidávat vlastní itemy do obsahu truhel s šancí a rozsahem množství (`min-max`)  
- Spravovat truhly příkazy přímo ve hře  

### Požadavky  
Před použitím je nutné zajistit, aby uživatel databáze z `WorldDatabaseInfo` (standardně `acore`) měl práva i na novou databázi `customs`:  

```sql
GRANT ALL PRIVILEGES ON customs.* TO 'acore'@'localhost';
FLUSH PRIVILEGES;
```

### ⚠️ Upozornění
Modul používá vlastní **entry** a **loot entry** v tabulkách `gameobject_template` a `gameobject_loot_template`.  
Je nutné zajistit, že tato ID nejsou v `acore_world` již obsazená jiným obsahem:

- **České verze truhel (`gameobject_template.entry`):**
  - `990200` – Prostá truhla s pokladem
  - `990201` – Vzácná truhla s pokladem
  - `990202` – Epická truhla s pokladem

- **Anglické verze truhel (`gameobject_template.entry`):**
  - `990210` – Plain Treasure Chest
  - `990211` – Rare Treasure Chest
  - `990212` – Epic Treasure Chest

- **Loot entry (`gameobject_loot_template.Entry`):**
  - `990200` – Basic loot
  - `990201` – Rare loot
  - `990202` – Epic loot

Pokud máš v databázi již jiné záznamy s těmito ID, je potřeba čísla v modulu i v SQL posunout na jiný volný rozsah.

**Poznámky:**
- Modul při startu **automaticky založí chybějící záznamy** v `gameobject_loot_template` (placeholder nebo podle `Treasure.DefaultItem.*` v `.conf`).  
  Existující loot **nepřepisuje** (používá vkládání typu *INSERT IGNORE*).  
  Samostatný SQL seed pro loot **není nutný**; pokud ho přesto přidáš, dělej to **idempotentně** (bez `DELETE`, s `INSERT IGNORE` / `ON DUPLICATE KEY UPDATE`).

- Po **prvním přidání truhel** je nutné provést **restart serveru**, aby se zobrazily ve světě. Následná přidávání už se projeví okamžitě.

- Po **přidání nebo úpravě lootu** v bedně je také potřeba **restart serveru**, aby se nový obsah začal objevovat.

- Volitelné SQL `sql/optional/world/base/sack_loot_edit.sql` **upravuje loot původních itemů**  
  **Small Sack of Coins (11966)** a **Fat Sack of Coins (11937)** tak, aby přestaly padat z klasických truhel  
  a byly exkluzivním obsahem truhel vytvořených tímto modulem.

### Příkazy
.treasure add basic
➝ Vytvoří prostou truhlu (basic) v aktuálním místě hráče
Příklad: .treasure add basic

.treasure add rare
➝ Vytvoří vzácnou truhlu (rare) v aktuálním místě hráče
Příklad: .treasure add rare

.treasure add epic
➝ Vytvoří epickou truhlu (epic) v aktuálním místě hráče
Příklad: .treasure add epic

.treasure list basic/rare/epic
➝ Vypíše seznam všech truhel dané kvality z databáze customs
Příklad: .treasure list basic

.treasure remove <ID>
➝ Odstraní truhlu podle ID z databáze customs i gameobject
Příklad: .treasure remove 5

.treasure additem <itemId> <min-max/count> <chance> <basic/rare/epic>
➝ Přidá item do lootu dané kvality s daným rozsahem a šancí
Příklad (rozsah): .treasure additem 17 1-5 0.5 basic
Příklad (pevný počet): .treasure additem 18 3 100 epic

.treasure tp <ID>
➝ Teleportuje hráče k truhle podle ID z customs
Příklad: .treasure tp 7

## License
This module is licensed under the [GNU General Public License v2.0 (GPL-2.0)](LICENSE).


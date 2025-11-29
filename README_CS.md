# mod-treasure  

### 🇬🇧 [English version](README_EN.md)

## Popis (CZ)  
Tento modul umožňuje:  
- Vytvářet 3 druhy truhel s pokladem (`basic`, `rare`, `epic`)  
- Nastavit jejich respawn čas a jazyk (čeština/angličtina)  
- Přidávat vlastní itemy do obsahu truhel s šancí a rozsahem množství (`min-max`)  
- Spravovat truhly příkazy přímo ve hře  

### Instalace / Požadavky  
Modul obsahuje autoupdater tudíž není potřeba ručně importovat .sql  
Pro správnou funkčnost autoupdateru je nutné zajistit, aby uživatel databáze z `(WorldDatabaseInfo) – "127.0.0.1;3306;acore;acore;acore_world"`  
měl práva i na novou databázi customs:

```
GRANT CREATE ON *.* TO 'acore'@'127.0.0.1';
GRANT ALL PRIVILEGES ON customs.* TO 'acore'@'127.0.0.1';
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
- Po **prvním přidání truhel** je nutné provést **restart serveru**, aby se zobrazily ve světě. Následná přidávání už se projeví okamžitě.

- Po **přidání nebo úpravě lootu** v bedně je také potřeba **restart serveru**, aby se nový obsah začal objevovat.

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



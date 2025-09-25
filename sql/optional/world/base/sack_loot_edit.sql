-- =========================================================
-- Adjust Small Sack of Coins (item 11966) and
--        Fat   Sack of Coins (item 11937)
-- =========================================================

START TRANSACTION;

-- -------------------------
-- Small Sack of Coins (11966)
-- -------------------------

-- 1) Disable obtaining the Small Sack itself from old sources
UPDATE `gameobject_loot_template`
SET `Chance` = 0
WHERE `Item` = 11966
  AND `Entry` IN (11103, 11105, 11160);

-- 2) Set chance of all item drops INSIDE the Small Sack to 0
UPDATE `item_loot_template`
SET `Chance` = 0
WHERE `Entry` = 11966;

-- 3) Money range: 50s .. 5g  (5000 .. 50000 copper)
UPDATE `item_template`
SET `minmoneyloot` =  5000,
    `maxmoneyloot` = 50000
WHERE `entry` = 11966;


-- -------------------------
-- Fat Sack of Coins (11937)
-- -------------------------

-- 1) Disable obtaining the Fat Sack itself from old sources
UPDATE `gameobject_loot_template`
SET `Chance` = 0
WHERE `Item` = 11937
  AND `Entry` IN (11103, 11105);

-- 2) Set chance of all item drops INSIDE the Fat Sack to 0
UPDATE `item_loot_template`
SET `Chance` = 0
WHERE `Entry` = 11937;

-- 3) Money range: 5g .. 30g  (50 000 .. 300 000 copper)
UPDATE `item_template`
SET `minmoneyloot` =  50000,
    `maxmoneyloot` = 300000
WHERE `entry` = 11937;

COMMIT;

-- ENTRIES: CS (990200-2) + EN (990210-12)
DELETE FROM `gameobject_template` WHERE `entry` IN (990200,990201,990202,990210,990211,990212);

-- CS
INSERT INTO `gameobject_template`
(`entry`,`type`,`displayId`,`name`,`size`,`Data0`,`Data1`,`Data3`,`Data10`,`Data12`,`Data15`,`ScriptName`)
VALUES
(990200,3,259,'Prostá truhla s pokladem',1,43,990200,1,1,1,1,'TreasureChestPersistGO'),
(990201,3,259,'Vzácná truhla s pokladem',1,43,990201,1,1,1,1,'TreasureChestPersistGO'),
(990202,3,259,'Epická truhla s pokladem',1,43,990202,1,1,1,1,'TreasureChestPersistGO');

-- EN
INSERT INTO `gameobject_template`
(`entry`,`type`,`displayId`,`name`,`size`,`Data0`,`Data1`,`Data3`,`Data10`,`Data12`,`Data15`,`ScriptName`)
VALUES
(990210,3,259,'Plain Treasure Chest',1,43,990200,1,1,1,1,'TreasureChestPersistGO'),
(990211,3,259,'Rare Treasure Chest',1,43,990201,1,1,1,1,'TreasureChestPersistGO'),
(990212,3,259,'Epic Treasure Chest',1,43,990202,1,1,1,1,'TreasureChestPersistGO');

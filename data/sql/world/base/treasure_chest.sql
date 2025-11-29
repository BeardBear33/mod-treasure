-- CS bedny
INSERT INTO `gameobject_template`
(`entry`,`type`,`displayId`,`name`,`size`,`Data0`,`Data1`,`Data3`,`Data10`,`Data12`,`Data15`,`ScriptName`)
VALUES
(990200,3,259,'Prostá truhla s pokladem',1,43,990200,1,1,1,1,'TreasureChestPersistGO'),
(990201,3,259,'Vzácná truhla s pokladem',1,43,990201,1,1,1,1,'TreasureChestPersistGO'),
(990202,3,259,'Epická truhla s pokladem',1,43,990202,1,1,1,1,'TreasureChestPersistGO')
ON DUPLICATE KEY UPDATE
    `type`       = VALUES(`type`),
    `displayId`  = VALUES(`displayId`),
    `name`       = VALUES(`name`),
    `size`       = VALUES(`size`),
    `Data0`      = VALUES(`Data0`),
    `Data1`      = VALUES(`Data1`),
    `Data3`      = VALUES(`Data3`),
    `Data10`     = VALUES(`Data10`),
    `Data12`     = VALUES(`Data12`),
    `Data15`     = VALUES(`Data15`),
    `ScriptName` = VALUES(`ScriptName`);

-- EN chests
INSERT INTO `gameobject_template`
(`entry`,`type`,`displayId`,`name`,`size`,`Data0`,`Data1`,`Data3`,`Data10`,`Data12`,`Data15`,`ScriptName`)
VALUES
(990210,3,259,'Plain Treasure Chest',1,43,990200,1,1,1,1,'TreasureChestPersistGO'),
(990211,3,259,'Rare Treasure Chest',1,43,990201,1,1,1,1,'TreasureChestPersistGO'),
(990212,3,259,'Epic Treasure Chest',1,43,990202,1,1,1,1,'TreasureChestPersistGO')
ON DUPLICATE KEY UPDATE
    `type`       = VALUES(`type`),
    `displayId`  = VALUES(`displayId`),
    `name`       = VALUES(`name`),
    `size`       = VALUES(`size`),
    `Data0`      = VALUES(`Data0`),
    `Data1`      = VALUES(`Data1`),
    `Data3`      = VALUES(`Data3`),
    `Data10`     = VALUES(`Data10`),
    `Data12`     = VALUES(`Data12`),
    `Data15`     = VALUES(`Data15`),
    `ScriptName` = VALUES(`ScriptName`);

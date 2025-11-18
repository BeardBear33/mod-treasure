-- customs.treasure_chest – evidence našich truhel (ID pro příkazy)
CREATE DATABASE IF NOT EXISTS `customs` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `customs`.`treasure_chest` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `quality` TINYINT UNSIGNED NOT NULL,       -- 1=Basic, 2=Rare, 3=Epic
  `guid` INT UNSIGNED NOT NULL,              -- vazba na gameobject.guid
  `map` SMALLINT UNSIGNED NOT NULL,
  `zoneId` SMALLINT UNSIGNED NOT NULL,
  `areaId` SMALLINT UNSIGNED NOT NULL,
  `position_x` FLOAT NOT NULL,
  `position_y` FLOAT NOT NULL,
  `position_z` FLOAT NOT NULL,
  `orientation` FLOAT NOT NULL,
  `respawnSecs` INT NOT NULL,
  `created_by` INT UNSIGNED DEFAULT NULL,    -- accountId
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


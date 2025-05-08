ALTER TABLE `creature_template` ADD COLUMN `corpse_delay` INT UNSIGNED NOT NULL DEFAULT 300 AFTER `ScriptName`;

UPDATE `creature_template` SET `corpse_delay` = 900 WHERE `rank` = 1;
UPDATE `creature_template` SET `corpse_delay` = 1200 WHERE `rank` = 2;
UPDATE `creature_template` SET `corpse_delay` = 3600 WHERE `rank` = 3;
UPDATE `creature_template` SET `corpse_delay` = 900 WHERE `rank` = 4;
UPDATE `creature_template` SET `corpse_delay` = 60 WHERE `rank` = 5;
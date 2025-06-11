DROP TABLE IF EXISTS `on_login_commands`;
CREATE TABLE `on_login_commands` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `player_guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier of login player',
  `command` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Command Text',
  `created_at` int unsigned NOT NULL DEFAULT '0',
  `updated_at` int unsigned NOT NULL DEFAULT '0',
  `deleted_at` int unsigned DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_deleted_at` (`deleted_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
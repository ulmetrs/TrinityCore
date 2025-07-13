DELETE FROM `command`
WHERE `name` IN ('onlogin');

INSERT INTO `command` (`name`, `help`) VALUES
('onlogin', 'Syntax: .onlogin [$playername] [$targetcommand]\r\nStores a command to be executed when the player logs in.');
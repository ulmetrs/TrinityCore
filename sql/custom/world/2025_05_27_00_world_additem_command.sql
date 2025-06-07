DELETE FROM `command`
WHERE `name` IN ('additem');

INSERT INTO `command` (`name`, `help`) VALUES
('additem', 'Syntax: .additem [$playername] #itemid/[#itemname]/#shift-click-item-link [#itemcount]\r\n\r\nAdds the specified number of items of id #itemid (or exact (!) name [#itemname] in brackets, or link created by #shift-click-item-link) to the inventory of $playername (if provided), otherwise to the selected character''s inventory, or to your own inventory if no character is selected and no $playername is given. If [#itemcount] is omitted, only one item will be added. A negative value for [#itemcount] will attempt to remove items.');
/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
Name: onlogin_commandscript
%Complete: 100
Comment: All onlogin related commands
Category: commandscripts
EndScriptData */

#include "CharacterCache.h"
#include "Chat.h"
#include "Language.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "OnLoginCmdMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include <unordered_map>
#include <vector>
#include <string>

#if TRINITY_COMPILER == TRINITY_COMPILER_GNU
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using namespace Trinity::ChatCommands;

class onlogin_commandscript : public CommandScript
{
public:
    onlogin_commandscript() : CommandScript("onlogin_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "onlogin", HandleOnLoginCommand, rbac::RBAC_PERM_COMMAND_ONLOGIN, Console::No }
        };
        return commandTable;
    }

    static bool HandleOnLoginCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        std::istringstream iss(args);

        std::string argPlayerName;
        iss >> argPlayerName;
        if (argPlayerName.empty())
            return false;

        std::string argCommand;
        std::getline(iss, argCommand);
        argCommand.erase(0, argCommand.find_first_not_of(" "));
        if (argCommand.empty())
            return false;

        std::string playerName = argPlayerName;
        if (!normalizePlayerName(playerName))
            return false;

        // if argPlayer is online, just run the command
        if (ObjectAccessor::FindPlayerByName(playerName))
        {
            return handler->ParseCommands(argCommand);
        }

        ObjectGuid guid = sCharacterCache->GetCharacterGuidByName(playerName);
        if (guid.IsEmpty())
        {
            handler->SendSysMessage(LANG_NO_PLAYERS_FOUND);
            return true;
        }

        sOnLoginCmdMgr->AddCommand(guid, argCommand);
        handler->PSendSysMessage("Command stored for %s: %s", playerName.c_str(), argCommand.c_str());
        return true;
    }
};

class OnLoginPlayerScript : public PlayerScript
{
public:
    OnLoginPlayerScript() : PlayerScript("OnLoginPlayerScript") { }

    void OnLogin(Player* player, bool loginFirst) override
    {
        TC_LOG_DEBUG("onlogin", "Player OnLogin name {} guid {}", player->GetName(), player->GetGUID());
        auto& cmds = sOnLoginCmdMgr->GetCommandsForPlayer(player->GetGUID());
        for (OnLoginCmd* cmd : cmds)
        {
            TC_LOG_DEBUG("onlogin", "Player OnLogin ParseCommand {}", cmd->GetCommand());
            CliHandler cliHandler(nullptr, nullptr);
            cliHandler.ParseCommands(cmd->GetCommand());
        }
        TC_LOG_DEBUG("onlogin", "Player OnLogin ClearCommandsForPlayer {}", player->GetGUID());
        sOnLoginCmdMgr->ClearCommandsForPlayer(player->GetGUID());
    }
};

// Register the script (add this to your script loader if needed)
void AddSC_onlogin_commandscript()
{
    new onlogin_commandscript();
    new OnLoginPlayerScript();
}
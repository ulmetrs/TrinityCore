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

    static std::unordered_map<uint32, std::vector<std::string>> s_pendingCommands;

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

        // Detect target's GUID
        
        if (Player* player = ObjectAccessor::FindPlayerByName(playerName))
        {
            // TODO reroute to just run the command, player is online
            // for now return error
            handler->SendSysMessage("player is online, just run the command");
            return true;
        }

        ObjectGuid guid = sCharacterCache->GetCharacterGuidByName(playerName);
        if (guid.IsEmpty())
        {
            handler->SendSysMessage(LANG_NO_PLAYERS_FOUND);
            return true;
        }

        s_pendingCommands[guid.GetCounter()].emplace_back(argCommand.c_str());
        handler->PSendSysMessage("Command {} stored for player {}", argCommand.c_str(), playerName.c_str());
        return true;
    }
};

std::unordered_map<uint32, std::vector<std::string>> onlogin_commandscript::s_pendingCommands;

class OnLoginPlayerScript : public PlayerScript
{
public:
    OnLoginPlayerScript() : PlayerScript("OnLoginPlayerScript") { }

    // If you want this to trigger on map entry, use OnMapChanged instead of OnLogin:
    // void OnMapChanged(Player* player) override
    void OnLogin(Player* player, bool loginFirst) override
    {
        TC_LOG_DEBUG("onlogin", "onlogin_commandscript player logged in {}", player->GetName());
        uint32 playerId = player->GetGUID().GetCounter();

        auto itr = onlogin_commandscript::s_pendingCommands.find(playerId);
        if (itr != onlogin_commandscript::s_pendingCommands.end())
        {
            TC_LOG_DEBUG("onlogin", "onlogin_commandscript found commands for player {}", player->GetName());
            for (const std::string& cmd : itr->second)
            {
                TC_LOG_DEBUG("onlogin", "onlogin_commandscript executing command {}", cmd.c_str());
                // Execute as server console (admin permissions)
                CliHandler cliHandler(nullptr, nullptr);
                cliHandler.ParseCommands(cmd.c_str());
            }
            onlogin_commandscript::s_pendingCommands.erase(itr);
        }
    }
};

// Register the script (add this to your script loader if needed)
void AddSC_onlogin_commandscript()
{
    new onlogin_commandscript();
    new OnLoginPlayerScript();
}
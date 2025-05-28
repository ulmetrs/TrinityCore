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

    // Static storage for pending commands
    static std::unordered_map<std::string, std::vector<std::string>> s_pendingCommands;

    static bool HandleOnLoginCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        TC_LOG_DEBUG("onlogin", "onlogin_commandscript with args {}", args);

        std::istringstream iss(args);
        std::string playerName;
        iss >> playerName;

        if (playerName.empty())
            return false;


        TC_LOG_DEBUG("onlogin", "onlogin_commandscript got player name {}", playerName);

        std::string restOfCommand;
        std::getline(iss, restOfCommand);
        // Remove leading spaces from restOfCommand
        restOfCommand.erase(0, restOfCommand.find_first_not_of(" "));

        if (restOfCommand.empty())
            return false;

        TC_LOG_DEBUG("onlogin", "onlogin_commandscript got rest of command {}", restOfCommand);

        std::string name = playerName;
        if (!normalizePlayerName(name))
            return false;

        TC_LOG_DEBUG("onlogin", "onlogin_commandscript got normalized player name {}", name);

        // Detect target's GUID
        ObjectGuid guid;
        if (Player* player = ObjectAccessor::FindPlayerByName(name))
        {
            TC_LOG_DEBUG("onlogin", "onlogin_commandscript found player by name, getting guid");
            guid = player->GetGUID();
        }
        else
        {
            TC_LOG_DEBUG("onlogin", "onlogin_commandscript no player found, getting from cache");
            guid = sCharacterCache->GetCharacterGuidByName(name);
        }

        // Target must exist
        if (guid.IsEmpty())
        {
            TC_LOG_DEBUG("onlogin", "onlogin_commandscript guid is empty returning error");
            handler->SendSysMessage(LANG_NO_PLAYERS_FOUND);
            return true;
        }

        s_pendingCommands[name].emplace_back(restOfCommand);

        handler->PSendSysMessage("Command stored for %s: %s", name.c_str(), restOfCommand.c_str());
        return true;
    }
};

// Define the static member
std::unordered_map<std::string, std::vector<std::string>> onlogin_commandscript::s_pendingCommands;

class OnLoginPlayerScript : public PlayerScript
{
public:
    OnLoginPlayerScript() : PlayerScript("OnLoginPlayerScript") { }

    // If you want this to trigger on map entry, use OnMapChanged instead of OnLogin:
    // void OnMapChanged(Player* player) override
    void OnLogin(Player* player, bool loginFirst) override
    {
        TC_LOG_DEBUG("onlogin", "onlogin_commandscript player logged in {}", player->GetName());
        std::string playerName = player->GetName();
        std::string name = playerName;
        if (!normalizePlayerName(name))
        {
            TC_LOG_DEBUG("onlogin", "onlogin_commandscript could not normalize player name {}", playerName);
            return;
        }

        auto itr = onlogin_commandscript::s_pendingCommands.find(name);
        if (itr != onlogin_commandscript::s_pendingCommands.end())
        {
            TC_LOG_DEBUG("onlogin", "onlogin_commandscript found commands for player {}", name);
            for (const std::string& cmd : itr->second)
            {
                TC_LOG_DEBUG("onlogin", "onlogin_commandscript executing command {}", cmd);
                // Execute as server console (admin permissions)
                CliHandler cliHandler(nullptr, nullptr);
                TC_LOG_DEBUG("onlogin", "onlogin_commandscript calling ParseCommands"); 
                cliHandler.ParseCommands(cmd.c_str());
                TC_LOG_DEBUG("onlogin", "onlogin_commandscript ParseCommands returned"); 
            }
            // Clear commands after execution
            TC_LOG_DEBUG("onlogin", "onlogin_commandscript erasing commands for player {}", name);
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
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

class onlogin_commandscript : public CommandScript
{
public:
    onlogin_commandscript() : CommandScript("onlogin_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "onlogin", SEC_ADMINISTRATOR, true, &HandleOnLoginCommand, "" }
        };
        return commandTable;
    }

    // Static storage for pending commands
    static std::unordered_map<std::string, std::vector<std::string>> s_pendingCommands;

    static bool HandleOnLoginCommand(ChatHandler* handler, char const* args)
    {
        if (!args || *args == '\0')
        {
            handler->SendSysMessage("Usage: .onlogin [playername] [command]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::istringstream iss(args);
        std::string playerName;
        iss >> playerName;

        if (playerName.empty())
        {
            handler->SendSysMessage("Usage: .onlogin [playername] [command]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string restOfCommand;
        std::getline(iss, restOfCommand);
        // Remove leading spaces from restOfCommand
        restOfCommand.erase(0, restOfCommand.find_first_not_of(" "));

        if (restOfCommand.empty())
        {
            handler->SendSysMessage("Usage: .onlogin [playername] [command]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string name = playerName;
        if (!normalizePlayerName(name))
            return false;

        // Detect target's GUID
        ObjectGuid guid;
        if (Player* player = ObjectAccessor::FindPlayerByName(name))
            guid = player->GetGUID();
        else
            guid = sCharacterCache->GetCharacterGuidByName(name);

        // Target must exist
        if (guid.IsEmpty())
        {
            handler->SendSysMessage(LANG_NO_PLAYERS_FOUND);
            return true;
        }

        // Store the command for the player (case-insensitive, normalized)
        std::transform(playerName.begin(), playerName.end(), playerName.begin(), ::tolower);
        s_pendingCommands[playerName].emplace_back(restOfCommand);

        handler->PSendSysMessage("Command stored for %s: %s", playerName.c_str(), restOfCommand.c_str());
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
        std::string playerKey = player->GetName();
        std::transform(playerKey.begin(), playerKey.end(), playerKey.begin(), ::tolower);

        auto itr = onlogin_commandscript::s_pendingCommands.find(playerKey);
        if (itr != onlogin_commandscript::s_pendingCommands.end())
        {
            for (const std::string& cmd : itr->second)
            {
                // Execute as server console (admin permissions)
                CliHandler cliHandler(nullptr, nullptr);
                cliHandler.ParseCommands(cmd.c_str());
            }
            // Clear commands after execution
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
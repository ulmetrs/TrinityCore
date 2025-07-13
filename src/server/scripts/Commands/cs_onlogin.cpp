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
#include "DatabaseEnv.h"
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

        // Insert command into database
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_ON_LOGIN_COMMANDS);
        stmt->setUInt32(0, guid.GetCounter());
        stmt->setString(1, argCommand);
        CharacterDatabase.Execute(stmt);

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
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ON_LOGIN_COMMANDS_BY_GUID);
        stmt->setUInt32(0, player->GetGUID().GetCounter());
        PreparedQueryResult result = CharacterDatabase.Query(stmt);
        if (!result)
            return;

        // Start a transaction
        SQLTransaction trans = CharacterDatabase.BeginTransaction();

        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].GetUInt32();
            std::string command = fields[2].GetString();

            CliHandler cliHandler(nullptr, nullptr);
            cliHandler.ParseCommands(command);

            // Update command in database (mark as deleted)
            CharacterDatabasePreparedStatement* updateStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ON_LOGIN_COMMANDS);
            updateStmt->setUInt32(0, id);
            CharacterDatabase.ExecuteOrAppend(trans, updateStmt);

        } while (result->NextRow());

        // Commit the transaction
        CharacterDatabase.CommitTransaction(trans);
    }
};

// Register the script (add this to your script loader if needed)
void AddSC_onlogin_commandscript()
{
    new onlogin_commandscript();
    new OnLoginPlayerScript();
}
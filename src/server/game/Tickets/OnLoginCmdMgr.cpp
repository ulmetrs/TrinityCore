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

#include "OnLoginCmdMgr.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"

OnLoginCmd::OnLoginCmd() { }
OnLoginCmd::OnLoginCmd(ObjectGuid playerGuid, std::string command) : _playerGuid(playerGuid), _command(command), _deletedAt(0)
{
    _id = sOnLoginCmdMgr->GenerateCommandId();
    _createAt = GameTime::GetGameTime();
    _updatedAt = GameTime::GetGameTime();
}
OnLoginCmd::~OnLoginCmd() { }

bool OnLoginCmd::LoadFromDB(Field* fields)
{
    // 0        1          2         3          4          5
    // id, player_guid, command, createdAt, updatedAt, deletedAt
    uint8 index = 0;
    _id         = fields[  index].GetUInt32();
    _playerGuid = ObjectGuid(HighGuid::Player, fields[++index].GetUInt32());
    _command    = fields[++index].GetString();
    _createAt   = fields[++index].GetUInt64();
    _updatedAt  = fields[++index].GetUInt64();
    _deletedAt  = fields[++index].GetUInt64();
    return true;
}

void OnLoginCmd::SaveToDB() const
{
    // 0        1          2         3          4          5
    // id, player_guid, command, createdAt, updatedAt, deletedAt
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ON_LOGIN_COMMANDS);
    uint8 index = 0;
    stmt->setUInt32(  index, _id);
    stmt->setUInt32(++index, _playerGuid.GetCounter());
    stmt->setString(++index, _command);
    stmt->setUInt64(++index, _createAt);
    stmt->setUInt64(++index, _updatedAt);
    stmt->setUInt64(++index, _deletedAt);
    CharacterDatabase.Execute(stmt);
}

void OnLoginCmd::SoftDeleteFromDB(CharacterDatabaseTransaction& trans) const
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ON_LOGIN_COMMANDS);
    stmt->setUInt32(0, _id);
    CharacterDatabase.ExecuteOrAppend(trans, stmt);
}

void OnLoginCmdMgr::ResetCommands()
{
    // Clean up existing commands
    for (auto& pair : _onLoginCommandList)
    {
        for (OnLoginCmd* cmd : pair.second)
            delete cmd;
    }
    _onLoginCommandList.clear();

    // Reset last command ID
    _lastCommandId = 0;

    // Truncate Table
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ALL_ON_LOGIN_COMMANDS);
    CharacterDatabase.Execute(stmt);
}

void OnLoginCmdMgr::LoadCommands()
{
    uint32 oldMSTime = getMSTime();

    // Clean up existing commands
    for (auto& pair : _onLoginCommandList)
    {
        for (OnLoginCmd* cmd : pair.second)
            delete cmd;
    }
    _onLoginCommandList.clear();

    _lastCommandId = 0;

    // Reload from database
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ON_LOGIN_COMMANDS);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 On Login Commands. DB table `on_login_command` is empty!");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        OnLoginCmd* cmd = new OnLoginCmd();
        if (!cmd->LoadFromDB(fields))
        {
            delete cmd;
            continue;
        }

        // Keep new command ids synced with database
        uint32 id = cmd->GetId();
        if (_lastCommandId < id)
            _lastCommandId = id;

        // Remove deleted commands: We need to load all deleted commands on select in order to update _lastCommandId
        // We could filter these out in the select but we would need to add another unique id column to the table to key
        // off of for updates. (The statements are executed asnyc and so we cannot get the auto increment id on insert)
        // For now, we are relying on GenerateCommandId to match our auto increment id so that updates will operate on the correct
        // record.  This strategy is copied from the gm_ticket system.
        if (cmd->GetDeletedAt() > 0)
        {
            delete cmd;
            continue;
        }

        _onLoginCommandList[cmd->GetPlayerGuid()].push_back(cmd);
        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} On Login Commands in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void OnLoginCmdMgr::AddCommand(ObjectGuid playerGuid, std::string const& command)
{
    OnLoginCmd* cmd = new OnLoginCmd(playerGuid, command);
    cmd->SaveToDB();
    _onLoginCommandList[playerGuid].push_back(cmd);
}

const std::vector<OnLoginCmd*>& OnLoginCmdMgr::GetCommandsForPlayer(ObjectGuid const& guid) const
{
    static const std::vector<OnLoginCmd*> empty;
    auto itr = _onLoginCommandList.find(guid);
    return itr != _onLoginCommandList.end() ? itr->second : empty;
}

void OnLoginCmdMgr::ClearCommandsForPlayer(ObjectGuid const& guid)
{
    auto itr = _onLoginCommandList.find(guid);
    if (itr == _onLoginCommandList.end())
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    for (OnLoginCmd* cmd : itr->second)
    {
        cmd->SoftDeleteFromDB(trans);
        delete cmd;
    }
    itr->second.clear();
    CharacterDatabase.CommitTransaction(trans);

    _onLoginCommandList.erase(itr);
}
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

#ifndef ON_LOGIN_CMD_MGR_H
#define ON_LOGIN_CMD_MGR_H

#include "DatabaseEnvFwd.h"
#include <unordered_map>
#include <vector>

class TC_GAME_API OnLoginCmd
{
public:
    OnLoginCmd();
    OnLoginCmd(ObjectGuid playerGuid, std::string command);
    ~OnLoginCmd();
    uint32 GetId() const { return _id; }
    ObjectGuid GetPlayerGuid() const { return _playerGuid; }
    std::string const& GetCommand() const { return _command; }
    uint64 GetCreateAt() const { return _createAt; }
    uint64 GetUpdatedAt() const { return _updatedAt; }
    uint64 GetDeletedAt() const { return _deletedAt; }

    bool LoadFromDB(Field* fields);
    void SaveToDB() const;
    void SoftDeleteFromDB(CharacterDatabaseTransaction& trans) const;

private:
    uint32 _id;
    ObjectGuid _playerGuid;
    std::string _command;
    uint64 _createAt;
    uint64 _updatedAt;
    uint64 _deletedAt;
};

class TC_GAME_API OnLoginCmdMgr
{
public:
    static OnLoginCmdMgr* instance()
    {
        static OnLoginCmdMgr instance;
        return &instance;
    }

    uint32 GenerateCommandId() { return ++_lastCommandId; }
    void ResetCommands();
    void LoadCommands();
    void AddCommand(ObjectGuid playerGuid, std::string const& command);

    const std::vector<OnLoginCmd*>& GetCommandsForPlayer(ObjectGuid const& guid) const;
    void ClearCommandsForPlayer(ObjectGuid const& guid);

private:
    OnLoginCmdMgr() : _lastCommandId(0) { }
    ~OnLoginCmdMgr()
    {
        for (auto& pair : _onLoginCommandList)
        {
            for (OnLoginCmd* cmd : pair.second)
                delete cmd;
        }
    }
    std::unordered_map<ObjectGuid, std::vector<OnLoginCmd*>> _onLoginCommandList;

    uint32 _lastCommandId;
};

#define sOnLoginCmdMgr OnLoginCmdMgr::instance()

#endif // ON_LOGIN_CMD_MGR_H

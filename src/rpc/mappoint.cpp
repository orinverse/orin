// Copyright (c) 2025 The Orin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <index/mappointindex.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <rpc/util.h>
#include <sync.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

static MapPointIndex& GetMapPointIndex()
{
    if (!g_mappoint_index) {
        throw JSONRPCError(RPC_MISC_ERROR, "Map point index is not enabled. Start the node with -mappointindex=1.");
    }
    g_mappoint_index->BlockUntilSyncedToCurrentChain();
    return *g_mappoint_index;
}

static UniValue MapPointToJSON(const MapPointInfo& info, bool include_transfers)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("origin_txid", info.origin_txid.GetHex());
    obj.pushKV("origin_height", info.origin_height);
    obj.pushKV("origin_owner", info.origin_owner);
    obj.pushKV("current_owner", info.current_owner);
    obj.pushKV("enc_lat", info.encoded_lat);
    obj.pushKV("enc_lon", info.encoded_lon);
    obj.pushKV("lat", info.Latitude());
    obj.pushKV("lon", info.Longitude());
    if (include_transfers) {
        UniValue arr(UniValue::VARR);
        for (const auto& transfer : info.transfers) {
            UniValue entry(UniValue::VOBJ);
            entry.pushKV("transfer_txid", transfer.transfer_txid.GetHex());
            entry.pushKV("height", transfer.height);
            entry.pushKV("new_owner", transfer.new_owner);
            arr.push_back(entry);
        }
        obj.pushKV("transfers", arr);
    }
    return obj;
}

static void SortPoints(std::vector<MapPointInfo>& points)
{
    std::sort(points.begin(), points.end(), [](const MapPointInfo& a, const MapPointInfo& b) {
        if (a.origin_height != b.origin_height) return a.origin_height < b.origin_height;
        if (a.current_owner != b.current_owner) return a.current_owner < b.current_owner;
        return a.origin_txid < b.origin_txid;
    });
}

static RPCHelpMan getmappoint()
{
    return RPCHelpMan{
        "getmappoint",
        "\nReturn information about a single RealMap point transaction.\n",
        {
            {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Transaction id that created the point"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR_HEX, "origin_txid", "Transaction id that created the point"},
                {RPCResult::Type::NUM, "origin_height", "Block height of the creation transaction"},
                {RPCResult::Type::STR, "origin_owner", "Original owner address"},
                {RPCResult::Type::STR, "current_owner", "Current owner address"},
                {RPCResult::Type::NUM, "enc_lat", "Encoded latitude (lat*1e6)"},
                {RPCResult::Type::NUM, "enc_lon", "Encoded longitude (lon*1e6)"},
                {RPCResult::Type::NUM, "lat", "Latitude"},
                {RPCResult::Type::NUM, "lon", "Longitude"},
                {RPCResult::Type::ARR, "transfers", "Ownership transfer history", {
                    {RPCResult::Type::OBJ, "", "", {
                        {RPCResult::Type::STR_HEX, "transfer_txid", "Transfer transaction id"},
                        {RPCResult::Type::NUM, "height", "Block height of the transfer"},
                        {RPCResult::Type::STR, "new_owner", "New owner address"},
                    }},
                }},
            }
        },
        RPCExamples{
            HelpExampleCli("getmappoint", "\"txid\"") +
            HelpExampleRpc("getmappoint", "\"txid\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            MapPointIndex& index = GetMapPointIndex();
            const uint256 txid = ParseHashV(request.params[0], "txid");
            MapPointInfo info;
            if (!index.GetPoint(txid, info)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Map point not found");
            }
            return MapPointToJSON(info, true);
        }};
}

static void ParseHeightParams(const JSONRPCRequest& request, size_t offset, int& from, int& to)
{
    if (request.params.size() > offset && !request.params[offset].isNull()) {
        from = request.params[offset].getInt<int>();
    } else {
        from = 0;
    }
    if (request.params.size() > offset + 1 && !request.params[offset + 1].isNull()) {
        to = request.params[offset + 1].getInt<int>();
    } else {
        to = std::numeric_limits<int>::max();
    }
    if (to < from) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "to_height must be greater than or equal to from_height");
    }
}

static RPCHelpMan listmappoints()
{
    return RPCHelpMan{
        "listmappoints",
        "\nList all RealMap points (optionally in a height range).\n",
        {
            {"from_height", RPCArg::Type::NUM, RPCArg::Default{0}, "Start height (inclusive)"},
            {"to_height", RPCArg::Type::NUM, RPCArg::DefaultHint{"tip"}, "End height (inclusive)"},
        },
        RPCResult{
            RPCResult::Type::ARR, "", "",
            {
                {RPCResult::Type::OBJ, "", "", {
                                               {RPCResult::Type::STR_HEX, "origin_txid", "Transaction id that created the point"},
                                               {RPCResult::Type::NUM, "origin_height", "Block height of the creation transaction"},
                                               {RPCResult::Type::STR, "origin_owner", "Original owner address"},
                                               {RPCResult::Type::STR, "current_owner", "Current owner address"},
                                               {RPCResult::Type::NUM, "enc_lat", "Encoded latitude"},
                                               {RPCResult::Type::NUM, "enc_lon", "Encoded longitude"},
                                               {RPCResult::Type::NUM, "lat", "Latitude"},
                                               {RPCResult::Type::NUM, "lon", "Longitude"},
                                           }},
            }
        },
        RPCExamples{
            HelpExampleCli("listmappoints", "") +
            HelpExampleRpc("listmappoints", "") +
            HelpExampleCli("listmappoints", "100 200") +
            HelpExampleRpc("listmappoints", "100, 200")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            MapPointIndex& index = GetMapPointIndex();
            int from{0};
            int to{std::numeric_limits<int>::max()};
            ParseHeightParams(request, 0, from, to);
            std::vector<MapPointInfo> points = index.GetPointsInHeightRange(from, to);
            SortPoints(points);
            UniValue arr(UniValue::VARR);
            for (const auto& info : points) {
                arr.push_back(MapPointToJSON(info, false));
            }
            return arr;
        }};
}

static RPCHelpMan getaddresspoints()
{
    return RPCHelpMan{
        "getaddresspoints",
        "\nList all RealMap points owned by the specified address or addresses.\n",
        {
            {"addresses", RPCArg::Type::STR, RPCArg::Optional::NO, "Address or JSON array of addresses"},
            {"from_height", RPCArg::Type::NUM, RPCArg::Default{0}, "Start height (inclusive)"},
            {"to_height", RPCArg::Type::NUM, RPCArg::DefaultHint{"tip"}, "End height (inclusive)"},
        },
        RPCResult{RPCResult::Type::ARR, "", "", {
                {RPCResult::Type::OBJ, "", "", {
                                                                              {RPCResult::Type::STR_HEX, "origin_txid", "Transaction id that created the point"},
                                                                              {RPCResult::Type::NUM, "origin_height", "Block height of the creation transaction"},
                                                                              {RPCResult::Type::STR, "origin_owner", "Original owner address"},
                                                                              {RPCResult::Type::STR, "current_owner", "Current owner address"},
                                                                              {RPCResult::Type::NUM, "enc_lat", "Encoded latitude"},
                                                                              {RPCResult::Type::NUM, "enc_lon", "Encoded longitude"},
                                                                              {RPCResult::Type::NUM, "lat", "Latitude"},
                                                                              {RPCResult::Type::NUM, "lon", "Longitude"},
                                                                          }},
                                           }},
        RPCExamples{
            HelpExampleCli("getaddresspoints", "\"" + EXAMPLE_ADDRESS[0] + "\"") +
            HelpExampleRpc("getaddresspoints", "\"" + EXAMPLE_ADDRESS[0] + "\"") +
            HelpExampleCli("getaddresspoints", "\"[" + EXAMPLE_ADDRESS[0] + "," + EXAMPLE_ADDRESS[1] + "]\" 0 1000")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            MapPointIndex& index = GetMapPointIndex();
            std::vector<std::string> owners;
            const UniValue& param = request.params[0];
            if (param.isStr()) {
                owners.push_back(param.get_str());
            } else if (param.isArray()) {
                owners.reserve(param.size());
                for (unsigned int i = 0; i < param.size(); ++i) {
                    owners.push_back(param[i].get_str());
                }
            } else {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "addresses must be a string or array");
            }
            if (owners.empty()) {
                return UniValue(UniValue::VARR);
            }
            int from{0};
            int to{std::numeric_limits<int>::max()};
            ParseHeightParams(request, 1, from, to);
            std::vector<MapPointInfo> points = index.GetPointsForOwner(owners, from, to);
            SortPoints(points);
            UniValue arr(UniValue::VARR);
            for (const auto& info : points) {
                arr.push_back(MapPointToJSON(info, false));
            }
            return arr;
        }};
}

static RPCHelpMan rebuildmappointindex()
{
    return RPCHelpMan{
        "rebuildmappointindex",
        "\nRebuild the map point index from the active chain.\n",
        {},
        RPCResult{RPCResult::Type::BOOL, "", "true if the rebuild was started"},
        RPCExamples{
            HelpExampleCli("rebuildmappointindex", "") +
            HelpExampleRpc("rebuildmappointindex", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            if (!g_mappoint_index) {
                throw JSONRPCError(RPC_MISC_ERROR, "Map point index is not enabled");
            }
            if (!g_mappoint_index->Rebuild()) {
                throw JSONRPCError(RPC_MISC_ERROR, "Failed to rebuild map point index");
            }
            return true;
        }};
}

void RegisterMapPointRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"blockchain", &getmappoint},
        {"blockchain", &listmappoints},
        {"blockchain", &getaddresspoints},
        {"blockchain", &rebuildmappointindex},
    };
    for (const auto& command : commands) {
        t.appendCommand(command.name, &command);
    }
}

// Copyright (c) 2025 The Orin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <core_io.h>
#include <index/mappointindex.h>
#include <key_io.h>
#include <rpc/util.h>
#include <script/standard.h>
#include <tinyformat.h>
#include <util/mappoint.h>
#include <util/translation.h>
#include <wallet/coincontrol.h>
#include <wallet/ismine.h>
#include <wallet/rpc/util.h>
#include <wallet/spend.h>
#include <wallet/wallet.h>

#include <univalue.h>

#include <univalue.h>

#ifndef BUILD_WALLET_TOOL

namespace wallet {

// Forward declaration from spend.cpp
UniValue SendMoney(CWallet& wallet,
                   const CCoinControl& coin_control,
                   std::vector<CRecipient>& recipients,
                   mapValue_t map_value,
                   bool verbose);

static constexpr CAmount DEFAULT_MAP_POINT_AMOUNT = COIN / 100; // 0.01 ORIN

RPCHelpMan sendmappoint()
{
    return RPCHelpMan{
        "sendmappoint",
        "\nCreate and broadcast a RealMap point transaction." + HELP_REQUIRING_PASSPHRASE,
        {
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "Owner address for the point"},
            {"latitude", RPCArg::Type::NUM, RPCArg::Optional::NO, "Latitude in decimal degrees"},
            {"longitude", RPCArg::Type::NUM, RPCArg::Optional::NO, "Longitude in decimal degrees"},
            {"amount", RPCArg::Type::AMOUNT, RPCArg::Default{0.01}, "Amount to send to the owner address"},
            {"comment", RPCArg::Type::STR, RPCArg::Default{""}, "Optional wallet comment"},
            {"verbose", RPCArg::Type::BOOL, RPCArg::Default{false}, "If true, return a json object with txid and fee reason"},
        },
        {
            RPCResult{"if verbose is false",
                RPCResult::Type::STR_HEX, "txid", "The transaction id"
            },
            RPCResult{"if verbose is true",
                RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::STR_HEX, "txid", "The transaction id"},
                    {RPCResult::Type::STR, "fee_reason", "The transaction fee reason"},
                }
            },
        },
        RPCExamples{
            HelpExampleCli("sendmappoint", "\"" + EXAMPLE_ADDRESS[0] + "\" 55.751244 37.618423") +
            HelpExampleRpc("sendmappoint", "\"" + EXAMPLE_ADDRESS[0] + "\",55.751244,37.618423") +
            HelpExampleCli("sendmappoint", "\"" + EXAMPLE_ADDRESS[0] + "\" 55.751244 37.618423 0.5 \"Moscow\" true")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) return UniValue::VNULL;

            pwallet->BlockUntilSyncedToCurrentChain();
            LOCK(pwallet->cs_wallet);

            EnsureWalletIsUnlocked(*pwallet);

            const std::string owner_str = request.params[0].get_str();
            CTxDestination owner_dest = DecodeDestination(owner_str);
            if (!IsValidDestination(owner_dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Orin address");
            }

            const double latitude = request.params[1].get_real();
            const double longitude = request.params[2].get_real();

            CAmount amount = request.params[3].isNull() ? DEFAULT_MAP_POINT_AMOUNT : AmountFromValue(request.params[3]);
            if (amount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount must be greater than zero");
            }

            int64_t enc_lat{0};
            int64_t enc_lon{0};
            std::string coord_error;
            if (!mappoint::EncodeCoordinates(latitude, longitude, enc_lat, enc_lon, coord_error)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, coord_error);
            }
            const std::string payload = mappoint::BuildPayload(enc_lat, enc_lon);

            std::vector<unsigned char> data(payload.begin(), payload.end());
            CScript op_return = CScript() << OP_RETURN << data;

            std::vector<CRecipient> recipients;
            recipients.push_back({GetScriptForDestination(owner_dest), amount, false});
            recipients.push_back({op_return, 0, false});

            mapValue_t map_value;
            map_value["mappoint"] = "1";
            if (!request.params[4].isNull() && !request.params[4].get_str().empty()) {
                map_value["comment"] = request.params[4].get_str();
            }
            map_value["mappoint_lat"] = strprintf("%.6f", latitude);
            map_value["mappoint_lon"] = strprintf("%.6f", longitude);

            CCoinControl coin_control;
            const bool verbose = request.params.size() > 5 ? request.params[5].get_bool() : false;
            return SendMoney(*pwallet, coin_control, recipients, std::move(map_value), verbose);
        }};
}

RPCHelpMan sendpointtransfer()
{
    return RPCHelpMan{
        "sendpointtransfer",
        "\nTransfer ownership of an existing RealMap point." + HELP_REQUIRING_PASSPHRASE,
        {
            {"point_txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Transaction id of the map point"},
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "New owner address"},
            {"amount", RPCArg::Type::AMOUNT, RPCArg::Default{0.01}, "Amount to send to the new owner"},
            {"comment", RPCArg::Type::STR, RPCArg::Default{""}, "Optional wallet comment"},
            {"verbose", RPCArg::Type::BOOL, RPCArg::Default{false}, "If true, return a json object with txid and fee reason"},
        },
        {
            RPCResult{"if verbose is false",
                RPCResult::Type::STR_HEX, "txid", "The transaction id"
            },
            RPCResult{"if verbose is true",
                RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::STR_HEX, "txid", "The transaction id"},
                    {RPCResult::Type::STR, "fee_reason", "The transaction fee reason"},
                }
            },
        },
        RPCExamples{
            HelpExampleCli("sendpointtransfer", "\"<point_txid>\" \"" + EXAMPLE_ADDRESS[0] + "\"") +
            HelpExampleRpc("sendpointtransfer", "\"<point_txid>\",\"" + EXAMPLE_ADDRESS[0] + "\"") +
            HelpExampleCli("sendpointtransfer", "\"<point_txid>\" \"" + EXAMPLE_ADDRESS[0] + "\" 0.5 \"transfer\" true")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) return UniValue::VNULL;

            pwallet->BlockUntilSyncedToCurrentChain();
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(*pwallet);

            if (!g_mappoint_index) {
                throw JSONRPCError(RPC_MISC_ERROR, "Map point index is not enabled. Start the node with -mappointindex=1.");
            }
            g_mappoint_index->BlockUntilSyncedToCurrentChain();

            const uint256 point_txid = ParseHashV(request.params[0], "point_txid");

            MapPointInfo info;
            if (!g_mappoint_index->GetPoint(point_txid, info)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Map point not found");
            }

            if (info.current_owner.empty()) {
                throw JSONRPCError(RPC_MISC_ERROR, "Map point does not have a current owner");
            }

            const std::string new_owner_str = request.params[1].get_str();
            CTxDestination new_owner_dest = DecodeDestination(new_owner_str);
            if (!IsValidDestination(new_owner_dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid new owner address");
            }

            CTxDestination current_owner_dest = DecodeDestination(info.current_owner);
            if (!IsValidDestination(current_owner_dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Current owner address is invalid");
            }
            CScript current_owner_script = GetScriptForDestination(current_owner_dest);
            if (!(pwallet->IsMine(current_owner_script) & ISMINE_SPENDABLE)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Wallet does not control the current owner address");
            }

            CAmount amount = request.params[2].isNull() ? DEFAULT_MAP_POINT_AMOUNT : AmountFromValue(request.params[2]);
            if (amount <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount must be greater than zero");
            }

            CCoinControl coin_control;
            const CoinsResult available = AvailableCoins(*pwallet);
            bool selected = false;
            for (const COutput& out : available.all()) {
                if (!out.spendable) continue;
                CTxDestination dest;
                if (ExtractDestination(out.txout.scriptPubKey, dest) && EncodeDestination(dest) == info.current_owner) {
                    coin_control.Select(out.outpoint);
                    selected = true;
                    break;
                }
            }
            if (!selected) {
                throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "No spendable UTXO found for the current owner address");
            }
            coin_control.m_allow_other_inputs = true;

            std::string payload = strprintf("%s:%s", mappoint::MAP_POINT_TRANSFER_PREFIX, point_txid.GetHex());
            std::vector<unsigned char> data(payload.begin(), payload.end());
            CScript op_return = CScript() << OP_RETURN << data;

            std::vector<CRecipient> recipients;
            recipients.push_back({GetScriptForDestination(new_owner_dest), amount, false});
            recipients.push_back({op_return, 0, false});

            mapValue_t map_value;
            map_value["mappoint_transfer"] = point_txid.GetHex();
            map_value["mappoint_previous_owner"] = info.current_owner;
            map_value["mappoint_new_owner"] = new_owner_str;
            if (!request.params[3].isNull() && !request.params[3].get_str().empty()) {
                map_value["comment"] = request.params[3].get_str();
            }

            const bool verbose = request.params.size() > 4 ? request.params[4].get_bool() : false;
            return SendMoney(*pwallet, coin_control, recipients, std::move(map_value), verbose);
        }};
}

} // namespace wallet

#else // BUILD_WALLET_TOOL

namespace wallet {

static RPCHelpMan UnsupportedCommand(const std::string& name)
{
    return RPCHelpMan{
        name,
        "\nThis command is not available in this binary.\n",
        {},
        RPCResult{RPCResult::Type::NONE, "", ""},
        RPCExamples{},
        [](const RPCHelpMan&, const JSONRPCRequest&) -> UniValue {
            throw JSONRPCError(RPC_MISC_ERROR, "Map point RPCs are not available in this wallet tool build");
        }};
}

RPCHelpMan sendmappoint()
{
    return UnsupportedCommand("sendmappoint");
}

RPCHelpMan sendpointtransfer()
{
    return UnsupportedCommand("sendpointtransfer");
}

} // namespace wallet

#endif // BUILD_WALLET_TOOL

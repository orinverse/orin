// Copyright (c) 2025 The Orin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <index/mappointindex.h>

#include <dbwrapper.h>
#include <key_io.h>
#include <node/blockstorage.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <undo.h>
#include <util/check.h>
#include <util/mappoint.h>
#include <util/system.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <vector>

using node::UndoReadFromDisk;

namespace {
constexpr uint8_t DB_POINT{'p'};
constexpr uint8_t DB_HEIGHT{'h'};
constexpr uint8_t DB_OWNER{'o'};
constexpr uint8_t DB_TRANSFER{'t'};
constexpr uint8_t DB_TRANSFER_HEIGHT{'y'};

struct HeightKey {
    uint32_t height{0};
    uint256 txid;

    SERIALIZE_METHODS(HeightKey, obj)
    {
        READWRITE(obj.height, obj.txid);
    }
};

struct OwnerKey {
    std::string owner;
    uint256 txid;

    SERIALIZE_METHODS(OwnerKey, obj)
    {
        READWRITE(obj.owner, obj.txid);
    }
};

struct TransferKey {
    uint256 origin;
    uint256 transfer;

    SERIALIZE_METHODS(TransferKey, obj)
    {
        READWRITE(obj.origin, obj.transfer);
    }
};

struct TransferHeightKey {
    uint32_t height{0};
    uint256 origin;
    uint256 transfer;

    SERIALIZE_METHODS(TransferHeightKey, obj)
    {
        READWRITE(obj.height, obj.origin, obj.transfer);
    }
};

bool ExtractOpReturnData(const CScript& script, std::string& payload)
{
    CScript::const_iterator it = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> data;
    if (!script.GetOp(it, opcode, data) || opcode != OP_RETURN) {
        return false;
    }
    if (!script.GetOp(it, opcode, data)) {
        return false;
    }
    if (opcode > OP_PUSHDATA4 || data.empty()) {
        return false;
    }
    payload.assign(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

bool ExtractOwnerAddress(const CTransaction& tx, std::string& owner)
{
    for (const auto& txout : tx.vout) {
        if (txout.scriptPubKey.IsUnspendable()) {
            continue;
        }
        CTxDestination dest;
        if (ExtractDestination(txout.scriptPubKey, dest)) {
            owner = EncodeDestination(dest);
            return true;
        }
    }
    return false;
}

} // namespace

class MapPointIndex::DB : public BaseIndex::DB
{
public:
    DB(size_t cache_size, bool memory, bool wipe, bool obfuscate);

    bool WritePoints(const std::vector<std::pair<uint256, Record>>& records);
    bool WriteRecord(const uint256& txid, const Record& record);
    bool ReadPoint(const uint256& txid, Record& record) const;
    bool ReadByHeight(uint32_t start, uint32_t stop, std::vector<MapPointInfo>& out) const;
    bool ReadOwners(const std::vector<std::string>& owners,
                    uint32_t start,
                    uint32_t stop,
                    std::vector<MapPointInfo>& out) const;
    bool ErasePointsAboveHeight(uint32_t height, std::vector<uint256>& removed_points);

    bool UpdateOwnerIndex(const std::string& old_owner, const std::string& new_owner, const uint256& origin);

    bool WriteTransfer(const TransferKey& key, const TransferRecord& record);
    bool ReadTransfers(const uint256& origin, std::vector<MapPointTransferInfo>& out) const;
    bool RemoveTransfersAboveHeight(uint32_t height, std::vector<std::pair<uint256, std::string>>& owner_updates);
    bool RemoveAllTransfersForOrigin(const uint256& origin);

    static MapPointInfo MakeInfo(const uint256& txid, const Record& record);
};

MapPointIndex::DB::DB(size_t cache_size, bool memory, bool wipe, bool obfuscate) :
    BaseIndex::DB(gArgs.GetDataDirNet() / "indexes" / "mappoint", cache_size, memory, wipe, obfuscate)
{}

MapPointInfo MapPointIndex::DB::MakeInfo(const uint256& txid, const Record& record)
{
    MapPointInfo info;
    info.origin_txid = txid;
    info.origin_height = static_cast<int>(record.height);
    info.origin_owner = record.origin_owner;
    info.current_owner = record.current_owner;
    info.encoded_lat = record.encoded_lat;
    info.encoded_lon = record.encoded_lon;
    return info;
}

bool MapPointIndex::DB::WritePoints(const std::vector<std::pair<uint256, Record>>& records)
{
    if (records.empty()) return true;

    CDBBatch batch(*this);
    for (const auto& entry : records) {
        const uint256& txid = entry.first;
        const Record& record = entry.second;
        batch.Write(std::make_pair(DB_POINT, txid), record);
        batch.Write(std::make_pair(DB_HEIGHT, HeightKey{record.height, txid}), uint8_t{0});
        if (!record.current_owner.empty()) {
            batch.Write(std::make_pair(DB_OWNER, OwnerKey{record.current_owner, txid}), uint8_t{0});
        }
    }
    return WriteBatch(batch);
}

bool MapPointIndex::DB::WriteRecord(const uint256& txid, const Record& record)
{
    return Write(std::make_pair(DB_POINT, txid), record);
}

bool MapPointIndex::DB::ReadPoint(const uint256& txid, Record& record) const
{
    return Read(std::make_pair(DB_POINT, txid), record);
}

bool MapPointIndex::DB::UpdateOwnerIndex(const std::string& old_owner, const std::string& new_owner, const uint256& origin)
{
    CDBBatch batch(*this);
    if (!old_owner.empty()) {
        batch.Erase(std::make_pair(DB_OWNER, OwnerKey{old_owner, origin}));
    }
    if (!new_owner.empty()) {
        batch.Write(std::make_pair(DB_OWNER, OwnerKey{new_owner, origin}), uint8_t{0});
    }
    return WriteBatch(batch);
}

bool MapPointIndex::DB::ReadByHeight(uint32_t start, uint32_t stop, std::vector<MapPointInfo>& out) const
{
    auto cursor = std::unique_ptr<CDBIterator>(const_cast<MapPointIndex::DB*>(this)->NewIterator());
    cursor->Seek(std::make_pair(DB_HEIGHT, HeightKey{start, uint256{}}));
    while (cursor->Valid()) {
        std::pair<uint8_t, HeightKey> key;
        if (!cursor->GetKey(key) || key.first != DB_HEIGHT) {
            break;
        }
        if (key.second.height > stop) {
            break;
        }
        Record record;
        if (ReadPoint(key.second.txid, record)) {
            out.emplace_back(MakeInfo(key.second.txid, record));
        }
        cursor->Next();
    }
    return true;
}

bool MapPointIndex::DB::ReadOwners(const std::vector<std::string>& owners,
                                   uint32_t start,
                                   uint32_t stop,
                                   std::vector<MapPointInfo>& out) const
{
    auto cursor = std::unique_ptr<CDBIterator>(const_cast<MapPointIndex::DB*>(this)->NewIterator());
    for (const auto& owner : owners) {
        cursor->Seek(std::make_pair(DB_OWNER, OwnerKey{owner, uint256{}}));
        while (cursor->Valid()) {
            std::pair<uint8_t, OwnerKey> key;
            if (!cursor->GetKey(key) || key.first != DB_OWNER) {
                break;
            }
            if (key.second.owner != owner) {
                break;
            }
            Record record;
            if (ReadPoint(key.second.txid, record)) {
                if (record.height >= start && record.height <= stop) {
                    out.emplace_back(MakeInfo(key.second.txid, record));
                }
            }
            cursor->Next();
        }
    }
    return true;
}

bool MapPointIndex::DB::ErasePointsAboveHeight(uint32_t height, std::vector<uint256>& removed_points)
{
    auto cursor = std::unique_ptr<CDBIterator>(NewIterator());
    cursor->Seek(std::make_pair(DB_HEIGHT, HeightKey{height + 1, uint256{}}));
    CDBBatch batch(*this);
    while (cursor->Valid()) {
        std::pair<uint8_t, HeightKey> key;
        if (!cursor->GetKey(key) || key.first != DB_HEIGHT) {
            break;
        }
        Record record;
        if (ReadPoint(key.second.txid, record)) {
            batch.Erase(std::make_pair(DB_POINT, key.second.txid));
            if (!record.current_owner.empty()) {
                batch.Erase(std::make_pair(DB_OWNER, OwnerKey{record.current_owner, key.second.txid}));
            }
            removed_points.emplace_back(key.second.txid);
        }
        batch.Erase(key);
        cursor->Next();
    }
    return WriteBatch(batch);
}

bool MapPointIndex::DB::WriteTransfer(const TransferKey& key, const TransferRecord& record)
{
    CDBBatch batch(*this);
    batch.Write(std::make_pair(DB_TRANSFER, key), record);
    batch.Write(std::make_pair(DB_TRANSFER_HEIGHT, TransferHeightKey{record.height, key.origin, key.transfer}), uint8_t{0});
    return WriteBatch(batch);
}

bool MapPointIndex::DB::ReadTransfers(const uint256& origin, std::vector<MapPointTransferInfo>& out) const
{
    auto cursor = std::unique_ptr<CDBIterator>(const_cast<MapPointIndex::DB*>(this)->NewIterator());
    cursor->Seek(std::make_pair(DB_TRANSFER, TransferKey{origin, uint256{}}));
    while (cursor->Valid()) {
        std::pair<uint8_t, TransferKey> key;
        if (!cursor->GetKey(key) || key.first != DB_TRANSFER) {
            break;
        }
        if (key.second.origin != origin) {
            break;
        }
        TransferRecord record;
        if (cursor->GetValue(record)) {
            MapPointTransferInfo info;
            info.transfer_txid = key.second.transfer;
            info.height = static_cast<int>(record.height);
            info.new_owner = record.new_owner;
            out.emplace_back(std::move(info));
        }
        cursor->Next();
    }
    std::sort(out.begin(), out.end(), [](const MapPointTransferInfo& a, const MapPointTransferInfo& b) {
        if (a.height == b.height) return a.transfer_txid < b.transfer_txid;
        return a.height < b.height;
    });
    return true;
}

bool MapPointIndex::DB::RemoveTransfersAboveHeight(uint32_t height, std::vector<std::pair<uint256, std::string>>& owner_updates)
{
    auto cursor = std::unique_ptr<CDBIterator>(NewIterator());
    cursor->Seek(std::make_pair(DB_TRANSFER_HEIGHT, TransferHeightKey{height + 1, uint256{}, uint256{}}));
    CDBBatch batch(*this);
    const size_t initial_size = owner_updates.size();
    while (cursor->Valid()) {
        std::pair<uint8_t, TransferHeightKey> key;
        if (!cursor->GetKey(key) || key.first != DB_TRANSFER_HEIGHT) {
            break;
        }
        if (key.second.height <= height) {
            break;
        }
        TransferRecord record;
        TransferKey transfer_key{key.second.origin, key.second.transfer};
        if (Read(std::make_pair(DB_TRANSFER, transfer_key), record)) {
            batch.Erase(std::make_pair(DB_TRANSFER, transfer_key));
            owner_updates.emplace_back(key.second.origin, record.previous_owner);
        }
        batch.Erase(key);
        cursor->Next();
    }
    std::reverse(owner_updates.begin() + initial_size, owner_updates.end());
    return WriteBatch(batch);
}

bool MapPointIndex::DB::RemoveAllTransfersForOrigin(const uint256& origin)
{
    auto cursor = std::unique_ptr<CDBIterator>(NewIterator());
    cursor->Seek(std::make_pair(DB_TRANSFER, TransferKey{origin, uint256{}}));
    CDBBatch batch(*this);
    while (cursor->Valid()) {
        std::pair<uint8_t, TransferKey> key;
        if (!cursor->GetKey(key) || key.first != DB_TRANSFER) {
            break;
        }
        if (key.second.origin != origin) {
            break;
        }
        TransferRecord record;
        if (cursor->GetValue(record)) {
            batch.Erase(std::make_pair(DB_TRANSFER_HEIGHT, TransferHeightKey{record.height, key.second.origin, key.second.transfer}));
        }
        batch.Erase(key);
        cursor->Next();
    }
    return WriteBatch(batch);
}

MapPointIndex::MapPointIndex(size_t cache_size, bool memory, bool wipe, bool obfuscate) :
    m_db(std::make_unique<DB>(cache_size, memory, wipe, obfuscate)),
    m_cache_size(cache_size),
    m_memory(memory),
    m_obfuscate(obfuscate)
{}

MapPointIndex::~MapPointIndex() = default;

namespace {

struct PendingTransfer {
    uint256 origin;
    uint256 transfer_txid;
    uint32_t height{0};
    std::string new_owner;
    std::string prev_owner;
};

} // namespace

bool MapPointIndex::WriteBlock(const CBlock& block, const CBlockIndex* pindex)
{
    std::map<uint256, Record> pending_points;
    std::vector<PendingTransfer> pending_transfers;

    bool undo_loaded = false;
    CBlockUndo block_undo;

    for (size_t tx_index = 0; tx_index < block.vtx.size(); ++tx_index) {
        const CTransaction& tx = *block.vtx[tx_index];

        Record record;
        if (ExtractRecord(tx, record)) {
            record.height = static_cast<uint32_t>(pindex->nHeight);
            pending_points.emplace(tx.GetHash(), record);
            continue;
        }

        std::string payload;
        uint256 origin_txid;
        if (!tx.IsCoinBase()) {
            for (const auto& txout : tx.vout) {
                if (!txout.scriptPubKey.IsUnspendable()) {
                    continue;
                }
                if (ExtractOpReturnData(txout.scriptPubKey, payload)) {
                    break;
                }
            }
        }
        if (payload.empty()) {
            continue;
        }
        if (!mappoint::ParseTransferPayload(payload, origin_txid)) {
            continue;
        }

        if (!undo_loaded) {
            if (pindex->GetUndoPos().IsNull() || !UndoReadFromDisk(block_undo, pindex)) {
                LogPrint(BCLog::LEVELDB, "MapPointIndex: failed to load undo data for block %s\n", pindex->GetBlockHash().ToString());
                return false;
            }
            undo_loaded = true;
        }
        if (tx_index == 0 || tx_index - 1 >= block_undo.vtxundo.size()) {
            continue;
        }

        const CTxUndo& tx_undo = block_undo.vtxundo[tx_index - 1];

        std::string prev_owner;
        if (const auto it = pending_points.find(origin_txid); it != pending_points.end()) {
            prev_owner = it->second.current_owner;
        } else {
            Record existing;
            if (!m_db->ReadPoint(origin_txid, existing)) {
                continue;
            }
            prev_owner = existing.current_owner;
        }
        if (prev_owner.empty()) {
            continue;
        }

        bool owns_input = false;
        for (size_t vin_index = 0; vin_index < tx.vin.size() && vin_index < tx_undo.vprevout.size(); ++vin_index) {
            const Coin& coin = tx_undo.vprevout[vin_index];
            if (coin.out.scriptPubKey.IsUnspendable()) {
                continue;
            }
            CTxDestination dest;
            if (ExtractDestination(coin.out.scriptPubKey, dest) && EncodeDestination(dest) == prev_owner) {
                owns_input = true;
                break;
            }
        }
        if (!owns_input) {
            continue;
        }

        std::string new_owner;
        if (!ExtractOwnerAddress(tx, new_owner)) {
            continue;
        }
        if (new_owner.empty() || new_owner == prev_owner) {
            continue;
        }

        if (const auto it = pending_points.find(origin_txid); it != pending_points.end()) {
            it->second.current_owner = new_owner;
        }

        pending_transfers.push_back({origin_txid, tx.GetHash(), static_cast<uint32_t>(pindex->nHeight), new_owner, prev_owner});
    }

    if (!pending_points.empty()) {
        std::vector<std::pair<uint256, Record>> creations;
        creations.reserve(pending_points.size());
        for (auto& entry : pending_points) {
            creations.emplace_back(entry.first, entry.second);
        }
        if (!m_db->WritePoints(creations)) {
            return false;
        }
    }

    for (const PendingTransfer& transfer : pending_transfers) {
        Record record;
        if (!m_db->ReadPoint(transfer.origin, record)) {
            continue;
        }
        const std::string current_owner = record.current_owner;
        record.current_owner = transfer.new_owner;
        if (!m_db->WriteRecord(transfer.origin, record)) {
            return false;
        }
        if (!m_db->UpdateOwnerIndex(current_owner, transfer.new_owner, transfer.origin)) {
            return false;
        }
        TransferRecord rec{transfer.height, transfer.new_owner, transfer.prev_owner};
        if (!m_db->WriteTransfer(TransferKey{transfer.origin, transfer.transfer_txid}, rec)) {
            return false;
        }
    }

    return true;
}

bool MapPointIndex::Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip)
{
    std::vector<std::pair<uint256, std::string>> owner_updates;
    if (!m_db->RemoveTransfersAboveHeight(new_tip->nHeight, owner_updates)) {
        return false;
    }
    for (const auto& update : owner_updates) {
        Record record;
        if (!m_db->ReadPoint(update.first, record)) {
            continue;
        }
        const std::string current_owner = record.current_owner;
        record.current_owner = update.second;
        if (!m_db->WriteRecord(update.first, record)) {
            return false;
        }
        if (!m_db->UpdateOwnerIndex(current_owner, update.second, update.first)) {
            return false;
        }
    }

    std::vector<uint256> removed_points;
    if (!m_db->ErasePointsAboveHeight(new_tip->nHeight, removed_points)) {
        return false;
    }
    for (const uint256& origin : removed_points) {
        if (!m_db->RemoveAllTransfersForOrigin(origin)) {
            return false;
        }
    }

    return BaseIndex::Rewind(current_tip, new_tip);
}

BaseIndex::DB& MapPointIndex::GetDB() const { return *m_db; }

bool MapPointIndex::ExtractRecord(const CTransaction& tx, Record& record)
{
    if (tx.IsCoinBase()) {
        return false;
    }
    std::string payload;
    for (const auto& txout : tx.vout) {
        if (!txout.scriptPubKey.IsUnspendable()) {
            continue;
        }
        if (ExtractOpReturnData(txout.scriptPubKey, payload)) {
            break;
        }
    }
    if (payload.empty()) {
        return false;
    }
    int64_t enc_lat{0};
    int64_t enc_lon{0};
    if (!mappoint::ParsePayload(payload, enc_lat, enc_lon)) {
        return false;
    }
    std::string owner;
    if (!ExtractOwnerAddress(tx, owner)) {
        return false;
    }
    record.encoded_lat = enc_lat;
    record.encoded_lon = enc_lon;
    record.origin_owner = owner;
    record.current_owner = owner;
    return true;
}

bool MapPointIndex::GetPoint(const uint256& txid, MapPointInfo& out) const
{
    Record record;
    if (!m_db->ReadPoint(txid, record)) {
        return false;
    }
    out = MapPointIndex::DB::MakeInfo(txid, record);
    out.transfers = GetTransfers(txid);
    return true;
}

std::vector<MapPointInfo> MapPointIndex::GetPointsForOwner(const std::vector<std::string>& owners,
                                                           int from_height,
                                                           int to_height) const
{
    std::vector<MapPointInfo> result;
    if (owners.empty()) {
        return result;
    }
    uint32_t from = std::max(0, from_height);
    uint32_t to = to_height < 0 ? std::numeric_limits<uint32_t>::max() : static_cast<uint32_t>(to_height);
    m_db->ReadOwners(owners, from, to, result);
    return result;
}

std::vector<MapPointInfo> MapPointIndex::GetPointsInHeightRange(int from_height,
                                                                int to_height) const
{
    std::vector<MapPointInfo> result;
    uint32_t from = std::max(0, from_height);
    uint32_t to = to_height < 0 ? std::numeric_limits<uint32_t>::max() : static_cast<uint32_t>(to_height);
    m_db->ReadByHeight(from, to, result);
    return result;
}

std::vector<MapPointTransferInfo> MapPointIndex::GetTransfers(const uint256& txid) const
{
    std::vector<MapPointTransferInfo> transfers;
    m_db->ReadTransfers(txid, transfers);
    return transfers;
}

bool MapPointIndex::Rebuild()
{
    Interrupt();
    Stop();
    m_db = std::make_unique<DB>(m_cache_size, m_memory, true, m_obfuscate);
    if (!m_chainstate) {
        return false;
    }
    return Start(*Assert(m_chainstate));
}

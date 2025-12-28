// Copyright (c) 2025 The Orin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ORIN_INDEX_MAPPOINTINDEX_H
#define ORIN_INDEX_MAPPOINTINDEX_H

#include <index/base.h>
#include <util/mappoint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class CBlock;
class CDBIterator;
class CScript;
class CTransaction;
class uint256;

struct MapPointTransferInfo {
    uint256 transfer_txid;
    int height{0};
    std::string new_owner;
};

struct MapPointInfo {
    uint256 origin_txid;
    int origin_height{0};
    std::string origin_owner;
    std::string current_owner;
    int64_t encoded_lat{0};
    int64_t encoded_lon{0};
    std::vector<MapPointTransferInfo> transfers;

    double Latitude() const { return mappoint::DecodeCoordinate(encoded_lat); }
    double Longitude() const { return mappoint::DecodeCoordinate(encoded_lon); }
};

class MapPointIndex final : public BaseIndex
{
private:
    struct Record {
        uint32_t height{0};
        std::string origin_owner;
        std::string current_owner;
        int64_t encoded_lat{0};
        int64_t encoded_lon{0};

        SERIALIZE_METHODS(Record, obj)
        {
            READWRITE(obj.height, obj.origin_owner, obj.current_owner, obj.encoded_lat, obj.encoded_lon);
        }
    };

    struct TransferRecord {
        uint32_t height{0};
        std::string new_owner;
        std::string previous_owner;

        SERIALIZE_METHODS(TransferRecord, obj)
        {
            READWRITE(obj.height, obj.new_owner, obj.previous_owner);
        }
    };

    class DB;

    std::unique_ptr<DB> m_db;
    size_t m_cache_size;
    bool m_memory;
    bool m_obfuscate;

    bool AllowPrune() const override { return true; }
    bool WriteBlock(const CBlock& block, const CBlockIndex* pindex) override;
    bool Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip) override;
    BaseIndex::DB& GetDB() const override;
    const char* GetName() const override { return "mappointindex"; }

    static bool ExtractRecord(const CTransaction& tx, Record& record);

public:
    MapPointIndex(size_t cache_size, bool memory = false, bool wipe = false, bool obfuscate = false);
    ~MapPointIndex() override;

    bool GetPoint(const uint256& txid, MapPointInfo& out) const;
    std::vector<MapPointInfo> GetPointsForOwner(const std::vector<std::string>& owners,
                                                int from_height,
                                                int to_height) const;
    std::vector<MapPointInfo> GetPointsInHeightRange(int from_height,
                                                     int to_height) const;
    std::vector<MapPointTransferInfo> GetTransfers(const uint256& txid) const;

    /** Drop the index database and rebuild it from the active chain. */
    bool Rebuild();
};

extern MapPointIndex* g_mappoint_index;

static const bool DEFAULT_MAPPOINTINDEX = true;

#endif // ORIN_INDEX_MAPPOINTINDEX_H

// Copyright (c) 2025 The Orin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/mappoint.h>

#include <tinyformat.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace mappoint {

static bool EncodeCoordinate(double value, double max_abs, int64_t& out, std::string& error)
{
    if (!std::isfinite(value)) {
        error = "Coordinate must be a finite number";
        return false;
    }
    if (value < -max_abs || value > max_abs) {
        error = strprintf("Coordinate %.8f out of range [-%.0f, %.0f]", value, max_abs, max_abs);
        return false;
    }
    out = llround(value * COORD_SCALE);
    return true;
}

bool EncodeCoordinates(double lat,
                       double lon,
                       int64_t& encoded_lat,
                       int64_t& encoded_lon,
                       std::string& error)
{
    if (!EncodeCoordinate(lat, MAX_LATITUDE, encoded_lat, error)) {
        return false;
    }
    if (!EncodeCoordinate(lon, MAX_LONGITUDE, encoded_lon, error)) {
        return false;
    }
    return true;
}

std::string BuildPayload(int64_t encoded_lat, int64_t encoded_lon)
{
    return strprintf("%s:%d:%d", MAP_POINT_PREFIX, encoded_lat, encoded_lon);
}

bool ParsePayload(const std::string& payload,
                  int64_t& encoded_lat,
                  int64_t& encoded_lon)
{
    if (payload.size() < sizeof(MAP_POINT_PREFIX)) {
        return false;
    }

    std::vector<std::string> parts = SplitString(payload, ':');
    if (parts.size() != 3) {
        return false;
    }
    if (parts[0] != MAP_POINT_PREFIX) {
        return false;
    }
    if (!ParseInt64(parts[1], &encoded_lat)) {
        return false;
    }
    if (!ParseInt64(parts[2], &encoded_lon)) {
        return false;
    }
    if (std::llabs(encoded_lat) > static_cast<int64_t>(MAX_LATITUDE * COORD_SCALE)) {
        return false;
    }
    if (std::llabs(encoded_lon) > static_cast<int64_t>(MAX_LONGITUDE * COORD_SCALE)) {
        return false;
    }
    return true;
}

bool ParseTransferPayload(const std::string& payload, uint256& point_txid)
{
    if (payload.size() <= sizeof(MAP_POINT_TRANSFER_PREFIX)) {
        return false;
    }
    std::vector<std::string> parts = SplitString(payload, ':');
    if (parts.size() != 2) {
        return false;
    }
    if (parts[0] != MAP_POINT_TRANSFER_PREFIX) {
        return false;
    }
    if (parts[1].size() != 64 || !IsHex(parts[1])) {
        return false;
    }
    point_txid.SetHex(parts[1]);
    return true;
}

} // namespace mappoint

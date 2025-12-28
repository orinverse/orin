// Copyright (c) 2025 The Orin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ORIN_UTIL_MAPPOINT_H
#define ORIN_UTIL_MAPPOINT_H

#include <cstdint>
#include <string>
#include <uint256.h>

namespace mappoint {

static constexpr const char MAP_POINT_PREFIX[] = "ORINMAP1";
static constexpr const char MAP_POINT_TRANSFER_PREFIX[] = "ORINMAPX";
static constexpr double COORD_SCALE = 1'000'000.0;
static constexpr double MAX_LATITUDE = 90.0;
static constexpr double MAX_LONGITUDE = 180.0;

/** Convert encoded coordinate to floating point representation. */
inline double DecodeCoordinate(int64_t encoded)
{
    return static_cast<double>(encoded) / COORD_SCALE;
}

/**
 * Encode latitude and longitude into their scaled integer representation.
 * Returns false and sets \a error on invalid coordinates (non finite or out
 * of the accepted range).
 */
bool EncodeCoordinates(double lat,
                       double lon,
                       int64_t& encoded_lat,
                       int64_t& encoded_lon,
                       std::string& error);

/** Build the OP_RETURN payload string using encoded coordinates. */
std::string BuildPayload(int64_t encoded_lat, int64_t encoded_lon);

/**
 * Parse a payload string (without surrounding script opcodes) and extract
 * encoded coordinates.
 */
bool ParsePayload(const std::string& payload,
                  int64_t& encoded_lat,
                  int64_t& encoded_lon);

bool ParseTransferPayload(const std::string& payload, uint256& point_txid);

} // namespace mappoint

#endif // ORIN_UTIL_MAPPOINT_H

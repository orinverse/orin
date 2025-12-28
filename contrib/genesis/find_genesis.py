#!/usr/bin/env python3
"""
Utility script to (re)generate Orin genesis block parameters.

Example:
  python3 contrib/genesis/find_genesis.py \
    --timestamp "Orin 13/Nov/2025 Rebooting the chain for privacy-first payments" \
    --pubkey 042b55887b34dfaca197bd9e2e965f56086979daf2ebc2a9c85fa72ba83d34e916e6b5e6c58c82becc828e7bb2a45a005b47f80969bd77094ff826a9000cc72c55
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from dataclasses import dataclass
from typing import Iterable, Tuple

COIN = 100_000_000


def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


def dsha256(data: bytes) -> bytes:
    return sha256(sha256(data))


def encode_script_num(value: int) -> bytes:
    if value == 0:
        return b""
    neg = value < 0
    value = abs(value)
    out = bytearray()
    while value:
        out.append(value & 0xFF)
        value >>= 8
    if out[-1] & 0x80:
        out.append(0x80 if neg else 0)
    elif neg:
        out[-1] |= 0x80
    return bytes(out)


def push_data(payload: bytes) -> bytes:
    size = len(payload)
    if size < 0x4C:
        return bytes([size]) + payload
    if size <= 0xFF:
        return b"\x4c" + bytes([size]) + payload
    if size <= 0xFFFF:
        return b"\x4d" + struct.pack("<H", size) + payload
    return b"\x4e" + struct.pack("<I", size) + payload


def encode_varint(value: int) -> bytes:
    if value < 0xFD:
        return bytes([value])
    if value <= 0xFFFF:
        return b"\xfd" + struct.pack("<H", value)
    if value <= 0xFFFFFFFF:
        return b"\xfe" + struct.pack("<I", value)
    return b"\xff" + struct.pack("<Q", value)


def build_coinbase(timestamp: bytes, pubkey: bytes, reward: int) -> Tuple[bytes, bytes]:
    script_sig = (
        push_data(encode_script_num(486604799)) +
        push_data(encode_script_num(4)) +
        push_data(timestamp)
    )
    tx = bytearray()
    tx += struct.pack("<I", 1)                     # version
    tx += encode_varint(1)                         # vin count
    tx += b"\x00" * 32                             # prevout hash
    tx += struct.pack("<I", 0xFFFFFFFF)            # prevout index
    tx += encode_varint(len(script_sig)) + script_sig
    tx += struct.pack("<I", 0xFFFFFFFF)            # sequence
    tx += encode_varint(1)                         # vout count
    tx += struct.pack("<Q", reward)
    script_pubkey = push_data(pubkey) + b"\xAC"     # OP_CHECKSIG
    tx += encode_varint(len(script_pubkey)) + script_pubkey
    tx += struct.pack("<I", 0)                     # locktime
    tx_bytes = bytes(tx)
    merkle = dsha256(tx_bytes)
    return tx_bytes, merkle


def bits_to_target(bits: int) -> int:
    exponent = bits >> 24
    mantissa = bits & 0xFFFFFF
    return mantissa * (1 << (8 * (exponent - 3)))


@dataclass
class NetworkSpec:
    name: str
    start_time: int
    bits: int
    start_nonce: int = 0


def mine_genesis(merkle: bytes, spec: NetworkSpec) -> Tuple[int, int, str]:
    target = bits_to_target(spec.bits)
    n_time = spec.start_time
    nonce = spec.start_nonce
    while True:
        header = bytearray()
        header += struct.pack("<I", 1)      # version
        header += b"\x00" * 32             # prev hash
        header += merkle[::-1]             # merkle root (internal little endian)
        header += struct.pack("<I", n_time)
        header += struct.pack("<I", spec.bits)
        header += struct.pack("<I", nonce)
        hash_bytes = dsha256(bytes(header))
        hash_int = int.from_bytes(hash_bytes[::-1], "big")
        if hash_int <= target:
            return n_time, nonce, hash_bytes[::-1].hex()
        nonce = (nonce + 1) & 0xFFFFFFFF
        if nonce == 0:
            n_time += 1


def parse_network_args(values: Iterable[str]) -> Iterable[NetworkSpec]:
    for item in values:
        parts = item.split(":")
        if len(parts) < 3:
            raise ValueError(f"Invalid network spec '{item}'. Expected format name:time:bits[:nonce]")
        name = parts[0]
        n_time = int(parts[1], 0)
        bits = int(parts[2], 0)
        start_nonce = int(parts[3], 0) if len(parts) > 3 else 0
        yield NetworkSpec(name, n_time, bits, start_nonce)


def main() -> None:
    parser = argparse.ArgumentParser(description="Genesis block finder for Orin-based chains.")
    parser.add_argument("--timestamp", required=True, help="ASCII string placed inside the coinbase scriptSig")
    parser.add_argument("--pubkey", required=True, help="Hex-encoded uncompressed public key for the genesis output")
    parser.add_argument("--reward", type=float, default=50.0, help="Block subsidy in coins (default: 50)")
    parser.add_argument(
        "--net",
        action="append",
        dest="nets",
        default=[],
        help="Network spec in the form name:time:bits[:nonce]. "
             "Bits can be specified in hex with 0x prefix. Repeat for multiple networks.",
    )
    args = parser.parse_args()

    if not args.nets:
        args.nets = [
            "main:1390009218:0x1e0ffff0",
            "test:1390579806:0x1e0ffff0",
            "regtest:1417626937:0x207fffff",
        ]

    timestamp_bytes = args.timestamp.encode("utf-8")
    pubkey_bytes = bytes.fromhex(args.pubkey)
    reward_satoshis = int(args.reward * COIN)
    _, merkle = build_coinbase(timestamp_bytes, pubkey_bytes, reward_satoshis)

    print(f"Merkle root: {merkle[::-1].hex()}")
    for spec in parse_network_args(args.nets):
        n_time, nonce, block_hash = mine_genesis(merkle, spec)
        print(f"[{spec.name}] time={n_time} nonce={nonce} hash={block_hash}")


if __name__ == "__main__":
    main()

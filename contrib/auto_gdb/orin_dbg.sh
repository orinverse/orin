#!/usr/bin/env bash
# Copyright (c) 2018-2023 The Orin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
# use testnet settings,  if you need mainnet,  use ~/.orincore/orind.pid file instead
export LC_ALL=C

orin_pid="$(<~/.orincore/testnet3/orind.pid)"
sudo gdb -batch -ex "source debug.gdb" orind "${orin_pid}"

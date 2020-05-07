// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef CONSENSUS_AUXPOW_H
#define CONSENSUS_AUXPOW_H

namespace AuxPow {

const int START_MAINNET = 10000;
const int START_TESTNET = 10000;
const int START_REGTEST = 10000;

const int CHAIN_ID = 0xa6;

enum
{
    // primary version
    BLOCK_VERSION_DEFAULT        = (1 << 0),

    // modifiers
    BLOCK_VERSION_AUXPOW         = (1 << 8),

    // bits allocated for chain ID
    BLOCK_VERSION_CHAIN_START    = (1 << 16),
    BLOCK_VERSION_CHAIN_END      = (1 << 30),
};
} // namespace AuxPow

#endif // CONSENSUS_AUXPOW_H

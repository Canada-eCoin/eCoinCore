// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include "arith_uint256.h"
#include "chain.h"
#include "util.h"
#include "auxpow/check.h"

unsigned int CalculateNextWorkRequired_V1(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    
    //if (nActualTimespan < params.nPowTargetTimespan/4)
    //    nActualTimespan = params.nPowTargetTimespan/4;
    //if (nActualTimespan > params.nPowTargetTimespan*4)
    //    nActualTimespan = params.nPowTargetTimespan*4;

    // This will only run in the first 500 blocks
    if (nActualTimespan <  30 / 4)
        nActualTimespan = 30 / 4;
    if (nActualTimespan > 30 *4)
        nActualTimespan = 30 *4;    

    // Retarget
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    // Canada eCoin: intermediate uint256 can overflow by 1 bit
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;
    if (fShift)
        bnNew <<= 1;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired_V1(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    // Canada eCoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = params.DifficultyAdjustmentInterval()-1;
    if ((pindexLast->nHeight+1) != params.DifficultyAdjustmentInterval())
        blockstogoback = params.DifficultyAdjustmentInterval();

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;

    assert(pindexFirst);

    return CalculateNextWorkRequired_V1(pindexLast, pindexFirst->GetBlockTime(), params);
}

// AntiGravityWave by reorder, derived from code by Evan Duffield - evan@darkcoin.io
unsigned int static AntiGravityWave(int64_t version, const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    int64_t nActualTimespan = 0;
    int64_t LastBlockTime = 0;
    int64_t PastBlocksMin = 24;
    int64_t PastBlocksMax = 24;
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    if (version == 1) {
        PastBlocksMin = 24;
        PastBlocksMax = 24;
    } else if (version == 2) {
        PastBlocksMin = 72;
        PastBlocksMax = 72;
    } else if (version == 3) {
        PastBlocksMin = 144;
        PastBlocksMax = 144;
    }

    int64_t CountBlocks = 0;
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || BlockLastSolved->nHeight < PastBlocksMin) {
            return nProofOfWorkLimit;
    }

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (PastBlocksMax > 0 && i > PastBlocksMax) { break; }
        CountBlocks++;

        if (CountBlocks <= PastBlocksMin) {
            if (CountBlocks == 1)
                PastDifficultyAverage.SetCompact(BlockReading->nBits);
            else { PastDifficultyAverage = ((PastDifficultyAveragePrev * CountBlocks)+(arith_uint256().SetCompact(BlockReading->nBits))) / (CountBlocks+1); }
                PastDifficultyAveragePrev = PastDifficultyAverage;
        }

        if (LastBlockTime > 0) {
            int64_t Diff = (LastBlockTime - BlockReading->GetBlockTime());
            nActualTimespan += Diff;
        }
        LastBlockTime = BlockReading->GetBlockTime();

        if (BlockReading->pprev == NULL) { assert(BlockReading); break; }
        BlockReading = BlockReading->pprev;
    }

    arith_uint256 bnNew(PastDifficultyAverage);

    if (version == 3)
        --CountBlocks;

    int64_t nTargetTimespan;
    if( BlockLastSolved->nHeight + 1 < 500 ) {
        nTargetTimespan = CountBlocks * 30; // 30 sec target time
    } else {
        nTargetTimespan = CountBlocks * params.nPowTargetSpacing;
    }

    int64_t div = 4;

    if (version == 0 || version == 1)
        div = 4;
    else if (version == 2)
        div = 3;
    else if (version == 3)
        div = 2;

    if (nActualTimespan < nTargetTimespan/div)
        nActualTimespan = nTargetTimespan/div;
    if (nActualTimespan > nTargetTimespan*div)
        nActualTimespan = nTargetTimespan*div;

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    if (bnNew > bnPowLimit) {
        bnNew = bnPowLimit;
    }

    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    // -regtest mode
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    if (pindexLast->nHeight+1 >= 2000 || (params.fPowAllowMinDifficultyBlocks && pindexLast->nHeight+1 >= 300000)) {
        return AntiGravityWave(3, pindexLast, pblock, params);
    } else if (pindexLast->nHeight+1 >= 1500) {
        return AntiGravityWave(2, pindexLast, pblock, params);
    } else if (pindexLast->nHeight+1 >= 1000) {
        return AntiGravityWave(1, pindexLast, pblock, params);
    } else if (pindexLast->nHeight+1 >= 2) {
        return AntiGravityWave(0, pindexLast, pblock, params);
    } else {
        return GetNextWorkRequired_V1(pindexLast, pblock, params);
    }
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    return UintToArith256(hash) <= bnTarget;

}

bool CheckBlockProofOfWork(const CBlockHeader *pblock, const Consensus::Params& params)
{
    // LogPrint("txdb", "CheckBlockProofOfWork(): block: %s\n", pblock->ToString());  // LEDTMP

    if (pblock->auxpow && (pblock->auxpow.get() != nullptr))
    {
        if (!CheckAuxpow(pblock->auxpow, pblock->GetHash(), pblock->GetChainID(), params))
            return error("CheckBlockProofOfWork() : AUX POW is not valid");
        // Check proof of work matches claimed amount
        if (!CheckProofOfWork(pblock->auxpow->GetParentBlockHash(), pblock->nBits, params))
            return error("CheckBlockProofOfWork() : AUX proof of work failed");
    }
    else
    {
        // Check proof of work matches claimed amount
        if (!CheckProofOfWork(pblock->GetPoWHash(), pblock->nBits, params))
            return error("CheckBlockProofOfWork() : proof of work failed");
    }
    return true;
}

bool CheckAuxPowValidity(const CBlockHeader* pblock, const Consensus::Params& params)
{
    if (!params.fPowAllowMinDifficultyBlocks)
    {
        if (pblock->GetChainID() != AuxPow::CHAIN_ID)
            return error("CheckAuxPowValidity() : block does not have our chain ID");
    }
    return true;
}

// TODO LED TMP temporary public interface for passing the build of test/pow_tests.cpp only
// TODO LED TMP this code should be removed and test/pow_test.cpp changed to call
// TODO LED TMP our interface to PoW --> GetNextWorkRequired()
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    return CalculateNextWorkRequired_V1(pindexLast, nFirstBlockTime, params);
}

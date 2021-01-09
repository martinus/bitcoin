// Copyright (c) 2015-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <bench/bench.h>
#include <script/interpreter.h>
#include <test/util/setup_common.h>

void static RandomScript(CScript& script)
{
    static const opcodetype oplist[] = {OP_FALSE, OP_1, OP_2, OP_3, OP_CHECKSIG, OP_IF, OP_VERIF, OP_RETURN, OP_CODESEPARATOR};
    script = CScript();
    int ops = (InsecureRandRange(10));
    for (int i = 0; i < ops; i++)
        script << oplist[InsecureRandRange(sizeof(oplist) / sizeof(oplist[0]))];
}

void static RandomTransaction(CMutableTransaction& tx, bool fSingle)
{
    tx.nVersion = InsecureRand32();
    tx.nLockTime = (InsecureRandBool()) ? InsecureRand32() : 0;
    int ins = 1;
    int outs = fSingle ? ins : (InsecureRandBits(2)) + 1;
    for (int in = 0; in < ins; in++) {
        tx.vin.push_back(CTxIn());
        CTxIn& txin = tx.vin.back();
        txin.prevout.hash = InsecureRand256();
        txin.prevout.n = InsecureRandBits(2);
        RandomScript(txin.scriptSig);
        txin.nSequence = (InsecureRandBool()) ? InsecureRand32() : std::numeric_limits<uint32_t>::max();
    }
    for (int out = 0; out < outs; out++) {
        tx.vout.push_back(CTxOut());
        CTxOut& txout = tx.vout.back();
        txout.nValue = InsecureRandRange(100000000);
        RandomScript(txout.scriptPubKey);
    }
}

static void SignatureHashBench(benchmark::Bench& bench)
{
    // make sure we always benchmark exactly the same random transaction
    g_insecure_rand_ctx = FastRandomContext(ArithToUint256(arith_uint256(33)));

    int nHashType = InsecureRand32();
    CMutableTransaction txTo;
    RandomTransaction(txTo, (nHashType & 0x1f) == SIGHASH_SINGLE);
    CScript scriptCode;
    RandomScript(scriptCode);

    bench.run([&] {
        unsigned nIn = 0U;
        auto sh = SignatureHash(scriptCode, txTo, nIn, nHashType, 0, SigVersion::BASE);
        ankerl::nanobench::doNotOptimizeAway(sh);
    });
}

BENCHMARK(SignatureHashBench);

// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <consensus/tx_check.h>
#include <primitives/transaction.h>

#include <random.h>


static void HasDuplicateInputsBench(benchmark::Bench& bench)
{
    // prepare a few random inputs
    auto rng = FastRandomContext(/*fDeterministic=*/true);
    auto vin = std::vector<CTxIn>();
    int numBits = 13;
    vin.reserve(1U << numBits);
    for (size_t i = 0; i < (1U << numBits); ++i) {
        auto hash = rng.rand256();
        auto n = static_cast<uint32_t>(rng.randrange(10));
        vin.emplace_back(COutPoint{hash, n});
    }

    bench.run([&] {
        auto randBits = rng.randrange(numBits);
        auto numInputs = rng.randrange(1U << randBits);
        if (numInputs == 0) {
            ++numInputs;
        }
        auto inputs = Span<CTxIn>(vin.data(), vin.data() + numInputs);
        ankerl::nanobench::doNotOptimizeAway(HasDuplicateInputs(inputs));
    });
}

BENCHMARK(HasDuplicateInputsBench, benchmark::PriorityLevel::HIGH);

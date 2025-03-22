// Copyright (c) 2019-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_HASHER_H
#define BITCOIN_UTIL_HASHER_H

#include <crypto/common.h>
#include <crypto/siphash.h>
#include <primitives/transaction.h>
#include <span.h>
#include <uint256.h>

#include <cstdint>
#include <cstring>

class SaltedTxidHasher
{
private:
    /** Salt */
    const uint64_t k0, k1;

public:
    SaltedTxidHasher();

    size_t operator()(const uint256& txid) const {
        return SipHashUint256(k0, k1, txid);
    }
};

class SaltedOutpointHasher
{
private:
    /** Salt */
    const uint64_t k0, k1;

public:
    SaltedOutpointHasher(bool deterministic = false);

    /**
     * Having the hash noexcept allows libstdc++'s unordered_map to recalculate
     * the hash during rehash, so it does not have to cache the value. This
     * reduces node's memory by sizeof(size_t). The required recalculation has
     * a slight performance penalty (around 1.6%), but this is compensated by
     * memory savings of about 9% which allow for a larger dbcache setting.
     *
     * @see https://gcc.gnu.org/onlinedocs/gcc-13.2.0/libstdc++/manual/manual/unordered_associative.html
     */
    size_t operator()(const COutPoint& id) const noexcept {
        auto const rapid_mum = [](uint64_t* a, uint64_t* b) {
#if defined(__SIZEOF_INT128__)
            __uint128_t r = *a;
            r *= *b;
            *a = (uint64_t)r;
            *b = (uint64_t)(r >> 64);
#elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64))
#if defined(_M_X64)
            *a = _umul128(*a, *b, b);
#else
            uint64_t c = __umulh(*a, *b);
            *a = *a * *b;
            *b = c;
#endif
#else
            uint64_t ha = *a >> 32, hb = *b >> 32, la = (uint32_t)*a, lb = (uint32_t)*b, hi, lo;
            uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb, t = rl + (rm0 << 32), c = t < rl;
            lo = t + (rm1 << 32);
            c += lo < t;
            hi = rh + (rm0 >> 32) + (rm1 >> 32) + c;
            *a = lo;
            *b = hi;
#endif
        };

        auto const rapid_mix = [&rapid_mum](uint64_t a, uint64_t b) -> uint64_t {
            rapid_mum(&a, &b);
            return a ^ b;
        };

        // return SipHashUint256Extra(k0, k1, id.hash, id.n);
        // (32+4) + 8+8 = 52 byte (6.5 uint64_t)
        // p0 = id.G
        uint256 const& h = id.hash;

        // Default secret parameters.
        static constexpr uint64_t secret[3] = {0x2d358dccaa6c78a5ull, 0x8bb84b93962eacc9ull, 0x4b33a62ed433d4a3ull};
        static constexpr uint64_t len = 48;
        uint64_t seed = id.n;

        seed ^= rapid_mix(seed ^ secret[0], secret[1]) ^ len;
        seed = rapid_mix(h.GetUint64(0) ^ secret[2], h.GetUint64(1) ^ seed ^ secret[1]);
        seed = rapid_mix(h.GetUint64(2) ^ secret[2], h.GetUint64(3) ^ seed);
        uint64_t a = k0 ^ secret[1];
        uint64_t b = k1 ^ seed;
        rapid_mum(&a, &b);
        return rapid_mix(a ^ secret[0] ^ len, b ^ secret[1]);
    }
};

struct FilterHeaderHasher
{
    size_t operator()(const uint256& hash) const { return ReadLE64(hash.begin()); }
};

/**
 * We're hashing a nonce into the entries themselves, so we don't need extra
 * blinding in the set hash computation.
 *
 * This may exhibit platform endian dependent behavior but because these are
 * nonced hashes (random) and this state is only ever used locally it is safe.
 * All that matters is local consistency.
 */
class SignatureCacheHasher
{
public:
    template <uint8_t hash_select>
    uint32_t operator()(const uint256& key) const
    {
        static_assert(hash_select <8, "SignatureCacheHasher only has 8 hashes available.");
        uint32_t u;
        std::memcpy(&u, key.begin()+4*hash_select, 4);
        return u;
    }
};

struct BlockHasher
{
    // this used to call `GetCheapHash()` in uint256, which was later moved; the
    // cheap hash function simply calls ReadLE64() however, so the end result is
    // identical
    size_t operator()(const uint256& hash) const { return ReadLE64(hash.begin()); }
};

class SaltedSipHasher
{
private:
    /** Salt */
    const uint64_t m_k0, m_k1;

public:
    SaltedSipHasher();

    size_t operator()(const Span<const unsigned char>& script) const;
};

#endif // BITCOIN_UTIL_HASHER_H

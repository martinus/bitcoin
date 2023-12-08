// Copyright (c) 2012-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>

#include <consensus/consensus.h>
#include <logging.h>
#include <random.h>
#include <util/trace.h>

bool CCoinsView::GetCoin(const COutPoint &outpoint, Coin &coin) const { return false; }
uint256 CCoinsView::GetBestBlock() const { return uint256(); }
std::vector<uint256> CCoinsView::GetHeadBlocks() const { return std::vector<uint256>(); }
bool CCoinsView::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock, bool erase) { return false; }
std::unique_ptr<CCoinsViewCursor> CCoinsView::Cursor() const { return nullptr; }

bool CCoinsView::HaveCoin(const COutPoint &outpoint) const
{
    Coin coin;
    return GetCoin(outpoint, coin);
}

CCoinsViewBacked::CCoinsViewBacked(CCoinsView *viewIn) : base(viewIn) { }
bool CCoinsViewBacked::GetCoin(const COutPoint &outpoint, Coin &coin) const { return base->GetCoin(outpoint, coin); }
bool CCoinsViewBacked::HaveCoin(const COutPoint &outpoint) const { return base->HaveCoin(outpoint); }
uint256 CCoinsViewBacked::GetBestBlock() const { return base->GetBestBlock(); }
std::vector<uint256> CCoinsViewBacked::GetHeadBlocks() const { return base->GetHeadBlocks(); }
void CCoinsViewBacked::SetBackend(CCoinsView &viewIn) { base = &viewIn; }
bool CCoinsViewBacked::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock, bool erase) { return base->BatchWrite(mapCoins, hashBlock, erase); }
std::unique_ptr<CCoinsViewCursor> CCoinsViewBacked::Cursor() const { return base->Cursor(); }
size_t CCoinsViewBacked::EstimateSize() const { return base->EstimateSize(); }

CCoinsViewCache::CCoinsViewCache(CCoinsView* baseIn, bool deterministic) :
    CCoinsViewBacked(baseIn), m_deterministic(deterministic)
{}

size_t CCoinsViewCache::DynamicMemoryUsage() const {
    return memusage::DynamicUsage(cacheCoins.data) + memusage::DynamicUsage(cacheCoins.map) + cachedCoinsUsage;
}

CCoinsMapEntry* CCoinsViewCache::FetchCoin(const COutPoint &outpoint) const {
    auto it = cacheCoins.map.find(outpoint);
    if (it != cacheCoins.map.end())
        return &cacheCoins.data[it->second];
    Coin tmp;
    if (!base->GetCoin(outpoint, tmp))
        return nullptr;
    auto mapEntry = cacheCoins.map.try_emplace(outpoint, cacheCoins.data.size());
    auto& ret = cacheCoins.data.emplace_back(&mapEntry.first->first, std::move(tmp));
    if (ret.second.coin.IsSpent()) {
        // The parent only has an empty entry for this outpoint; we can consider our
        // version as fresh.
        ret.second.flags = CCoinsCacheEntry::FRESH;
    }
    cachedCoinsUsage += ret.second.coin.DynamicMemoryUsage();
    return &ret;
}

bool CCoinsViewCache::GetCoin(const COutPoint &outpoint, Coin &coin) const {
    CCoinsMapEntry* c = FetchCoin(outpoint);
    if (c != nullptr) {
        coin = c->second.coin;
        return !coin.IsSpent();
    }
    return false;
}

void CCoinsViewCache::AddCoin(const COutPoint &outpoint, Coin&& coin, bool possible_overwrite) {
    assert(!coin.IsSpent());
    if (coin.out.scriptPubKey.IsUnspendable()) return;
    auto [it, inserted] = cacheCoins.map.try_emplace(outpoint, cacheCoins.data.size());
    bool fresh = false;
    CCoinsCacheEntry* entry = nullptr;
    if (!inserted) {
        entry = &cacheCoins.data[it->second].second;
        cachedCoinsUsage -= entry->coin.DynamicMemoryUsage();
    } else {
        entry = &cacheCoins.data.emplace_back(&it->first).second;
    }
    if (!possible_overwrite) {
        if (!entry->coin.IsSpent()) {
            throw std::logic_error("Attempted to overwrite an unspent coin (when possible_overwrite is false)");
        }
        // If the coin exists in this cache as a spent coin and is DIRTY, then
        // its spentness hasn't been flushed to the parent cache. We're
        // re-adding the coin to this cache now but we can't mark it as FRESH.
        // If we mark it FRESH and then spend it before the cache is flushed
        // we would remove it from this cache and would never flush spentness
        // to the parent cache.
        //
        // Re-adding a spent coin can happen in the case of a re-org (the coin
        // is 'spent' when the block adding it is disconnected and then
        // re-added when it is also added in a newly connected block).
        //
        // If the coin doesn't exist in the current cache, or is spent but not
        // DIRTY, then it can be marked FRESH.
        fresh = !(entry->flags & CCoinsCacheEntry::DIRTY);
    }
    entry->coin = std::move(coin);
    entry->flags |= CCoinsCacheEntry::DIRTY | (fresh ? CCoinsCacheEntry::FRESH : 0);
    cachedCoinsUsage += entry->coin.DynamicMemoryUsage();
    TRACE5(utxocache, add,
           outpoint.hash.data(),
           (uint32_t)outpoint.n,
           (uint32_t)entry->coin.nHeight,
           (int64_t)entry->coin.out.nValue,
           (bool)entry->coin.IsCoinBase());
}

void CCoinsViewCache::EmplaceCoinInternalDANGER(COutPoint&& outpoint, Coin&& coin) {
    cachedCoinsUsage += coin.DynamicMemoryUsage();
    cacheCoins.map.try_emplace(std::move(outpoint), cacheCoins.data.size());
    cacheCoins.data.emplace_back(std::move(coin), CCoinsCacheEntry::DIRTY);
}

void AddCoins(CCoinsViewCache& cache, const CTransaction &tx, int nHeight, bool check_for_overwrite) {
    bool fCoinbase = tx.IsCoinBase();
    const Txid& txid = tx.GetHash();
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        bool overwrite = check_for_overwrite ? cache.HaveCoin(COutPoint(txid, i)) : fCoinbase;
        // Coinbase transactions can always be overwritten, in order to correctly
        // deal with the pre-BIP30 occurrences of duplicate coinbase transactions.
        cache.AddCoin(COutPoint(txid, i), Coin(tx.vout[i], nHeight, fCoinbase), overwrite);
    }
}

bool CCoinsViewCache::SpendCoin(const COutPoint &outpoint, Coin* moveout) {
    CCoinsMapEntry* entry = FetchCoin(outpoint);
    if (entry == nullptr) return false;
    cachedCoinsUsage -= entry->second.coin.DynamicMemoryUsage();
    TRACE5(utxocache, spent,
           outpoint.hash.data(),
           (uint32_t)outpoint.n,
           (uint32_t)entry->second.coin.nHeight,
           (int64_t)entry->second.coin.out.nValue,
           (bool)entry->second.coin.IsCoinBase());
    if (moveout) {
        *moveout = std::move(entry->second.coin);
    }
    if (entry->second.flags & CCoinsCacheEntry::FRESH) {
        *entry = std::move(cacheCoins.data.back());
        cacheCoins.data.pop_back();
        cacheCoins.map.erase(outpoint);
    } else {
        entry->second.flags |= CCoinsCacheEntry::DIRTY;
        entry->second.coin.Clear();
    }
    return true;
}

static const Coin coinEmpty;

const Coin& CCoinsViewCache::AccessCoin(const COutPoint &outpoint) const {
    CCoinsMapEntry* entry = FetchCoin(outpoint);
    if (entry == nullptr) {
        return coinEmpty;
    } else {
        return entry->second.coin;
    }
}

bool CCoinsViewCache::HaveCoin(const COutPoint &outpoint) const {
    CCoinsMapEntry* entry = FetchCoin(outpoint);
    return (entry != nullptr && !entry->second.coin.IsSpent());
}

bool CCoinsViewCache::HaveCoinInCache(const COutPoint &outpoint) const {
    auto it = cacheCoins.map.find(outpoint);
    if (it == cacheCoins.map.end()) {
        return false;
    }
    return !cacheCoins.data[it->second].second.coin.IsSpent();
}

uint256 CCoinsViewCache::GetBestBlock() const {
    if (hashBlock.IsNull())
        hashBlock = base->GetBestBlock();
    return hashBlock;
}

void CCoinsViewCache::SetBestBlock(const uint256 &hashBlockIn) {
    hashBlock = hashBlockIn;
}

bool CCoinsViewCache::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlockIn, bool erase) {

    for (auto const& it : mapCoins.data) {
        // Ignore non-dirty entries (optimization).
        if (!(it.second.flags & CCoinsCacheEntry::DIRTY)) {
            continue;
        }
        auto itUs = cacheCoins.map.find(*it.first);
        if (itUs == cacheCoins.map.end()) {
            // The parent cache does not have an entry, while the child cache does.
            // We can ignore it if it's both spent and FRESH in the child
            if (!(it.second.flags & CCoinsCacheEntry::FRESH && it.second.coin.IsSpent())) {
                // Create the coin in the parent cache, move the data up
                // and mark it as dirty.
                cacheCoins.map.try_emplace(*it.first, cacheCoins.data.size());
                if (erase) {
                    // The `move` call here is purely an optimization; we rely on the
                    // `mapCoins.erase` call in the `for` expression to actually remove
                    // the entry from the child map.
                    cacheCoins.data.emplace_back(std::move(it.second.coin));
                } else {
                    cacheCoins.data.emplace_back(it.second.coin);
                }
                auto& entry = cacheCoins.data.back().second;
                cachedCoinsUsage += entry.coin.DynamicMemoryUsage();
                entry.flags = CCoinsCacheEntry::DIRTY;
                // We can mark it FRESH in the parent if it was FRESH in the child
                // Otherwise it might have just been flushed from the parent's cache
                // and already exist in the grandparent
                if (it.second.flags & CCoinsCacheEntry::FRESH) {
                    entry.flags |= CCoinsCacheEntry::FRESH;
                }
            }
        } else {
            auto& entry = cacheCoins.data[itUs->second];
            // Found the entry in the parent cache
            if ((it.second.flags & CCoinsCacheEntry::FRESH) && !entry.second.coin.IsSpent()) {
                // The coin was marked FRESH in the child cache, but the coin
                // exists in the parent cache. If this ever happens, it means
                // the FRESH flag was misapplied and there is a logic error in
                // the calling code.
                throw std::logic_error("FRESH flag misapplied to coin that exists in parent cache");
            }

            if ((entry.second.flags & CCoinsCacheEntry::FRESH) && entry.second.coin.IsSpent()) {
                // The grandparent cache does not have an entry, and the coin
                // has been spent. We can just delete it from the parent cache.
                cachedCoinsUsage -= entry.second.coin.DynamicMemoryUsage();
                cacheCoins.map.erase(itUs);
                entry = std::move(cacheCoins.data.back());
                cacheCoins.map[*entry.first] = std::distance(cacheCoins.data.data(), &entry);
            } else {
                // A normal modification.
                cachedCoinsUsage -= entry.second.coin.DynamicMemoryUsage();
                if (erase) {
                    // The `move` call here is purely an optimization; we rely on the
                    // `mapCoins.erase` call in the `for` expression to actually remove
                    // the entry from the child map.
                    entry.second.coin = std::move(it.second.coin);
                } else {
                    entry.second.coin = it.second.coin;
                }
                cachedCoinsUsage += entry.second.coin.DynamicMemoryUsage();
                entry.second.flags |= CCoinsCacheEntry::DIRTY;
                // NOTE: It isn't safe to mark the coin as FRESH in the parent
                // cache. If it already existed and was spent in the parent
                // cache then marking it FRESH would prevent that spentness
                // from being flushed to the grandparent.
            }
        }
    }
    if (erase) {
        mapCoins.data.clear();
        mapCoins.map.clear();
    }
    ////////////////////

    hashBlock = hashBlockIn;
    return true;
}

bool CCoinsViewCache::Flush() {
    bool fOk = base->BatchWrite(cacheCoins, hashBlock, /*erase=*/true);
    if (fOk) {
        if (!cacheCoins.data.empty()) {
            /* BatchWrite must erase all cacheCoins elements when erase=true. */
            throw std::logic_error("Not all cached coins were erased");
        }
        ReallocateCache();
    }
    cachedCoinsUsage = 0;
    return fOk;
}

bool CCoinsViewCache::Sync()
{
    bool fOk = base->BatchWrite(cacheCoins, hashBlock, /*erase=*/false);
    // Instead of clearing `cacheCoins` as we would in Flush(), just clear the
    // FRESH/DIRTY flags of any coin that isn't spent.
    for (auto it = cacheCoins.begin(); it != cacheCoins.end(); ) {
        if (it->second.coin.IsSpent()) {
            cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
            it = cacheCoins.erase(it);
        } else {
            it->second.flags = 0;
            ++it;
        }
    }
    return fOk;
}

void CCoinsViewCache::Uncache(const COutPoint& hash)
{
    CCoinsMap::iterator it = cacheCoins.find(hash);
    if (it != cacheCoins.end() && it->second.flags == 0) {
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
        TRACE5(utxocache, uncache,
               hash.hash.data(),
               (uint32_t)hash.n,
               (uint32_t)it->second.coin.nHeight,
               (int64_t)it->second.coin.out.nValue,
               (bool)it->second.coin.IsCoinBase());
        cacheCoins.erase(it);
    }
}

unsigned int CCoinsViewCache::GetCacheSize() const {
    return cacheCoins.size();
}

bool CCoinsViewCache::HaveInputs(const CTransaction& tx) const
{
    if (!tx.IsCoinBase()) {
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            if (!HaveCoin(tx.vin[i].prevout)) {
                return false;
            }
        }
    }
    return true;
}

void CCoinsViewCache::ReallocateCache()
{
    // Cache should be empty when we're calling this.
    assert(cacheCoins.size() == 0);
    cacheCoins.~CCoinsMap();
    m_cache_coins_memory_resource.~CCoinsMapMemoryResource();
    ::new (&m_cache_coins_memory_resource) CCoinsMapMemoryResource{};
    ::new (&cacheCoins) CCoinsMap{0, SaltedOutpointHasher{/*deterministic=*/m_deterministic}, CCoinsMap::key_equal{}, &m_cache_coins_memory_resource};
}

void CCoinsViewCache::SanityCheck() const
{
    size_t recomputed_usage = 0;
    for (const auto& [_, entry] : cacheCoins) {
        unsigned attr = 0;
        if (entry.flags & CCoinsCacheEntry::DIRTY) attr |= 1;
        if (entry.flags & CCoinsCacheEntry::FRESH) attr |= 2;
        if (entry.coin.IsSpent()) attr |= 4;
        // Only 5 combinations are possible.
        assert(attr != 2 && attr != 4 && attr != 7);

        // Recompute cachedCoinsUsage.
        recomputed_usage += entry.coin.DynamicMemoryUsage();
    }
    assert(recomputed_usage == cachedCoinsUsage);
}

static const size_t MIN_TRANSACTION_OUTPUT_WEIGHT = WITNESS_SCALE_FACTOR * ::GetSerializeSize(CTxOut());
static const size_t MAX_OUTPUTS_PER_BLOCK = MAX_BLOCK_WEIGHT / MIN_TRANSACTION_OUTPUT_WEIGHT;

const Coin& AccessByTxid(const CCoinsViewCache& view, const Txid& txid)
{
    COutPoint iter(txid, 0);
    while (iter.n < MAX_OUTPUTS_PER_BLOCK) {
        const Coin& alternate = view.AccessCoin(iter);
        if (!alternate.IsSpent()) return alternate;
        ++iter.n;
    }
    return coinEmpty;
}

template <typename Func>
static bool ExecuteBackedWrapper(Func func, const std::vector<std::function<void()>>& err_callbacks)
{
    try {
        return func();
    } catch(const std::runtime_error& e) {
        for (const auto& f : err_callbacks) {
            f();
        }
        LogPrintf("Error reading from database: %s\n", e.what());
        // Starting the shutdown sequence and returning false to the caller would be
        // interpreted as 'entry not found' (as opposed to unable to read data), and
        // could lead to invalid interpretation. Just exit immediately, as we can't
        // continue anyway, and all writes should be atomic.
        std::abort();
    }
}

bool CCoinsViewErrorCatcher::GetCoin(const COutPoint &outpoint, Coin &coin) const {
    return ExecuteBackedWrapper([&]() { return CCoinsViewBacked::GetCoin(outpoint, coin); }, m_err_callbacks);
}

bool CCoinsViewErrorCatcher::HaveCoin(const COutPoint &outpoint) const {
    return ExecuteBackedWrapper([&]() { return CCoinsViewBacked::HaveCoin(outpoint); }, m_err_callbacks);
}

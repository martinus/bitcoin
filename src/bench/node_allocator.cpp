// Copyright (c) 2019-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <coins.h>
#include <primitives/transaction.h>
#include <support/allocators/node_allocator/factory.h>

#include <cstring>
#include <list>
#include <memory_resource>
#include <type_traits>
#include <unordered_map>

using namespace std::literals;

template <typename Map>
void BenchFillClearMap(benchmark::Bench& bench, Map& map)
{
    size_t batch_size = 5000;

    // make sure each iteration of the benchmark contains exactly 5000 inserts and one clear.
    // do this at least 10 times so we get reasonable accurate results
    typename Map::key_type key;

    bench.batch(batch_size).minEpochTime(200ms).run([&] {
        for (size_t i = 0; i < batch_size; ++i) {
            // add a random number for better spread in the map
            key.n += 0x967f29d1u;
            map[key];
        }
        map.clear();
    });
}

static void NodeAllocator_StdUnorderedMap(benchmark::Bench& bench)
{
    auto map = std::unordered_map<COutPoint, CCoinsCacheEntry, SaltedOutpointHasher>();
    BenchFillClearMap(bench, map);
}

static void NodeAllocator_StdUnorderedMapWithNodeAllocator(benchmark::Bench& bench)
{
    using Factory = node_allocator::Factory<std::unordered_map<COutPoint, CCoinsCacheEntry, SaltedOutpointHasher>>;
    Factory::MemoryResourceType memory_resource{};
    auto map = Factory::CreateContainer(&memory_resource);
    BenchFillClearMap(bench, map);
}


static void NodeAllocator_PMR(benchmark::Bench& bench)
{
    using CCoinsMap = std::pmr::unordered_map<COutPoint, CCoinsCacheEntry, SaltedOutpointHasher>;

    auto options = std::pmr::pool_options();
    options.largest_required_pool_block = 128;
    options.max_blocks_per_chunk = 262144 / options.largest_required_pool_block;

    // auto mr = std::pmr::unsynchronized_pool_resource();
    //  auto mr = std::pmr::monotonic_buffer_resource();
    auto& mr = *std::pmr::new_delete_resource();
    auto pmr = std::pmr::polymorphic_allocator<std::byte>{&mr};
    auto map = CCoinsMap{0, SaltedOutpointHasher{}, std::equal_to<COutPoint>{}, pmr};

    BenchFillClearMap(bench, map);
}

/**
 * NodePoolResource stores a number of byte chunks which are used to give out memory. 
 * size. The class is purposely kept very simple. It only knows about "Allocations" and "Pools".
 *
 * - Pool: MemoryResource allocates one memory pool at a time. These pools are kept around until the
 * memory resource is destroyed.
 *
 * - Allocations: Node-based containers allocate one node at a time. Whenever that happens, the
 * MemoryResource's Allocate() gives out memory for one node. These are carved out from a previously
 * allocated memory pool, or from a free list if it contains entries. Whenever a node is given back
 * with Deallocate(), it is put into that free list.
 */

template <size_t MAX_BLOCK_SIZE_BYTES>
class NodePoolResource : public std::pmr::memory_resource
{
    /**
     * In-place linked list of the allocations, used for the free list.
     */
    struct FreeList {
        FreeList* next = nullptr;
    };

    static constexpr size_t CHUNK_SIZE_BYTES = 262144;
    static constexpr size_t BLOCK_ALIGNMENT_BYTES = std::alignment_of_v<FreeList>;

    /**
     * Fallback allocator when pool is not used.
     */
    std::pmr::memory_resource* const m_upstream_resource = std::pmr::get_default_resource();

    /**
     * Contains all allocated pools of memory, used to free the data in the destructor.
     */
    std::vector<std::unique_ptr<std::byte[]>> m_allocated_chunks{};

    /**
     * Single linked lists of all data that came from deallocating.
     */
    std::vector<FreeList*> m_pools{MAX_BLOCK_SIZE_BYTES / BLOCK_ALIGNMENT_BYTES + 1};

    /**
     * Points to the beginning of available memory for carving out allocations.
     */
    std::byte* m_available_memory_it;

    /**
     * Points to the end of available memory for carving out allocations.
     *
     * That member variable is redundant, and is always equal to `m_allocated_chunks.back().get() + CHUNK_SIZE_BYTES`
     * whenever it is accessed, but `m_untouched_memory_end` caches this for clarity and efficiency.
     */
    std::byte* m_untouched_memory_end;

    [[nodiscard]] static constexpr size_t poolIdxOr0(size_t bytes, size_t alignment)
    {
        if (bytes <= MAX_BLOCK_SIZE_BYTES && alignment == BLOCK_ALIGNMENT_BYTES) {
            return bytes / BLOCK_ALIGNMENT_BYTES;
        }
        return 0;
    }

    void* do_allocate(size_t bytes, size_t alignment) override
    {
        if (const auto idx = poolIdxOr0(bytes, alignment)) {
            if (nullptr != m_pools[idx]) {
                return std::exchange(m_pools[idx], m_pools[idx]->next);
            }

            if (m_available_memory_it + bytes > m_available_memory_end) {
                allocateChunk();
            }

            return std::exchange(m_available_memory_it, m_available_memory_it + bytes);
        }
        return m_upstream_resource->allocate(bytes, alignment);
    }

    void do_deallocate(void* p, size_t bytes, size_t alignment) override
    {
        if (const auto idx = poolIdxOr0(bytes, alignment)) {
            auto* a = new (p) FreeList{};
            a->next = std::exchange(m_pools[idx], a);
        } else {
            m_upstream_resource->deallocate(p, bytes, alignment);
        }
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
    {
        return this == &other;
    }

    void allocateChunk()
    {
        m_allocated_chunks.emplace_back(new std::byte[CHUNK_SIZE_BYTES]);
        m_available_memory_it = m_allocated_chunks.back().get();
        m_available_memory_end = m_available_memory_it + CHUNK_SIZE_BYTES;
    }

public:
    NodePoolResource()
    {
        allocateChunk();
    }
};


static void NodeAllocator_CustomPMR(benchmark::Bench& bench)
{
    using CCoinsMap = std::pmr::unordered_map<COutPoint, CCoinsCacheEntry, SaltedOutpointHasher>;

    auto mr = NodePoolResource<256>();
    auto pmr = std::pmr::polymorphic_allocator<std::byte>{&mr};
    auto map = CCoinsMap{0, SaltedOutpointHasher{}, std::equal_to<COutPoint>{}, pmr};

    BenchFillClearMap(bench, map);
}

BENCHMARK(NodeAllocator_CustomPMR);
BENCHMARK(NodeAllocator_PMR);
BENCHMARK(NodeAllocator_StdUnorderedMap);
BENCHMARK(NodeAllocator_StdUnorderedMapWithNodeAllocator);


template <typename Func>
auto b(Func test_func, int iterations)
{
    const auto start = std::chrono::system_clock::now();
    while (iterations-- > 0) {
        test_func();
    }
    const auto stop = std::chrono::system_clock::now();
    const auto secs = std::chrono::duration<double>(stop - start);
    return secs.count();
}


static void NodeAllocator_foo(benchmark::Bench& bench)
{
    constexpr int total_nodes{5'000};

    std::unordered_map<uint64_t, uint64_t> list;
    uint64_t key = 213;
    ankerl::nanobench::Bench().minEpochIterations(1000).run("default_std_alloc", [&] {
        for (int i{}; i != total_nodes; ++i) {
            list[key];
            key += 0x967f29d1u;
        }
        list.clear();
    });

    std::pmr::unordered_map<uint64_t, uint64_t> pmr_list;
    ankerl::nanobench::Bench().minEpochIterations(1000).run("default_pmr_alloc", [&] {
        for (int i{}; i != total_nodes; ++i) {
            pmr_list[key];
            key += 0x967f29d1u;
        }
        pmr_list.clear();
    });

    {
        std::pmr::unsynchronized_pool_resource mbr;
        std::pmr::polymorphic_allocator<std::byte> pa{&mbr};
        std::pmr::unordered_map<uint64_t, uint64_t> mr_list{pa};
        ankerl::nanobench::Bench().minEpochIterations(1000).run("pmr_pool", [&] {
            for (int i{}; i != total_nodes; ++i) {
                mr_list[key];
                key += 0x967f29d1u;
            }
            mr_list.clear();
        });
    }

    std::pmr::monotonic_buffer_resource mbr;
    std::pmr::polymorphic_allocator<int> pa{&mbr};
    std::pmr::unordered_map<uint64_t, uint64_t> mr_list{pa};
    ankerl::nanobench::Bench().minEpochIterations(1000).run("pmr_alloc_no_buf", [&] {
        for (int i{}; i != total_nodes; ++i) {
            mr_list[key];
            key += 0x967f29d1u;
        }
        mr_list.clear();
    });

    std::array<std::byte, total_nodes * 32> buffer; // enough to fit in all nodes
    std::pmr::monotonic_buffer_resource mbr2{buffer.data(), buffer.size()};
    std::pmr::polymorphic_allocator<int> pa2{&mbr};
    std::pmr::unordered_map<uint64_t, uint64_t> mbr_list{pa2};
    ankerl::nanobench::Bench().minEpochIterations(1000).run("pmr_alloc_and_buf", [&] {
        for (int i{}; i != total_nodes; ++i) {
            mbr_list[key];
            key += 0x967f29d1u;
        }
        mbr_list.clear();
    });
}

BENCHMARK(NodeAllocator_foo);
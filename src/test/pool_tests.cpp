// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <support/allocators/pool.h>
#include <test/util/poolresourcetester.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(pool_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(basic_allocating)
{
    auto resource = PoolResource<8, 8>();
    PoolResourceTester::CheckAllDataAccountedFor(resource);

    // first chunk is already allocated
    size_t expected_bytes_available = resource.ChunkSizeBytes();
    BOOST_TEST(expected_bytes_available == PoolResourceTester::AvailableMemoryFromChunk(resource));

    // chunk is used, no more allocation
    void* block = resource.Allocate(8, 8);
    expected_bytes_available -= 8;
    BOOST_TEST(expected_bytes_available == PoolResourceTester::AvailableMemoryFromChunk(resource));

    BOOST_TEST(0 == PoolResourceTester::FreeListSizes(resource)[1]);
    resource.Deallocate(block, 8, 8);
    PoolResourceTester::CheckAllDataAccountedFor(resource);
    BOOST_TEST(1 == PoolResourceTester::FreeListSizes(resource)[1]);

    // alignment is too small, but the best fitting freelist is used. Nothing is allocated.
    block = resource.Allocate(8, 1);
    BOOST_TEST(0 == PoolResourceTester::FreeListSizes(resource)[1]);
    BOOST_TEST(expected_bytes_available == PoolResourceTester::AvailableMemoryFromChunk(resource));

    resource.Deallocate(block, 8, 1);
    PoolResourceTester::CheckAllDataAccountedFor(resource);
    BOOST_TEST(1 == PoolResourceTester::FreeListSizes(resource)[1]);
    BOOST_TEST(expected_bytes_available == PoolResourceTester::AvailableMemoryFromChunk(resource));

    // can't use chunk because alignment is too big
    block = resource.Allocate(8, 16);
    PoolResourceTester::CheckAllDataAccountedFor(resource);
    BOOST_TEST(1 == PoolResourceTester::FreeListSizes(resource)[1]);
    BOOST_TEST(expected_bytes_available == PoolResourceTester::AvailableMemoryFromChunk(resource));

    resource.Deallocate(block, 8, 16);
    PoolResourceTester::CheckAllDataAccountedFor(resource);
    BOOST_TEST(1 == PoolResourceTester::FreeListSizes(resource)[1]);
    BOOST_TEST(expected_bytes_available == PoolResourceTester::AvailableMemoryFromChunk(resource));

    // can't use chunk because size is too big
    block = resource.Allocate(16, 8);
    PoolResourceTester::CheckAllDataAccountedFor(resource);
    BOOST_TEST(1 == PoolResourceTester::FreeListSizes(resource)[1]);
    BOOST_TEST(expected_bytes_available == PoolResourceTester::AvailableMemoryFromChunk(resource));

    resource.Deallocate(block, 16, 8);
    PoolResourceTester::CheckAllDataAccountedFor(resource);
    BOOST_TEST(1 == PoolResourceTester::FreeListSizes(resource)[1]);
    BOOST_TEST(expected_bytes_available == PoolResourceTester::AvailableMemoryFromChunk(resource));
}

// Allocates from 0 to n bytes were n > the PoolResource's data, and each should work
BOOST_AUTO_TEST_CASE(allocate_any_byte)
{
    auto resource = PoolResource<128, 8>(1024);
    auto counts = PoolResourceTester::FreeListSizes(resource);

    uint8_t num_allocs = 200;

    auto data = std::vector<Span<uint8_t>>();

    // allocate an increasing number of bytes
    for (uint8_t num_bytes = 1; num_bytes < num_allocs; ++num_bytes) {
        uint8_t* bytes = new (resource.Allocate(num_bytes, 1)) uint8_t[num_bytes];
        BOOST_TEST(bytes != nullptr);
        data.emplace_back(bytes, num_bytes);

        // set each byte to i
        std::fill(bytes, bytes + num_bytes, num_bytes);
    }

    // now that we got all allocated, test if all still have the correct values, and give everything back to the allocator
    uint8_t val = 1;
    for (auto const& span : data) {
        for (auto x : span) {
            BOOST_TEST(val == x);
        }
        std::destroy(span.data(), span.data() + span.size());
        resource.Deallocate(span.data(), span.size(), 1);
        ++val;
    }

    PoolResourceTester::CheckAllDataAccountedFor(resource);
}

BOOST_AUTO_TEST_CASE(random_allocations)
{
    struct PtrSizeAlignment {
        void* ptr;
        size_t bytes;
        size_t alignment;
    };

    // makes a bunch of random allocations and gives all of them back in random order.
    auto resource = PoolResource<128, 8>(65536);
    std::vector<PtrSizeAlignment> ptr_size_alignment{};
    for (size_t i = 0; i < 1000; ++i) {
        // make it a bit more likely to allocate than deallocate
        if (ptr_size_alignment.empty() || 0 != InsecureRandRange(4)) {
            // allocate a random item
            std::size_t alignment = std::size_t{1} << InsecureRandRange(7);           // 1, 2, ..., 128
            std::size_t size = (InsecureRandRange(2000) / alignment + 1) * alignment; // multiple of alignment
            void* ptr = resource.Allocate(size, alignment);
            BOOST_TEST(ptr != nullptr);
            BOOST_TEST((reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0);
            ptr_size_alignment.push_back({ptr, size, alignment});
        } else {
            // deallocate a random item
            auto& x = ptr_size_alignment[InsecureRandRange(ptr_size_alignment.size())];
            resource.Deallocate(x.ptr, x.bytes, x.alignment);
            x = ptr_size_alignment.back();
            ptr_size_alignment.pop_back();
        }
    }

    // deallocate all the rest
    for (auto const& x : ptr_size_alignment) {
        resource.Deallocate(x.ptr, x.bytes, x.alignment);
    }

    PoolResourceTester::CheckAllDataAccountedFor(resource);
}

BOOST_AUTO_TEST_SUITE_END()

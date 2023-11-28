// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UNALIGNED_WRAPPER_H
#define BITCOIN_UNALIGNED_WRAPPER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>


template <typename T>
class UnalignedWrapper
{
    std::array<std::byte, sizeof(T)> m_data{};

    static_assert(std::is_trivial_v<T>, "is_trivial means is_trivially_copyable and trivial default ctor");

    [[nodiscard]] constexpr T get() const
    {
        T current;
        std::memcpy(&current, m_data.data(), sizeof(T));
        return current;
    }

    constexpr UnalignedWrapper& put(T val)
    {
        std::memcpy(m_data.data(), &val, sizeof(T));
        return *this;
    }

public:
    constexpr UnalignedWrapper() = default;

    constexpr explicit UnalignedWrapper(T val) noexcept
    {
        put(val);
    }

    constexpr UnalignedWrapper& operator=(T val)
    {
        return put(val);
    }

    /**
     * Use defined conversion function is not explicit
     */
    constexpr operator T() const
    {
        return get();
    }

    constexpr UnalignedWrapper& operator+=(T other) noexcept
    {
        return put(get() + other);
    }

    constexpr UnalignedWrapper& operator-=(T other) noexcept
    {
        return put(get() - other);
    }
};

/**
 * A minimal wrapper around a trivial type which uses memcpy to and from an array to strip the alignment from the data type.
 * This can be used in structs to reduce alignment and therefore reduce padding, which can make the data more compact.
 */
template <typename T, size_t StartBit, size_t NumBits>
class UnalignedBitmaskWrapper
{
    static constexpr size_t MaxNumBits = sizeof(T) * 8;
    static constexpr T BitMask = static_cast<T>(((T{1} << NumBits) - 1U) << StartBit);
    std::array<std::byte, sizeof(T)> m_data{};

    static_assert(std::is_trivial_v<T>, "is_trivial means is_trivially_copyable and trivial default ctor");
    static_assert(std::is_unsigned_v<T>);
    static_assert(StartBit + NumBits <= MaxNumBits, "bits out of range");
    static_assert(NumBits >= 1, "need at least one bit");
    static_assert(NumBits < MaxNumBits, "use UnalignedWrapper instead");

    [[nodiscard]] constexpr T get() const
    {
        T current;
        std::memcpy(&current, m_data.data(), sizeof(T));
        return current;
    }

    constexpr UnalignedBitmaskWrapper& put(T val)
    {
        std::memcpy(m_data.data(), &val, sizeof(T));
        return *this;
    }

public:
    constexpr UnalignedBitmaskWrapper() = default;

    constexpr explicit UnalignedBitmaskWrapper(T val) noexcept
    {
        put((val << StartBit) & BitMask);
    }

    constexpr UnalignedBitmaskWrapper& operator=(T val)
    {
        return put(((val << StartBit) & BitMask) | (get() & ~BitMask));
    }

    /**
     * Use defined conversion function is not explicit
     */
    constexpr operator T() const
    {
        return (get() & BitMask) >> StartBit;
    }
};

#endif // BITCOIN_UNALIGNED_WRAPPER_H

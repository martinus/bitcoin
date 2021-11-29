// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LOGGING_H
#define BITCOIN_LOGGING_H

#include <fs.h>
#include <tinyformat.h>
#include <threadsafety.h>
#include <util/string.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <vector>

// inspired by https://github.com/s9w/oof

namespace term {

namespace detail {

inline static constexpr auto hsv_to_rgb(double hue, double saturation, double value) -> std::array<double, 3> {
    auto h_i = static_cast<int>(hue * 6);
    auto f = hue * 6 - h_i;
    auto p = value * (1 - saturation);
    auto q = value * (1 - f * saturation);
    auto t = value * (1 - (1 - f) * saturation);

    switch (h_i) {
    case 0:
        return {value, t, p};
    case 1:
        return {q, value, p};
    case 2:
        return {p, value, t};
    case 3:
        return {p, q, value};
    case 4:
        return {t, p, value};
    case 5:
        return {value, p, q};
    }

    throw std::runtime_error("overflow?");
}

} // namespace detail

enum class ColorType : uint8_t { foreground, background, reset };

// Changes background color
struct Color {
    uint8_t m_r{};
    uint8_t m_g{};
    uint8_t m_b{};
    ColorType m_color_type = ColorType::reset;

    // reset
    inline constexpr Color() = default;

    inline constexpr Color(ColorType ct, double r, double g, double b)
        : m_r(std::clamp<int>(r * 256, 0, 255))
        , m_g(std::clamp<int>(g * 256, 0, 255))
        , m_b(std::clamp<int>(b * 256, 0, 255))
        , m_color_type(ct) {}
};

inline constexpr auto operator<<(std::ostream& os, Color const& c) -> std::ostream& {
    if (ColorType::reset == c.m_color_type) {
        return os << "\x1b[0m";
    }

    char const* code = ColorType::foreground == c.m_color_type ? "\x1b[38;2;" : "\x1b[48;2;";
    return os << code << (int)c.m_r << ';' << (int)c.m_g << ';' << (int)c.m_b << 'm';
}

inline constexpr auto hsv_bg(double hue, double saturation, double value) -> Color {
    auto rgb = detail::hsv_to_rgb(hue, saturation, value);
    return {ColorType::background, rgb[0], rgb[1], rgb[2]};
}

inline constexpr auto hsv_fg(double hue, double saturation, double value) -> Color {
    auto rgb = detail::hsv_to_rgb(hue, saturation, value);
    return {ColorType::foreground, rgb[0], rgb[1], rgb[2]};
}

inline auto reset() -> Color {
    return {};
}

// murmur 3 hash finalizer
inline constexpr auto mix(uint64_t x) -> uint64_t {
    x ^= (x >> 33);
    x *= 0xff51afd7ed558ccd;
    x ^= (x >> 33);
    x *= 0xc4ceb9fe1a85ec53;
    x ^= (x >> 33);
    return x;
}

inline constexpr auto hash(std::string_view sv) -> uint64_t {
    uint64_t h = 1234;
    for (auto c : sv) {
        h = mix(h ^ c);
    }
    return h;
}

inline constexpr auto hash100(std::string_view sv) -> int {
    return static_cast<int>(hash(sv) / 18446744073709551616.0 * 100.0);
}

// values 0-100. Ought to be enough resolution for everybody
template <int Hue100, int Saturation100, int Value100>
struct HSV_FG {
    static constexpr auto value = hsv_fg(Hue100 / 100.0, Saturation100 / 100.0, Value100 / 100.0);
};

} // namespace term

static const bool DEFAULT_LOGTIMEMICROS = false;
static const bool DEFAULT_LOGIPS        = false;
static const bool DEFAULT_LOGTIMESTAMPS = true;
static const bool DEFAULT_LOGTHREADNAMES = false;
static const bool DEFAULT_LOGSOURCELOCATIONS = false;
extern const char * const DEFAULT_DEBUGLOGFILE;

extern bool fLogIPs;

struct LogCategory {
    std::string category;
    bool active;
};

namespace BCLog {
    enum LogFlags : uint32_t {
        NONE        = 0,
        NET         = (1 <<  0),
        TOR         = (1 <<  1),
        MEMPOOL     = (1 <<  2),
        HTTP        = (1 <<  3),
        BENCH       = (1 <<  4),
        ZMQ         = (1 <<  5),
        WALLETDB    = (1 <<  6),
        RPC         = (1 <<  7),
        ESTIMATEFEE = (1 <<  8),
        ADDRMAN     = (1 <<  9),
        SELECTCOINS = (1 << 10),
        REINDEX     = (1 << 11),
        CMPCTBLOCK  = (1 << 12),
        RAND        = (1 << 13),
        PRUNE       = (1 << 14),
        PROXY       = (1 << 15),
        MEMPOOLREJ  = (1 << 16),
        LIBEVENT    = (1 << 17),
        COINDB      = (1 << 18),
        QT          = (1 << 19),
        LEVELDB     = (1 << 20),
        VALIDATION  = (1 << 21),
        I2P         = (1 << 22),
        IPC         = (1 << 23),
        LOCK        = (1 << 24),
        UTIL        = (1 << 25),
        BLOCKSTORE  = (1 << 26),
        ALL         = ~(uint32_t)0,
    };

    class Logger
    {
    private:
        mutable StdMutex m_cs; // Can not use Mutex from sync.h because in debug mode it would cause a deadlock when a potential deadlock was detected

        FILE* m_fileout GUARDED_BY(m_cs) = nullptr;
        std::list<std::string> m_msgs_before_open GUARDED_BY(m_cs);
        bool m_buffering GUARDED_BY(m_cs) = true; //!< Buffer messages before logging can be started.

        /**
         * m_started_new_line is a state variable that will suppress printing of
         * the timestamp when multiple calls are made that don't end in a
         * newline.
         */
        std::atomic_bool m_started_new_line{true};

        /** Log categories bitfield. */
        std::atomic<uint32_t> m_categories{0};

        std::string LogTimestampStr(const std::string& str);

        /** Slots that connect to the print signal */
        std::list<std::function<void(const std::string&)>> m_print_callbacks GUARDED_BY(m_cs) {};

    public:
        bool m_print_to_console = false;
        bool m_print_to_file = false;

        bool m_log_timestamps = DEFAULT_LOGTIMESTAMPS;
        bool m_log_time_micros = DEFAULT_LOGTIMEMICROS;
        bool m_log_threadnames = DEFAULT_LOGTHREADNAMES;
        bool m_log_sourcelocations = DEFAULT_LOGSOURCELOCATIONS;

        fs::path m_file_path;
        std::atomic<bool> m_reopen_file{false};

        /** Send a string to the log output */
        void LogPrintStr(const std::string& str, const std::string& logging_function, const std::string& source_file, const int source_line, term::Color col);

        /** Returns whether logs will be written to any output */
        bool Enabled() const
        {
            StdLockGuard scoped_lock(m_cs);
            return m_buffering || m_print_to_console || m_print_to_file || !m_print_callbacks.empty();
        }

        /** Connect a slot to the print signal and return the connection */
        std::list<std::function<void(const std::string&)>>::iterator PushBackCallback(std::function<void(const std::string&)> fun)
        {
            StdLockGuard scoped_lock(m_cs);
            m_print_callbacks.push_back(std::move(fun));
            return --m_print_callbacks.end();
        }

        /** Delete a connection */
        void DeleteCallback(std::list<std::function<void(const std::string&)>>::iterator it)
        {
            StdLockGuard scoped_lock(m_cs);
            m_print_callbacks.erase(it);
        }

        /** Start logging (and flush all buffered messages) */
        bool StartLogging();
        /** Only for testing */
        void DisconnectTestLogger();

        void ShrinkDebugFile();

        uint32_t GetCategoryMask() const { return m_categories.load(); }

        void EnableCategory(LogFlags flag);
        bool EnableCategory(const std::string& str);
        void DisableCategory(LogFlags flag);
        bool DisableCategory(const std::string& str);

        bool WillLogCategory(LogFlags category) const;
        /** Returns a vector of the log categories in alphabetical order. */
        std::vector<LogCategory> LogCategoriesList() const;
        /** Returns a string with the log categories in alphabetical order. */
        std::string LogCategoriesString() const
        {
            return Join(LogCategoriesList(), ", ", [&](const LogCategory& i) { return i.category; });
        };

        bool DefaultShrinkDebugFile() const;
    };

} // namespace BCLog

BCLog::Logger& LogInstance();

/** Return true if log accepts specified category */
static inline bool LogAcceptCategory(BCLog::LogFlags category)
{
    return LogInstance().WillLogCategory(category);
}

/** Return true if str parses as a log category and set the flag */
bool GetLogCategory(BCLog::LogFlags& flag, const std::string& str);

// Be conservative when using LogPrintf/error or other things which
// unconditionally log to debug.log! It should not be the case that an inbound
// peer can fill up a user's disk with debug.log entries.

template <typename... Args>
static inline void LogPrintf_(const std::string& logging_function, const std::string& source_file, const int source_line, term::Color col, const char* fmt, const Args&... args)
{
    if (LogInstance().Enabled()) {
        std::string log_msg;
        try {
            log_msg = tfm::format(fmt, args...);
        } catch (tinyformat::format_error& fmterr) {
            /* Original format string will have newline so don't add one here */
            log_msg = "Error \"" + std::string(fmterr.what()) + "\" while formatting log message: " + fmt;
        }
        LogInstance().LogPrintStr(log_msg, logging_function, source_file, source_line, col);
    }
}

#define LogPrintf(...) LogPrintf_(__func__, __FILE__, __LINE__, term::HSV_FG<term::hash100(__FILE__), 50, 100>::value, __VA_ARGS__)

// Use a macro instead of a function for conditional logging to prevent
// evaluating arguments when logging for the category is not enabled.
#define LogPrint(category, ...)              \
    do {                                     \
        if (LogAcceptCategory((category))) { \
            LogPrintf(__VA_ARGS__);          \
        }                                    \
    } while (0)

#endif // BITCOIN_LOGGING_H

#pragma once
#include "types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <random>

struct SymbolInfo {
    std::string symbol;
    std::string name;
    double price;
    double change;
    double change_pct;
};

extern std::string g_upstox_access_token;

class DataFetcher {
public:
    DataFetcher();
    ~DataFetcher();

    // Non-copyable, non-movable (owns curl global state)
    DataFetcher(const DataFetcher&) = delete;
    DataFetcher& operator=(const DataFetcher&) = delete;

    std::vector<Candle> fetch_intraday(const std::string& symbol,
                                       const std::string& period = "5d",
                                       const std::string& interval = "1m");

    double get_latest_price(const std::string& symbol);

    SymbolInfo get_symbol_info(const std::string& symbol);

    std::vector<std::string> get_all_symbols() const;

    std::string resolve_symbol(const std::string& symbol) const;

private:
    struct CacheEntry {
        std::vector<Candle> candles;
        std::chrono::steady_clock::time_point timestamp;
    };

    static constexpr int CACHE_TTL_SECONDS = 20;

    std::unordered_map<std::string, std::string> symbol_map_;
    std::unordered_map<std::string, std::string> display_names_;
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex cache_mutex_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> jitter_dist_;

    std::string http_get(const std::string& url);
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    std::string unix_to_iso8601(int64_t timestamp);
};

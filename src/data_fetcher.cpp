#include "data_fetcher.h"
#include "json.hpp"
#include <curl/curl.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

DataFetcher::DataFetcher()
    : rng_(std::random_device{}())
    , jitter_dist_(-0.0002, 0.0002)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Upstox Instrument mappings
    symbol_map_ = {
        {"NIFTY",      "NSE_INDEX|Nifty 50"},
        {"BANKNIFTY",  "NSE_INDEX|Nifty Bank"},
        {"RELIANCE",   "NSE_EQ|INE002A01018"},
        {"TCS",        "NSE_EQ|INE467B01029"},
        {"HDFCBANK",   "NSE_EQ|INE040A01034"},
        {"ADANIENT",   "NSE_EQ|INE423A01024"},
        {"ADANIPORTS", "NSE_EQ|INE742F01042"},
        {"ADANIGREEN", "NSE_EQ|INE364U01010"},
        {"ADANIPOWER", "NSE_EQ|INE814H01029"},
    };

    // Human-readable display names
    display_names_ = {
        {"NIFTY",      "NIFTY 50"},
        {"BANKNIFTY",  "Bank NIFTY"},
        {"RELIANCE",   "Reliance Industries"},
        {"TCS",        "TCS Limited"},
        {"HDFCBANK",   "HDFC Bank"},
        {"ADANIENT",   "Adani Enterprises"},
        {"ADANIPORTS", "Adani Ports"},
        {"ADANIGREEN", "Adani Green Energy"},
        {"ADANIPOWER", "Adani Power"},
    };
}

DataFetcher::~DataFetcher() {
    curl_global_cleanup();
}

// ── libcurl write callback ──────────────────────────────────────────────────────
size_t DataFetcher::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* response = static_cast<std::string*>(userdata);
    response->append(ptr, size * nmemb);
    return size * nmemb;
}

// ── HTTP GET via libcurl ────────────────────────────────────────────────────────
std::string DataFetcher::http_get(const std::string& url) {
    std::string response;
    CURL* curl = curl_easy_init();
    if (!curl) return response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 TradingBot/1.0");
    // Disable SSL verification for simplicity (production should enable)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    if (!g_upstox_access_token.empty()) {
        std::string auth_header = "Authorization: Bearer " + g_upstox_access_token;
        headers = curl_slist_append(headers, auth_header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[DataFetcher] curl error: " << curl_easy_strerror(res)
                  << " url=" << url << "\n";
        response.clear();
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

// ── Unix timestamp → ISO 8601 ───────────────────────────────────────────────────
std::string DataFetcher::unix_to_iso8601(int64_t ts) {
    std::time_t t = static_cast<std::time_t>(ts);
    std::tm tm_buf{};
    gmtime_r(&t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ── Fetch intraday OHLCV from Yahoo Finance v8 ─────────────────────────────────
std::vector<Candle> DataFetcher::fetch_intraday(const std::string& symbol,
                                                 const std::string& period,
                                                 const std::string& interval)
{
    std::string cache_key = symbol + "|" + period + "|" + interval;

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cache_.find(cache_key);
        if (it != cache_.end()) {
            auto elapsed = std::chrono::steady_clock::now() - it->second.timestamp;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < CACHE_TTL_SECONDS) {
                return it->second.candles;
            }
        }
    }

    // Resolve Upstox instrument key
    std::string instrument_key = resolve_symbol(symbol);
    
    // Replace spaces with %20 (do not URL encode the | symbol, Upstox rejects it)
    std::string encoded_key = instrument_key;
    size_t pos = 0;
    while ((pos = encoded_key.find(" ", pos)) != std::string::npos) {
        encoded_key.replace(pos, 1, "%20");
        pos += 3;
    }

    // Build Upstox API URL
    std::string url = "https://api.upstox.com/v2/historical-candle/intraday/"
                    + encoded_key + "/1minute";

    std::string raw = http_get(url);
    if (raw.empty()) return {};

    // Parse JSON response
    std::vector<Candle> candles;
    try {
        auto j = json::parse(raw);
        if (j["status"] != "success") {
            std::cerr << "[DataFetcher] Upstox error: " << raw << "\n";
            return {};
        }

        auto& candle_data = j["data"]["candles"];
        // Upstox returns data from newest to oldest. We need oldest to newest.
        size_t n = candle_data.size();
        candles.reserve(n);

        for (int i = n - 1; i >= 0; --i) {
            auto& arr = candle_data[i];
            Candle c;
            c.timestamp = arr[0].get<std::string>();
            c.open   = arr[1].get<double>();
            c.high   = arr[2].get<double>();
            c.low    = arr[3].get<double>();
            c.close  = arr[4].get<double>();
            c.volume = arr[5].get<int64_t>();
            candles.push_back(std::move(c));
        }
    } catch (const std::exception& e) {
        std::cerr << "[DataFetcher] Parse error for " << symbol << ": " << e.what() << "\n";
        return {};
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_[cache_key] = CacheEntry{candles, std::chrono::steady_clock::now()};
    }

    return candles;
}

// ── Latest close price ──────────────────────────────────────────────────────────
double DataFetcher::get_latest_price(const std::string& symbol) {
    auto candles = fetch_intraday(symbol);
    if (candles.empty()) return std::nan("");
    return candles.back().close;
}

// ── Symbol info with micro-jitter ───────────────────────────────────────────────
SymbolInfo DataFetcher::get_symbol_info(const std::string& symbol) {
    SymbolInfo info;
    info.symbol = symbol;

    auto it = display_names_.find(symbol);
    info.name = (it != display_names_.end()) ? it->second : symbol;

    auto candles = fetch_intraday(symbol);
    if (candles.empty()) {
        info.price = 0.0;
        info.change = 0.0;
        info.change_pct = 0.0;
        return info;
    }

    double raw_price = candles.back().close;

    // Add micro-jitter to simulate live tick variation
    double jitter = jitter_dist_(rng_);
    info.price = raw_price + raw_price * jitter;

    // Calculate change from first candle's open
    double open_price = candles.front().open;
    info.change = info.price - open_price;
    info.change_pct = (open_price != 0.0) ? (info.change / open_price) * 100.0 : 0.0;

    return info;
}

// ── All symbol keys ─────────────────────────────────────────────────────────────
std::vector<std::string> DataFetcher::get_all_symbols() const {
    std::vector<std::string> symbols;
    symbols.reserve(symbol_map_.size());
    for (const auto& [key, _] : symbol_map_) {
        symbols.push_back(key);
    }
    return symbols;
}

// ── Resolve friendly name → Yahoo ticker ────────────────────────────────────────
std::string DataFetcher::resolve_symbol(const std::string& symbol) const {
    auto it = symbol_map_.find(symbol);
    return (it != symbol_map_.end()) ? it->second : symbol;
}

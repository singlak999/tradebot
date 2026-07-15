// ═══════════════════════════════════════════════════════════════════════════════
// Trading Bot HTTP Server — main.cpp
// High-performance C++20 server using cpp-httplib, nlohmann/json, libcurl
// ═══════════════════════════════════════════════════════════════════════════════
#include "httplib.h"
#include "json.hpp"
#include "types.h"
#include "data_fetcher.h"
#include "indicators.h"
#include "strategies.h"
#include "simulator.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <curl/curl.h>

using json = nlohmann::json;

// ─── Helper: current time as ISO 8601 ───────────────────────────────────────────
static std::string now_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

// ─── Helper: read file to string ────────────────────────────────────────────────
static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// ─── Helper: guess MIME type from extension ─────────────────────────────────────
static std::string mime_type(const std::string& path) {
    if (path.ends_with(".html"))  return "text/html";
    if (path.ends_with(".css"))   return "text/css";
    if (path.ends_with(".js"))    return "application/javascript";
    if (path.ends_with(".json"))  return "application/json";
    if (path.ends_with(".png"))   return "image/png";
    if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
    if (path.ends_with(".svg"))   return "image/svg+xml";
    if (path.ends_with(".ico"))   return "image/x-icon";
    return "application/octet-stream";
}

// ═══════════════════════════════════════════════════════════════════════════════
// JSON serialization helpers
// ═══════════════════════════════════════════════════════════════════════════════

static json candle_to_json(const Candle& c) {
    return {
        {"timestamp", c.timestamp},
        {"open",      c.open},
        {"high",      c.high},
        {"low",       c.low},
        {"close",     c.close},
        {"volume",    c.volume}
    };
}

static json signal_to_json(const Signal& s) {
    return {
        {"value",      s.value},
        {"confidence", s.confidence}
    };
}

static json position_to_json(const Position& p) {
    return {
        {"id",               p.id},
        {"symbol",           p.symbol},
        {"type",             p.type},
        {"entry_price",      p.entry_price},
        {"underlying_entry", p.underlying_entry},
        {"quantity",         p.quantity},
        {"cost",             p.cost},
        {"entry_time",       p.entry_time},
        {"strategy",         p.strategy},
        {"status",           p.status},
        {"stop_loss",        p.stop_loss},
        {"target",           p.target},
        {"unrealized_pnl",   p.unrealized_pnl},
        {"exit_price",       p.exit_price},
        {"exit_time",        p.exit_time},
        {"pnl",              p.pnl}
    };
}

static json trade_to_json(const TradeRecord& t) {
    return {
        {"id",               t.id},
        {"symbol",           t.symbol},
        {"type",             t.type},
        {"action",           t.action},
        {"entry_price",      t.entry_price},
        {"exit_price",       t.exit_price},
        {"underlying_price", t.underlying_price},
        {"quantity",         t.quantity},
        {"cost",             t.cost},
        {"pnl",              t.pnl},
        {"timestamp",        t.timestamp},
        {"strategy",         t.strategy},
        {"close_reason",     t.close_reason}
    };
}

static json equity_to_json(const EquityPoint& e) {
    return {
        {"timestamp", e.timestamp},
        {"balance",   e.balance}
    };
}

static json portfolio_to_json(const PortfolioSummary& p) {
    return {
        {"initial_balance", p.initial_balance},
        {"balance",         p.balance},
        {"total_pnl",       p.total_pnl},
        {"realized_pnl",    p.realized_pnl},
        {"unrealized_pnl",  p.unrealized_pnl},
        {"pnl_percentage",  p.pnl_percentage},
        {"open_positions",  p.open_positions},
        {"total_trades",    p.total_trades},
        {"closed_trades",   p.closed_trades},
        {"win_rate",        p.win_rate},
        {"max_drawdown",    p.max_drawdown},
        {"sharpe_ratio",    p.sharpe_ratio},
        {"winning_trades",  p.winning_trades},
        {"losing_trades",   p.losing_trades},
        {"total_fees",      p.total_fees}
    };
}

static json symbol_info_to_json(const SymbolInfo& si) {
    return {
        {"symbol",     si.symbol},
        {"name",       si.name},
        {"price",      si.price},
        {"change",     si.change},
        {"change_pct", si.change_pct}
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// Global state
// ═══════════════════════════════════════════════════════════════════════════════

static DataFetcher g_data_fetcher;
static PaperTrader g_paper_trader;
std::string g_upstox_access_token; // Defined here for extern in data_fetcher.h

static std::mutex              g_sim_mutex;
static std::atomic<bool>       g_simulation_running{false};
static std::string             g_strategy_name;
static std::string             g_symbol;
static std::vector<std::string> g_symbols;
static std::string             g_started_at;
static std::string             g_last_update;
static std::string             g_error_msg;

// Track which signals have already been acted upon (strategy|symbol|timestamp)
static std::set<std::string>   g_acted_signals;
static std::mutex              g_acted_mutex;

// Strategies
static std::vector<std::unique_ptr<IStrategy>> g_strategies;

static void init_strategies() {
    g_strategies.push_back(std::make_unique<MomentumScalper>());
    g_strategies.push_back(std::make_unique<MeanReversion>());
    g_strategies.push_back(std::make_unique<SupertrendFollower>());
    g_strategies.push_back(std::make_unique<CombinedStrategy>());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Background simulation loop
// ═══════════════════════════════════════════════════════════════════════════════

static void simulation_loop() {
    std::cout << "[Simulation] Started on " << g_symbols.size() << " symbols\n";

    while (g_simulation_running.load()) {
        try {
            // Check for market close (15:15 IST onwards)
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf{};
            localtime_r(&t, &tm_buf);
            bool market_closing = (tm_buf.tm_hour == 15 && tm_buf.tm_min >= 15) || (tm_buf.tm_hour > 15) || (tm_buf.tm_hour < 9);

            // Build current price map for SL/target checking
            std::map<std::string, double> current_prices;

            for (const auto& sym : g_symbols) {
                auto candles = g_data_fetcher.fetch_intraday(sym);
                if (candles.empty()) continue;

                double price = candles.back().close;
                current_prices[sym] = price;

                // Run each strategy
                for (auto& strategy : g_strategies) {
                    auto signals = strategy->generate_signals(candles);
                    if (signals.empty()) continue;

                    // Use the latest signal
                    const auto& sig = signals.back();
                    if (sig.value == 0) continue; // HOLD — skip

                    // Deduplicate: key = strategy|symbol|timestamp
                    std::string sig_key = strategy->name() + "|" + sym + "|"
                                        + candles.back().timestamp;

                    {
                        std::lock_guard<std::mutex> lock(g_acted_mutex);
                        if (g_acted_signals.count(sig_key)) continue;
                        g_acted_signals.insert(sig_key);
                    }

                    if (market_closing && sig.value == 1) {
                        continue; // Do not open new positions when market is closing
                    }

                    // Execute trade
                    auto result = g_paper_trader.execute_trade(
                        sig.value, sym, price, 1,
                        candles.back().timestamp, strategy->name());

                    if (result.has_value()) {
                        std::cout << "[Trade] " << result->action << " "
                                  << result->type << " " << sym
                                  << " @ " << price
                                  << " strategy=" << strategy->name() << "\n";
                    }
                }
            }

            // Check stop-loss / target for all open positions
            auto sl_trades = g_paper_trader.check_stop_loss_target(current_prices);
            for (const auto& t : sl_trades) {
                std::cout << "[SL/Target] Closed " << t.symbol
                          << " reason=" << t.close_reason
                          << " pnl=" << t.pnl << "\n";
            }

            // Square off all positions if market is closing
            if (market_closing) {
                auto open_positions = g_paper_trader.get_open_positions();
                if (!open_positions.empty()) {
                    std::cout << "[Square Off] Market closing. Auto-closing all " << open_positions.size() << " positions.\n";
                    for (const auto& pos : open_positions) {
                        double price = current_prices.count(pos.symbol) ? current_prices[pos.symbol] : pos.entry_price;
                        auto result = g_paper_trader.close_position(pos.id, price, now_iso8601());
                        if (result.has_value()) {
                            std::cout << "[Square Off] Closed " << pos.symbol << " pnl=" << result->pnl << "\n";
                        }
                    }
                }
            }

            // Update last_update timestamp
            {
                std::lock_guard<std::mutex> lock(g_sim_mutex);
                g_last_update = now_iso8601();
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(g_sim_mutex);
            g_error_msg = e.what();
            std::cerr << "[Simulation] Error: " << e.what() << "\n";
        }

        // Sleep 15 seconds between iterations (interruptible)
        for (int i = 0; i < 150 && g_simulation_running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << "[Simulation] Stopped\n";
}

// ─── Start simulation helper ────────────────────────────────────────────────────
static void start_simulation(const std::vector<std::string>& symbols,
                             const std::string& strategy = "all")
{
    if (g_simulation_running.load()) return;

    {
        std::lock_guard<std::mutex> lock(g_sim_mutex);
        g_symbols = symbols;
        g_strategy_name = strategy;
        g_started_at = now_iso8601();
        g_last_update = g_started_at;
        g_error_msg.clear();
    }

    {
        std::lock_guard<std::mutex> lock(g_acted_mutex);
        g_acted_signals.clear();
    }

    g_simulation_running.store(true);
    std::thread(simulation_loop).detach();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Build JSON snapshots (used by SSE and REST endpoints)
// ═══════════════════════════════════════════════════════════════════════════════

static json build_simulation_json() {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    return {
        {"running",      g_simulation_running.load()},
        {"strategy",     g_strategy_name},
        {"symbols",      g_symbols},
        {"started_at",   g_started_at},
        {"last_update",  g_last_update},
        {"error",        g_error_msg},
        {"authenticated", !g_upstox_access_token.empty()}
    };
}

static json build_portfolio_json() {
    return portfolio_to_json(g_paper_trader.get_portfolio_summary());
}

static json build_trades_json() {
    auto trades = g_paper_trader.get_trade_history();
    json arr = json::array();
    for (const auto& t : trades) arr.push_back(trade_to_json(t));
    return arr;
}

static json build_equity_json() {
    auto curve = g_paper_trader.get_equity_curve();
    json arr = json::array();
    for (const auto& e : curve) arr.push_back(equity_to_json(e));
    return arr;
}

static json build_open_positions_json() {
    auto positions = g_paper_trader.get_open_positions();
    json arr = json::array();
    for (const auto& p : positions) arr.push_back(position_to_json(p));
    return arr;
}

static json build_market_data_json() {
    auto symbols = g_data_fetcher.get_all_symbols();
    json arr = json::array();
    for (const auto& sym : symbols) {
        auto info = g_data_fetcher.get_symbol_info(sym);
        arr.push_back(symbol_info_to_json(info));
    }
    return arr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main — HTTP Server Setup
// ═══════════════════════════════════════════════════════════════════════════════

int main() {
    init_strategies();

    httplib::Server svr;

    // ── CORS: apply to all responses ────────────────────────────────────────
    svr.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });

    // ── OPTIONS preflight ───────────────────────────────────────────────────
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });

    // ── GET / — Serve index.html ────────────────────────────────────────────
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        auto html = read_file("/home/krishna/trading-bot-cpp/templates/index.html");
        if (html.empty()) {
            res.status = 404;
            res.set_content("index.html not found", "text/plain");
        } else {
            res.set_content(html, "text/html");
        }
    });

    // ── GET /static/(.*) — Serve static assets ─────────────────────────────
    svr.Get("/static/(.*)", [](const httplib::Request& req, httplib::Response& res) {
        std::string path = "/home/krishna/trading-bot-cpp/static/" + req.matches[1].str();
        auto content = read_file(path);
        if (content.empty()) {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        } else {
            res.set_content(content, mime_type(path));
        }
    });

    // ── Upstox OAuth Flow ───────────────────────────────────────────────────
    auto get_env = [](const char* name, const std::string& def = "") {
        const char* val = std::getenv(name);
        return val ? std::string(val) : def;
    };
    
    std::string UPSTOX_API_KEY = get_env("UPSTOX_API_KEY", "YOUR_API_KEY");
    std::string UPSTOX_API_SECRET = get_env("UPSTOX_API_SECRET", "YOUR_API_SECRET");
    const std::string REDIRECT_URI = "http://127.0.0.1:5000/api/auth/upstox/callback";

    svr.Get("/api/auth/upstox/login", [&](const httplib::Request&, httplib::Response& res) {
        std::string auth_url = "https://api.upstox.com/v2/login/authorization/dialog?response_type=code&client_id=" 
                             + UPSTOX_API_KEY + "&redirect_uri=" + REDIRECT_URI;
        res.set_redirect(auth_url);
    });

    svr.Get("/api/auth/upstox/callback", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("code")) {
            std::string code = req.get_param_value("code");
            
            // Exchange code for token
            CURL* curl = curl_easy_init();
            std::string response_data;
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, "https://api.upstox.com/v2/login/authorization/token");
                std::string post_fields = "code=" + code + "&client_id=" + UPSTOX_API_KEY 
                                        + "&client_secret=" + UPSTOX_API_SECRET 
                                        + "&redirect_uri=" + REDIRECT_URI 
                                        + "&grant_type=authorization_code";
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
                
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
                headers = curl_slist_append(headers, "Accept: application/json");
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
                    auto* str = static_cast<std::string*>(userdata);
                    str->append(ptr, size * nmemb);
                    return size * nmemb;
                });
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
                
                curl_easy_perform(curl);
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                
                try {
                    auto j = json::parse(response_data);
                    if (j.contains("access_token")) {
                        g_upstox_access_token = j["access_token"];
                        res.set_content("<html><body><h2>Successfully authenticated with Upstox!</h2><p>You can close this window and start trading.</p><script>setTimeout(()=>window.location.href='/', 2000);</script></body></html>", "text/html");
                        return;
                    }
                } catch (...) {}
            }
            res.set_content("Failed to fetch access token: " + response_data, "text/plain");
        } else {
            res.set_content("Authorization code not provided.", "text/plain");
        }
    });

    // ── GET /api/simulation/stream — SSE endpoint ───────────────────────────
    svr.Get("/api/simulation/stream", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_chunked_content_provider("text/event-stream",
            [](size_t /*offset*/, httplib::DataSink& sink) {
                // Build full state snapshot
                json payload = {
                    {"simulation",     build_simulation_json()},
                    {"portfolio",      build_portfolio_json()},
                    {"trades",         build_trades_json()},
                    {"equity_curve",   build_equity_json()},
                    {"open_positions", build_open_positions_json()},
                    {"market_data",    build_market_data_json()}
                };

                std::string event = "data: " + payload.dump() + "\n\n";

                if (!sink.is_writable()) return false;
                sink.write(event.data(), event.size());

                // Sleep 1 second before next push
                std::this_thread::sleep_for(std::chrono::seconds(1));
                return true;
            });
    });

    // ── GET /api/market-data — All symbols with indicators ──────────────────
    svr.Get("/api/market-data", [](const httplib::Request&, httplib::Response& res) {
        auto symbols = g_data_fetcher.get_all_symbols();
        json arr = json::array();

        for (const auto& sym : symbols) {
            auto info = g_data_fetcher.get_symbol_info(sym);
            json entry = symbol_info_to_json(info);

            // Attach latest indicator values
            auto candles = g_data_fetcher.fetch_intraday(sym);
            if (!candles.empty()) {
                std::vector<double> closes, highs, lows;
                closes.reserve(candles.size());
                highs.reserve(candles.size());
                lows.reserve(candles.size());
                for (const auto& c : candles) {
                    closes.push_back(c.close);
                    highs.push_back(c.high);
                    lows.push_back(c.low);
                }

                auto rsi = calculate_rsi(closes);
                auto macd = calculate_macd(closes);
                auto bb = calculate_bollinger_bands(closes);

                entry["rsi"]  = rsi.empty()  ? 50.0 : rsi.back();
                entry["macd"] = macd.histogram.empty() ? 0.0 : macd.histogram.back();
                entry["bb_upper"] = bb.upper.empty() ? 0.0 : bb.upper.back();
                entry["bb_lower"] = bb.lower.empty() ? 0.0 : bb.lower.back();
            }

            arr.push_back(entry);
        }

        res.set_content(json{{"symbols", arr}}.dump(), "application/json");
    });

    // ── GET /api/market-data/:symbol/chart — Candle data ────────────────────
    svr.Get("/api/market-data/(\\w+)/chart", [](const httplib::Request& req, httplib::Response& res) {
        std::string symbol = req.matches[1].str();
        auto candles = g_data_fetcher.fetch_intraday(symbol);

        json arr = json::array();
        for (const auto& c : candles) arr.push_back(candle_to_json(c));

        res.set_content(json{{"candles", arr}}.dump(), "application/json");
    });

    // ── GET /api/simulation/status — Current state (auto-starts on first call)
    svr.Get("/api/simulation/status", [](const httplib::Request&, httplib::Response& res) {
        // Auto-start simulation on ALL symbols if not running
        if (!g_simulation_running.load()) {
            start_simulation(g_data_fetcher.get_all_symbols(), "all");
        }

        json body = {
            {"simulation", build_simulation_json()},
            {"portfolio",  build_portfolio_json()}
        };
        res.set_content(body.dump(), "application/json");
    });

    // ── GET /api/simulation/trades ──────────────────────────────────────────
    svr.Get("/api/simulation/trades", [](const httplib::Request&, httplib::Response& res) {
        auto trades = build_trades_json();
        json body = {
            {"trades", trades},
            {"count",  trades.size()}
        };
        res.set_content(body.dump(), "application/json");
    });

    // ── GET /api/simulation/equity-curve ────────────────────────────────────
    svr.Get("/api/simulation/equity-curve", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{{"equity_curve", build_equity_json()}}.dump(), "application/json");
    });

    // ── GET /api/simulation/signals — Signals from all strategies ───────────
    svr.Get("/api/simulation/signals", [](const httplib::Request&, httplib::Response& res) {
        auto symbols = g_data_fetcher.get_all_symbols();
        json result = json::object();

        for (const auto& sym : symbols) {
            auto candles = g_data_fetcher.fetch_intraday(sym);
            if (candles.empty()) continue;

            json sym_signals = json::object();
            for (auto& strategy : g_strategies) {
                auto signals = strategy->generate_signals(candles);
                json sig_arr = json::array();
                for (const auto& s : signals) sig_arr.push_back(signal_to_json(s));
                sym_signals[strategy->name()] = {
                    {"signals", sig_arr},
                    {"latest",  signals.empty() ? signal_to_json({0, 0.0})
                                                : signal_to_json(signals.back())}
                };
            }
            result[sym] = sym_signals;
        }

        res.set_content(json{{"signals", result}}.dump(), "application/json");
    });

    // ── POST /api/simulation/start ──────────────────────────────────────────
    svr.Post("/api/simulation/start", [](const httplib::Request& req, httplib::Response& res) {
        std::vector<std::string> symbols;
        std::string strategy = "all";

        try {
            auto body = json::parse(req.body);
            if (body.contains("symbols") && body["symbols"].is_array()) {
                symbols = body["symbols"].get<std::vector<std::string>>();
            }
            if (body.contains("strategy")) {
                strategy = body["strategy"].get<std::string>();
            }
        } catch (...) {
            // Use defaults on parse failure
        }

        if (symbols.empty()) {
            symbols = g_data_fetcher.get_all_symbols();
        }

        start_simulation(symbols, strategy);

        res.set_content(json{
            {"status", "started"},
            {"symbols", symbols},
            {"strategy", strategy}
        }.dump(), "application/json");
    });

    // ── POST /api/simulation/stop ───────────────────────────────────────────
    svr.Post("/api/simulation/stop", [](const httplib::Request&, httplib::Response& res) {
        g_simulation_running.store(false);
        res.set_content(json{{"status", "stopped"}}.dump(), "application/json");
    });

    // ── POST /api/simulation/reset ──────────────────────────────────────────
    svr.Post("/api/simulation/reset", [](const httplib::Request&, httplib::Response& res) {
        g_simulation_running.store(false);
        // Give the sim loop time to notice the stop
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        g_paper_trader.reset();

        {
            std::lock_guard<std::mutex> lock(g_acted_mutex);
            g_acted_signals.clear();
        }
        {
            std::lock_guard<std::mutex> lock(g_sim_mutex);
            g_started_at.clear();
            g_last_update.clear();
            g_error_msg.clear();
        }

        res.set_content(json{{"status", "reset"}}.dump(), "application/json");
    });

    // ── Start server ────────────────────────────────────────────────────────
    std::cout << "═══════════════════════════════════════════════════\n"
              << " Trading Bot C++ Server\n"
              << " Listening on http://0.0.0.0:5000\n"
              << "═══════════════════════════════════════════════════\n";

    if (!svr.listen("0.0.0.0", 5000)) {
        std::cerr << "Failed to start server on port 5000\n";
        return 1;
    }

    return 0;
}

#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <chrono>

struct Candle {
    std::string timestamp;
    double open, high, low, close;
    int64_t volume;
};

struct Signal {
    int value; // 1=BUY, -1=SELL, 0=HOLD
    double confidence; // 0-100
};

struct Position {
    std::string id;
    std::string symbol;
    std::string type; // CALL or PUT
    double entry_price;
    double underlying_entry;
    int quantity;
    double cost;
    std::string entry_time;
    std::string strategy;
    std::string status; // OPEN or CLOSED
    double stop_loss;
    double target;
    double unrealized_pnl;
    double exit_price;
    std::string exit_time;
    double pnl;
};

struct TradeRecord {
    std::string id;
    std::string symbol;
    std::string type; // CALL or PUT
    std::string action; // OPEN or CLOSE
    double entry_price;
    double exit_price;
    double underlying_price;
    int quantity;
    double cost;
    double pnl;
    std::string timestamp;
    std::string strategy;
    std::string close_reason; // STOP_LOSS, TARGET, or empty
};

struct EquityPoint {
    std::string timestamp;
    double balance;
};

struct PortfolioSummary {
    double initial_balance;
    double balance;
    double total_pnl;
    double realized_pnl;
    double unrealized_pnl;
    double pnl_percentage;
    int open_positions;
    int total_trades;
    int closed_trades;
    double win_rate;
    double max_drawdown;
    double sharpe_ratio;
    int winning_trades;
    int losing_trades;
    double total_fees;
};

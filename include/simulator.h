#pragma once
#include "types.h"
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <mutex>
#include <random>

class PaperTrader {
public:
    PaperTrader();

    std::optional<TradeRecord> execute_trade(int signal, const std::string& symbol,
                                              double price, int quantity,
                                              const std::string& timestamp,
                                              const std::string& strategy_name);

    std::optional<TradeRecord> close_position(const std::string& position_id,
                                               double price,
                                               const std::string& timestamp);

    std::vector<TradeRecord> check_stop_loss_target(const std::map<std::string, double>& current_prices);

    PortfolioSummary get_portfolio_summary() const;
    std::vector<EquityPoint> get_equity_curve() const;
    std::vector<TradeRecord> get_trade_history() const;
    std::vector<Position> get_open_positions() const;

    void reset();

private:
    static constexpr double INITIAL_BALANCE = 10000.0;
    static constexpr double MAX_RISK_PER_TRADE = 0.90;
    static constexpr double STOP_LOSS_PCT = 0.15;
    static constexpr double TARGET_PCT = 0.25;
    static constexpr double DELTA_FACTOR_MIN = 0.01;
    static constexpr double DELTA_FACTOR_MAX = 0.05;

    double balance_;
    double peak_balance_;
    double realized_pnl_;
    std::vector<Position> open_positions_;
    std::vector<TradeRecord> trade_history_;
    std::vector<EquityPoint> equity_curve_;
    std::vector<double> returns_;
    double total_fees_;

    mutable std::mutex mutex_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> delta_dist_;

    std::string generate_position_id();
    double calculate_max_drawdown() const;
    double calculate_sharpe_ratio() const;
};

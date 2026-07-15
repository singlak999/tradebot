#include "../include/simulator.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <random>

PaperTrader::PaperTrader()
    : balance_(INITIAL_BALANCE)
    , peak_balance_(INITIAL_BALANCE)
    , realized_pnl_(0.0)
    , total_fees_(0.0)
    , rng_(std::random_device{}())
    , delta_dist_(DELTA_FACTOR_MIN, DELTA_FACTOR_MAX)
{
    equity_curve_.push_back({"init", INITIAL_BALANCE});
}

std::string PaperTrader::generate_position_id() {
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    uint64_t val = dist(rng_);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << val;
    return oss.str();
}

std::optional<TradeRecord> PaperTrader::execute_trade(int signal,
                                                       const std::string& symbol,
                                                       double price,
                                                       int /*quantity*/,
                                                       const std::string& timestamp,
                                                       const std::string& strategy_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (signal == 0) return std::nullopt;

    // Calculate option premium
    double delta_factor = delta_dist_(rng_);
    double premium = price * delta_factor;

    // Position sizing
    double max_allocation = balance_ * MAX_RISK_PER_TRADE;
    int qty = std::max(1, static_cast<int>(max_allocation / premium));
    double cost = qty * premium;

    if (cost > balance_) {
        qty = std::max(1, static_cast<int>(balance_ / premium));
        cost = qty * premium;
    }

    if (balance_ < premium) return std::nullopt;

    std::string pos_type = (signal == 1) ? "CALL" : "PUT";
    std::string pos_id = generate_position_id();

    // Deduct cost
    balance_ -= cost;

    // Create position
    Position pos;
    pos.id = pos_id;
    pos.symbol = symbol;
    pos.type = pos_type;
    pos.entry_price = premium;
    pos.underlying_entry = price;
    pos.quantity = qty;
    pos.cost = cost;
    pos.entry_time = timestamp;
    pos.strategy = strategy_name;
    pos.status = "OPEN";
    pos.stop_loss = premium * (1.0 - STOP_LOSS_PCT);
    pos.target = premium * (1.0 + TARGET_PCT);
    pos.unrealized_pnl = 0.0;
    pos.exit_price = 0.0;
    pos.exit_time = "";
    pos.pnl = 0.0;

    open_positions_.push_back(std::move(pos));

    // Create trade record
    TradeRecord record;
    record.id = pos_id;
    record.symbol = symbol;
    record.type = pos_type;
    record.action = "OPEN";
    record.entry_price = premium;
    record.exit_price = 0.0;
    record.underlying_price = price;
    record.quantity = qty;
    record.cost = cost;
    record.pnl = 0.0;
    record.timestamp = timestamp;
    record.strategy = strategy_name;
    record.close_reason = "";

    trade_history_.push_back(record);

    // Update equity curve
    equity_curve_.push_back({timestamp, balance_});

    return record;
}

std::optional<TradeRecord> PaperTrader::close_position(const std::string& position_id,
                                                        double price,
                                                        const std::string& timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(open_positions_.begin(), open_positions_.end(),
        [&](const Position& p) { return p.id == position_id; });

    if (it == open_positions_.end()) return std::nullopt;

    Position& pos = *it;

    // Calculate exit premium based on underlying price change
    double price_change_pct = (price - pos.underlying_entry) / pos.underlying_entry;
    double exit_premium;

    if (pos.type == "CALL") {
        exit_premium = pos.entry_price * (1.0 + price_change_pct * 5.0);
    } else {
        exit_premium = pos.entry_price * (1.0 - price_change_pct * 5.0);
    }
    exit_premium = std::max(exit_premium, 0.01);

    double pnl = (exit_premium - pos.entry_price) * pos.quantity;
    
    // Deduct brokerage fee (55 INR)
    const double BROKERAGE = 55.0;
    pnl -= BROKERAGE;
    total_fees_ += BROKERAGE;

    // Update balance
    double proceeds = exit_premium * pos.quantity;
    balance_ += proceeds - BROKERAGE;
    realized_pnl_ += pnl;

    // Determine close reason
    std::string close_reason;
    if (exit_premium <= pos.stop_loss) {
        close_reason = "STOP_LOSS";
    } else if (exit_premium >= pos.target) {
        close_reason = "TARGET";
    }

    // Update peak balance
    if (balance_ > peak_balance_) {
        peak_balance_ = balance_;
    }

    // Record return for Sharpe calculation
    double ret = pnl / pos.cost;
    returns_.push_back(ret);

    // Create trade record
    TradeRecord record;
    record.id = pos.id;
    record.symbol = pos.symbol;
    record.type = pos.type;
    record.action = "CLOSE";
    record.entry_price = pos.entry_price;
    record.exit_price = exit_premium;
    record.underlying_price = price;
    record.quantity = pos.quantity;
    record.cost = pos.cost;
    record.pnl = pnl;
    record.timestamp = timestamp;
    record.strategy = pos.strategy;
    record.close_reason = close_reason;

    trade_history_.push_back(record);

    // Remove from open positions (swap-and-pop for perf)
    std::iter_swap(it, open_positions_.end() - 1);
    open_positions_.pop_back();

    // Update equity curve
    equity_curve_.push_back({timestamp, balance_});

    return record;
}

std::vector<TradeRecord> PaperTrader::check_stop_loss_target(const std::map<std::string, double>& current_prices) {
    // Collect IDs to close (cannot modify open_positions_ while iterating through close_position)
    std::vector<std::pair<std::string, double>> to_close;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pos : open_positions_) {
            auto pit = current_prices.find(pos.symbol);
            if (pit == current_prices.end()) continue;

            double current_price = pit->second;
            double price_change_pct = (current_price - pos.underlying_entry) / pos.underlying_entry;
            double current_premium;

            if (pos.type == "CALL") {
                current_premium = pos.entry_price * (1.0 + price_change_pct * 5.0);
            } else {
                current_premium = pos.entry_price * (1.0 - price_change_pct * 5.0);
            }
            current_premium = std::max(current_premium, 0.01);

            if (current_premium <= pos.stop_loss || current_premium >= pos.target) {
                to_close.emplace_back(pos.id, current_price);
            }
        }
    }

    std::vector<TradeRecord> closed;
    for (const auto& [id, price] : to_close) {
        auto record = close_position(id, price, "auto");
        if (record) {
            closed.push_back(std::move(*record));
        }
    }

    return closed;
}

double PaperTrader::calculate_max_drawdown() const {
    double peak = INITIAL_BALANCE;
    double max_dd = 0.0;

    for (const auto& pt : equity_curve_) {
        if (pt.balance > peak) {
            peak = pt.balance;
        }
        double dd = (peak - pt.balance) / peak * 100.0;
        if (dd > max_dd) {
            max_dd = dd;
        }
    }

    return max_dd;
}

double PaperTrader::calculate_sharpe_ratio() const {
    if (returns_.size() < 2) return 0.0;

    const double risk_free = 0.065 / 252.0;
    const int n = static_cast<int>(returns_.size());

    double sum = 0.0;
    for (double r : returns_) {
        sum += (r - risk_free);
    }
    double mean_excess = sum / n;

    double sq_sum = 0.0;
    for (double r : returns_) {
        double diff = (r - risk_free) - mean_excess;
        sq_sum += diff * diff;
    }
    double std_dev = std::sqrt(sq_sum / n);

    if (std_dev < 1e-12) return 0.0;

    return (mean_excess / std_dev) * std::sqrt(252.0);
}

PortfolioSummary PaperTrader::get_portfolio_summary() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Calculate unrealized P&L
    double unrealized = 0.0;
    for (const auto& pos : open_positions_) {
        unrealized += pos.unrealized_pnl;
    }

    double total_pnl = realized_pnl_ + unrealized;

    // Count wins/losses from closed trades
    int closed_trades = 0;
    int winning = 0;
    int losing = 0;

    for (const auto& t : trade_history_) {
        if (t.action == "CLOSE") {
            ++closed_trades;
            if (t.pnl > 0.0) ++winning;
            else if (t.pnl < 0.0) ++losing;
        }
    }

    double win_rate = (closed_trades > 0) ? (static_cast<double>(winning) / closed_trades * 100.0) : 0.0;

    PortfolioSummary summary;
    summary.initial_balance = INITIAL_BALANCE;
    summary.balance = balance_;
    summary.total_pnl = total_pnl;
    summary.realized_pnl = realized_pnl_;
    summary.unrealized_pnl = unrealized;
    summary.pnl_percentage = total_pnl / INITIAL_BALANCE * 100.0;
    summary.open_positions = static_cast<int>(open_positions_.size());
    summary.total_trades = static_cast<int>(trade_history_.size());
    summary.closed_trades = closed_trades;
    summary.win_rate = win_rate;
    summary.max_drawdown = calculate_max_drawdown();
    summary.sharpe_ratio = calculate_sharpe_ratio();
    summary.winning_trades = winning;
    summary.losing_trades = losing;
    summary.total_fees = total_fees_;

    return summary;
}

std::vector<EquityPoint> PaperTrader::get_equity_curve() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return equity_curve_;
}

std::vector<TradeRecord> PaperTrader::get_trade_history() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return trade_history_;
}

std::vector<Position> PaperTrader::get_open_positions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return open_positions_;
}

void PaperTrader::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    balance_ = INITIAL_BALANCE;
    peak_balance_ = INITIAL_BALANCE;
    realized_pnl_ = 0.0;
    total_fees_ = 0.0;
    open_positions_.clear();
    trade_history_.clear();
    equity_curve_.clear();
    returns_.clear();

    equity_curve_.push_back({"init", INITIAL_BALANCE});
}

#include "../include/strategies.h"
#include "../include/indicators.h"
#include <cmath>
#include <algorithm>
#include <numeric>

// Helper: extract close prices from candles
static std::vector<double> extract_close(const std::vector<Candle>& candles) {
    std::vector<double> close(candles.size());
    for (size_t i = 0; i < candles.size(); ++i) {
        close[i] = candles[i].close;
    }
    return close;
}

static std::vector<double> extract_high(const std::vector<Candle>& candles) {
    std::vector<double> high(candles.size());
    for (size_t i = 0; i < candles.size(); ++i) {
        high[i] = candles[i].high;
    }
    return high;
}

static std::vector<double> extract_low(const std::vector<Candle>& candles) {
    std::vector<double> low(candles.size());
    for (size_t i = 0; i < candles.size(); ++i) {
        low[i] = candles[i].low;
    }
    return low;
}

static std::vector<int64_t> extract_volume(const std::vector<Candle>& candles) {
    std::vector<int64_t> vol(candles.size());
    for (size_t i = 0; i < candles.size(); ++i) {
        vol[i] = candles[i].volume;
    }
    return vol;
}

// ─── MomentumScalper ────────────────────────────────────────────────────────────

std::string MomentumScalper::name() const {
    return "MomentumScalper";
}

std::vector<Signal> MomentumScalper::generate_signals(const std::vector<Candle>& candles) {
    const int n = static_cast<int>(candles.size());
    std::vector<Signal> signals(n, {0, 0.0});

    if (n == 0) return signals;

    auto close = extract_close(candles);
    auto rsi = calculate_rsi(close, 14);
    auto macd = calculate_macd(close, 12, 26, 9);
    auto ema9 = calculate_ema(close, 9);
    auto ema21 = calculate_ema(close, 21);

    for (int i = 0; i < n; ++i) {
        if (std::isnan(rsi[i]) || std::isnan(macd.histogram[i]) ||
            std::isnan(ema9[i]) || std::isnan(ema21[i])) {
            continue;
        }

        // BUY: RSI < 35 AND MACD histogram > 0 AND EMA(9) > EMA(21)
        if (rsi[i] < 35.0 && macd.histogram[i] > 0.0 && ema9[i] > ema21[i]) {
            double conf = std::min((35.0 - rsi[i]) / 35.0 * 100.0, 100.0);
            signals[i] = {1, conf};
        }
        // SELL: RSI > 65 AND MACD histogram < 0 AND EMA(9) < EMA(21)
        else if (rsi[i] > 65.0 && macd.histogram[i] < 0.0 && ema9[i] < ema21[i]) {
            double conf = std::min((rsi[i] - 65.0) / 35.0 * 100.0, 100.0);
            signals[i] = {-1, conf};
        }
    }

    return signals;
}

// ─── MeanReversion ──────────────────────────────────────────────────────────────

std::string MeanReversion::name() const {
    return "MeanReversion";
}

std::vector<Signal> MeanReversion::generate_signals(const std::vector<Candle>& candles) {
    const int n = static_cast<int>(candles.size());
    std::vector<Signal> signals(n, {0, 0.0});

    if (n == 0) return signals;

    auto close = extract_close(candles);
    auto rsi = calculate_rsi(close, 14);
    auto bb = calculate_bollinger_bands(close, 20, 2.0);

    for (int i = 0; i < n; ++i) {
        if (std::isnan(rsi[i]) || std::isnan(bb.lower[i]) || std::isnan(bb.upper[i])) {
            continue;
        }

        // BUY: close <= lower_BB AND RSI < 30
        if (close[i] <= bb.lower[i] && rsi[i] < 30.0) {
            double conf = std::min((30.0 - rsi[i]) / 30.0 * 100.0, 100.0);
            signals[i] = {1, conf};
        }
        // SELL: close >= upper_BB AND RSI > 70
        else if (close[i] >= bb.upper[i] && rsi[i] > 70.0) {
            double conf = std::min((rsi[i] - 70.0) / 30.0 * 100.0, 100.0);
            signals[i] = {-1, conf};
        }
    }

    return signals;
}

// ─── SupertrendFollower ─────────────────────────────────────────────────────────

std::string SupertrendFollower::name() const {
    return "SupertrendFollower";
}

std::vector<Signal> SupertrendFollower::generate_signals(const std::vector<Candle>& candles) {
    const int n = static_cast<int>(candles.size());
    std::vector<Signal> signals(n, {0, 0.0});

    if (n < 2) return signals;

    auto close = extract_close(candles);
    auto high = extract_high(candles);
    auto low = extract_low(candles);
    auto volumes = extract_volume(candles);

    auto st = calculate_supertrend(high, low, close, 10, 3.0);

    // Pre-compute 20-period average volume
    std::vector<double> avg_volume(n, 0.0);
    {
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            sum += static_cast<double>(volumes[i]);
            if (i >= 20) {
                sum -= static_cast<double>(volumes[i - 20]);
                avg_volume[i] = sum / 20.0;
            } else if (i >= 19) {
                avg_volume[i] = sum / 20.0;
            }
        }
    }

    for (int i = 1; i < n; ++i) {
        int prev_dir = st.direction[i - 1];
        int curr_dir = st.direction[i];

        // BUY: direction turns 1 AND volume > 1.5 * avg
        if (prev_dir == -1 && curr_dir == 1 && static_cast<double>(volumes[i]) > 1.5 * avg_volume[i]) {
            signals[i] = {1, 80.0};
        }
        // SELL: direction turns -1 AND volume > 1.5 * avg
        else if (prev_dir == 1 && curr_dir == -1 && static_cast<double>(volumes[i]) > 1.5 * avg_volume[i]) {
            signals[i] = {-1, 80.0};
        }
    }

    return signals;
}

// ─── CombinedStrategy ───────────────────────────────────────────────────────────

std::string CombinedStrategy::name() const {
    return "CombinedStrategy";
}

std::vector<Signal> CombinedStrategy::generate_signals(const std::vector<Candle>& candles) {
    const int n = static_cast<int>(candles.size());
    std::vector<Signal> signals(n, {0, 0.0});

    if (n == 0) return signals;

    auto mom_signals = momentum_.generate_signals(candles);
    auto mr_signals = mean_reversion_.generate_signals(candles);
    auto st_signals = supertrend_.generate_signals(candles);

    for (int i = 0; i < n; ++i) {
        double score = mom_signals[i].value * 0.4
                     + mr_signals[i].value * 0.3
                     + st_signals[i].value * 0.3;

        if (score >= 0.65) {
            signals[i] = {1, score * 100.0};
        } else if (score <= -0.65) {
            signals[i] = {-1, std::abs(score) * 100.0};
        }
    }

    return signals;
}

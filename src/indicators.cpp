#include "../include/indicators.h"
#include <cmath>
#include <numeric>
#include <limits>
#include <algorithm>

static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

std::vector<double> calculate_sma(const std::vector<double>& close, int period) {
    const int n = static_cast<int>(close.size());
    std::vector<double> result(n, NaN);

    if (n < period) return result;

    double sum = 0.0;
    for (int i = 0; i < period; ++i) {
        sum += close[i];
    }
    result[period - 1] = sum / period;

    for (int i = period; i < n; ++i) {
        sum += close[i] - close[i - period];
        result[i] = sum / period;
    }

    return result;
}

std::vector<double> calculate_ema(const std::vector<double>& close, int period) {
    const int n = static_cast<int>(close.size());
    std::vector<double> result(n, NaN);

    if (n == 0) return result;

    const double k = 2.0 / (period + 1.0);
    result[0] = close[0];

    for (int i = 1; i < n; ++i) {
        result[i] = close[i] * k + result[i - 1] * (1.0 - k);
    }

    return result;
}

// Wilder EMA helper (alpha = 1/period)
static std::vector<double> wilder_ema(const std::vector<double>& data, int period) {
    const int n = static_cast<int>(data.size());
    std::vector<double> result(n, NaN);

    if (n == 0) return result;

    const double alpha = 1.0 / period;
    result[0] = data[0];

    for (int i = 1; i < n; ++i) {
        result[i] = data[i] * alpha + result[i - 1] * (1.0 - alpha);
    }

    return result;
}

std::vector<double> calculate_rsi(const std::vector<double>& close, int period) {
    const int n = static_cast<int>(close.size());
    std::vector<double> result(n, NaN);

    if (n < 2) return result;

    // Calculate deltas
    std::vector<double> gains(n - 1);
    std::vector<double> losses(n - 1);

    for (int i = 0; i < n - 1; ++i) {
        double delta = close[i + 1] - close[i];
        gains[i] = std::max(delta, 0.0);
        losses[i] = std::max(-delta, 0.0);
    }

    // Wilder smoothing of gains and losses
    auto avg_gains = wilder_ema(gains, period);
    auto avg_losses = wilder_ema(losses, period);

    // Calculate RSI (result is offset by 1 since deltas start at index 1)
    for (int i = 0; i < static_cast<int>(avg_gains.size()); ++i) {
        double ag = avg_gains[i];
        double al = avg_losses[i];

        if (std::isnan(ag) || std::isnan(al)) continue;

        if (al == 0.0 && ag > 0.0) {
            result[i + 1] = 100.0;
        } else if (al == 0.0 && ag == 0.0) {
            result[i + 1] = 50.0;
        } else {
            double rs = ag / al;
            result[i + 1] = 100.0 - 100.0 / (1.0 + rs);
        }
    }

    return result;
}

MACDResult calculate_macd(const std::vector<double>& close, int fast, int slow, int signal) {
    auto ema_fast = calculate_ema(close, fast);
    auto ema_slow = calculate_ema(close, slow);

    const int n = static_cast<int>(close.size());
    std::vector<double> macd_line(n, NaN);

    for (int i = 0; i < n; ++i) {
        if (!std::isnan(ema_fast[i]) && !std::isnan(ema_slow[i])) {
            macd_line[i] = ema_fast[i] - ema_slow[i];
        }
    }

    // Build a clean series for signal EMA calculation
    std::vector<double> macd_clean;
    macd_clean.reserve(n);
    int first_valid = -1;
    for (int i = 0; i < n; ++i) {
        if (!std::isnan(macd_line[i])) {
            if (first_valid < 0) first_valid = i;
            macd_clean.push_back(macd_line[i]);
        }
    }

    auto signal_clean = calculate_ema(macd_clean, signal);

    std::vector<double> signal_line(n, NaN);
    std::vector<double> histogram(n, NaN);

    for (int i = 0; i < static_cast<int>(macd_clean.size()); ++i) {
        int idx = first_valid + i;
        signal_line[idx] = signal_clean[i];
        if (!std::isnan(macd_line[idx]) && !std::isnan(signal_line[idx])) {
            histogram[idx] = macd_line[idx] - signal_line[idx];
        }
    }

    return { std::move(macd_line), std::move(signal_line), std::move(histogram) };
}

BBResult calculate_bollinger_bands(const std::vector<double>& close, int period, double std_dev) {
    const int n = static_cast<int>(close.size());
    auto middle = calculate_sma(close, period);

    std::vector<double> upper(n, NaN);
    std::vector<double> lower(n, NaN);

    for (int i = period - 1; i < n; ++i) {
        // Calculate standard deviation over the window
        double sum = 0.0;
        double sum_sq = 0.0;
        for (int j = i - period + 1; j <= i; ++j) {
            sum += close[j];
            sum_sq += close[j] * close[j];
        }
        double mean = sum / period;
        double variance = sum_sq / period - mean * mean;
        double sd = std::sqrt(std::max(variance, 0.0));

        upper[i] = middle[i] + std_dev * sd;
        lower[i] = middle[i] - std_dev * sd;
    }

    return { std::move(upper), std::move(middle), std::move(lower) };
}

std::vector<double> calculate_atr(const std::vector<double>& high,
                                   const std::vector<double>& low,
                                   const std::vector<double>& close,
                                   int period) {
    const int n = static_cast<int>(close.size());
    std::vector<double> result(n, NaN);

    if (n < 2) return result;

    // Calculate True Range
    std::vector<double> tr(n, NaN);
    tr[0] = high[0] - low[0]; // First TR is just high-low

    for (int i = 1; i < n; ++i) {
        double hl = high[i] - low[i];
        double hc = std::abs(high[i] - close[i - 1]);
        double lc = std::abs(low[i] - close[i - 1]);
        tr[i] = std::max({ hl, hc, lc });
    }

    // Wilder smoothing of TR
    const double alpha = 1.0 / period;
    result[0] = tr[0];

    for (int i = 1; i < n; ++i) {
        result[i] = tr[i] * alpha + result[i - 1] * (1.0 - alpha);
    }

    return result;
}

SupertrendResult calculate_supertrend(const std::vector<double>& high,
                                       const std::vector<double>& low,
                                       const std::vector<double>& close,
                                       int period,
                                       double multiplier) {
    const int n = static_cast<int>(close.size());
    std::vector<double> supertrend(n, NaN);
    std::vector<int> direction(n, 1);

    if (n == 0) return { std::move(supertrend), std::move(direction) };

    auto atr = calculate_atr(high, low, close, period);

    std::vector<double> final_upper(n, NaN);
    std::vector<double> final_lower(n, NaN);

    for (int i = 0; i < n; ++i) {
        if (std::isnan(atr[i])) continue;

        double hl2 = (high[i] + low[i]) / 2.0;
        double basic_upper = hl2 + multiplier * atr[i];
        double basic_lower = hl2 - multiplier * atr[i];

        if (i == 0) {
            final_upper[i] = basic_upper;
            final_lower[i] = basic_lower;
            // Default direction
            direction[i] = (close[i] <= final_upper[i]) ? 1 : -1;
        } else {
            // Final upper band
            if (!std::isnan(final_upper[i - 1])) {
                final_upper[i] = (basic_upper < final_upper[i - 1] || close[i - 1] > final_upper[i - 1])
                    ? basic_upper : final_upper[i - 1];
            } else {
                final_upper[i] = basic_upper;
            }

            // Final lower band
            if (!std::isnan(final_lower[i - 1])) {
                final_lower[i] = (basic_lower > final_lower[i - 1] || close[i - 1] < final_lower[i - 1])
                    ? basic_lower : final_lower[i - 1];
            } else {
                final_lower[i] = basic_lower;
            }

            // Direction
            int prev_dir = direction[i - 1];
            if (prev_dir == 1) {
                direction[i] = (close[i] < final_lower[i]) ? -1 : 1;
            } else {
                direction[i] = (close[i] > final_upper[i]) ? 1 : -1;
            }
        }

        supertrend[i] = (direction[i] == 1) ? final_lower[i] : final_upper[i];
    }

    return { std::move(supertrend), std::move(direction) };
}

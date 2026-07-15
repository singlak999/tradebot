#pragma once
#include <vector>
#include <cmath>

struct MACDResult {
    std::vector<double> macd_line;
    std::vector<double> signal_line;
    std::vector<double> histogram;
};

struct BBResult {
    std::vector<double> upper;
    std::vector<double> middle;
    std::vector<double> lower;
};

struct SupertrendResult {
    std::vector<double> supertrend;
    std::vector<int> direction;
};

std::vector<double> calculate_sma(const std::vector<double>& close, int period);
std::vector<double> calculate_ema(const std::vector<double>& close, int period);
std::vector<double> calculate_rsi(const std::vector<double>& close, int period = 14);
MACDResult calculate_macd(const std::vector<double>& close, int fast = 12, int slow = 26, int signal = 9);
BBResult calculate_bollinger_bands(const std::vector<double>& close, int period = 20, double std_dev = 2.0);
std::vector<double> calculate_atr(const std::vector<double>& high, const std::vector<double>& low, const std::vector<double>& close, int period = 14);
SupertrendResult calculate_supertrend(const std::vector<double>& high, const std::vector<double>& low, const std::vector<double>& close, int period = 10, double multiplier = 3.0);

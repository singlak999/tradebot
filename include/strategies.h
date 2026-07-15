#pragma once
#include "types.h"
#include <vector>
#include <string>
#include <memory>

class IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual std::string name() const = 0;
    virtual std::vector<Signal> generate_signals(const std::vector<Candle>& candles) = 0;
};

class MomentumScalper : public IStrategy {
public:
    std::string name() const override;
    std::vector<Signal> generate_signals(const std::vector<Candle>& candles) override;
};

class MeanReversion : public IStrategy {
public:
    std::string name() const override;
    std::vector<Signal> generate_signals(const std::vector<Candle>& candles) override;
};

class SupertrendFollower : public IStrategy {
public:
    std::string name() const override;
    std::vector<Signal> generate_signals(const std::vector<Candle>& candles) override;
};

class CombinedStrategy : public IStrategy {
public:
    std::string name() const override;
    std::vector<Signal> generate_signals(const std::vector<Candle>& candles) override;

private:
    MomentumScalper momentum_;
    MeanReversion mean_reversion_;
    SupertrendFollower supertrend_;
};

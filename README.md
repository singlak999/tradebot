<div align="center">
  <h1>⚡ Ultra-Fast C++ F&O Trading Bot</h1>
  <p>A high-performance algorithmic paper-trading simulation engine built in C++ specifically for the Indian NSE F&O market, powered by the official <strong>Upstox V2 API</strong>.</p>

  <img src="https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++17"/>
  <img src="https://img.shields.io/badge/CMake-Ready-064F8C?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake"/>
  <img src="https://img.shields.io/badge/Frontend-Vanilla_JS-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black" alt="Vanilla JS"/>
  <img src="https://img.shields.io/badge/Broker-Upstox-800080?style=for-the-badge" alt="Upstox API"/>
</div>

<br />

## 🌟 Features

* **🚀 C++ Engine**: Core logic built completely in C++ for ultra-low latency execution and minimal overhead.
* **📊 Upstox API V2 Integration**: Directly fetches real-time 1-minute historical candles from the NSE using Upstox.
* **🔐 OAuth2 Flow Built-in**: Includes a native C++ HTTP server to handle the Upstox OAuth login and redirect flow locally.
* **🧠 Technical Indicators**: Includes custom, high-speed C++ implementations of MACD, RSI, EMA, SMA, Bollinger Bands, ATR, Supertrend, and VWAP.
* **⚙️ Multi-Strategy Support**: Toggle between *Momentum Scalper*, *Mean Reversion*, *Supertrend Follower*, and a *Combined Strategy*.
* **📈 Premium UI Dashboard**: Features a stunning, glassmorphism Bloomberg-style dark mode dashboard built with Chart.js and raw CSS/JS. Real-time updates delivered via Server-Sent Events (SSE).
* **💰 Cost Simulation**: Accurately tracks ₹55 per round-trip brokerage and taxes to simulate realistic F&O profit bleed.

## 📸 Dashboard Preview

The frontend UI runs entirely in your browser and automatically synchronizes with the backend via a low-latency SSE stream.
*(Includes a dynamic equity curve, animated PnL cards, and live buy/sell markers)*

## 🛠 Prerequisites

Make sure you have the following installed on your system:
* **C++ Compiler** (GCC 9+ or Clang 10+ supporting C++17)
* **CMake** (v3.10+)
* **Make**
* **libcurl** (for Upstox HTTP requests)

### Installing Dependencies (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake libcurl4-openssl-dev
```

## 🚀 Installation & Build

1. **Clone the repository:**
   ```bash
   git clone https://github.com/singlak999/tradebot.git
   cd tradebot
   ```

2. **Build the C++ Engine:**
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

## 🔑 Configuration (Environment Variables)

Because this bot connects to a live broker, you **must not hardcode your API keys**. The C++ server securely reads them from environment variables before starting. 

Before running the executable, set your Upstox App credentials:

```bash
export UPSTOX_API_KEY="your-upstox-api-key"
export UPSTOX_API_SECRET="your-upstox-api-secret"
```
> **Note:** Ensure you have added `http://127.0.0.1:5000/api/auth/upstox/callback` as the Redirect URI in your Upstox Developer Console.

## 🚦 Running the Bot

1. Execute the binary from the project root (so it can locate the HTML/CSS/JS files):
   ```bash
   cd ..
   ./build/trading_bot
   ```
2. Open your browser and navigate to `http://127.0.0.1:5000`.
3. Click the **🔑 Login to Upstox** button on the dashboard to authenticate.
4. Select your Strategy and Symbol, then hit **Start Simulation**!

## 📜 Supported Symbols

The engine is pre-mapped with correct Upstox Instrument Keys for the following high-volume NSE equities and indices:
* `NIFTY` (Nifty 50)
* `BANKNIFTY` (Nifty Bank)
* `RELIANCE` (Reliance Industries)
* `TCS` (Tata Consultancy Services)
* `HDFCBANK` (HDFC Bank)
* Adani Group: `ADANIENT`, `ADANIPORTS`, `ADANIGREEN`, `ADANIPOWER`

## ⚠️ Disclaimer
**For Educational Purposes Only.** This software simulates trades. I am not responsible for any financial losses incurred if you modify this codebase to place real-money orders. Algorithmic trading carries significant risk.

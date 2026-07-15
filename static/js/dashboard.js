/* ============================================================
   F&O Trading Bot Dashboard - JavaScript
   Premium Trading Dashboard Logic
   ============================================================ */

// --- Constants ---
const API_BASE = '';
const POLL_INTERVAL_SIM = 5000;       // 5s for simulation data
const POLL_INTERVAL_MARKET = 30000;   // 30s for market data
const INITIAL_BALANCE = 10000;

// --- State ---
let simulationRunning = false;
let currentSymbol = 'NIFTY';
let currentTimeRange = '1D';
let priceChart = null;
let equityChart = null;
let connected = true;
let previousBalance = INITIAL_BALANCE;
let previousTrades = [];

// --- Initialization ---
document.addEventListener('DOMContentLoaded', () => {
    initClock();
    initCharts();
    initEventListeners();
    fetchAllData();
    initEventStream();
});

// ============================================================
// Clock & Market Status
// ============================================================

function initClock() {
    updateClock();
    setInterval(updateClock, 1000);
}

function updateClock() {
    const now = new Date();
    // Convert to IST (UTC+5:30)
    const istOffset = 5.5 * 60 * 60 * 1000;
    const ist = new Date(now.getTime() + (istOffset + now.getTimezoneOffset() * 60000));

    const timeStr = ist.toLocaleTimeString('en-IN', {
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: true
    });
    const dateStr = ist.toLocaleDateString('en-IN', {
        weekday: 'short',
        day: 'numeric',
        month: 'short'
    });

    const clockEl = document.getElementById('clock');
    if (clockEl) {
        clockEl.textContent = `${dateStr} • ${timeStr} IST`;
    }

    updateMarketStatus(ist);
}

function updateMarketStatus(ist) {
    const day = ist.getDay();
    const hours = ist.getHours();
    const minutes = ist.getMinutes();
    const totalMinutes = hours * 60 + minutes;

    // Market open: Mon(1)-Fri(5), 9:15 AM (555 min) - 3:30 PM (930 min) IST
    const isWeekday = day >= 1 && day <= 5;
    const isMarketHours = totalMinutes >= 555 && totalMinutes <= 930;
    const marketOpen = isWeekday && isMarketHours;

    const el = document.getElementById('market-status');
    if (!el) return;

    if (marketOpen) {
        el.textContent = 'MARKET OPEN';
        el.className = 'market-status open';
    } else {
        el.textContent = 'MARKET CLOSED';
        el.className = 'market-status closed';
    }
}

// ============================================================
// Chart.js Initialization
// ============================================================

function initCharts() {
    initPriceChart();
    initEquityChart();
}

function initPriceChart() {
    const canvas = document.getElementById('price-chart');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    // Default gradient
    const gradientGreen = ctx.createLinearGradient(0, 0, 0, 350);
    gradientGreen.addColorStop(0, 'rgba(6, 182, 212, 0.25)');
    gradientGreen.addColorStop(0.5, 'rgba(6, 182, 212, 0.08)');
    gradientGreen.addColorStop(1, 'rgba(6, 182, 212, 0)');

    priceChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Price',
                    data: [],
                    borderColor: '#06b6d4',
                    backgroundColor: gradientGreen,
                    fill: true,
                    tension: 0.4,
                    borderWidth: 2,
                    pointRadius: 0,
                    pointHoverRadius: 5,
                    pointHoverBackgroundColor: '#06b6d4',
                    pointHoverBorderColor: '#fff',
                    pointHoverBorderWidth: 2,
                    order: 1
                },
                {
                    label: 'EMA 9',
                    data: [],
                    borderColor: '#fbbf24',
                    borderWidth: 1.5,
                    fill: false,
                    tension: 0.4,
                    pointRadius: 0,
                    order: 2
                },
                {
                    label: 'EMA 21',
                    data: [],
                    borderColor: '#f97316',
                    borderWidth: 1.5,
                    fill: false,
                    tension: 0.4,
                    pointRadius: 0,
                    order: 3
                },
                {
                    label: 'BB Upper',
                    data: [],
                    borderColor: 'rgba(0, 255, 136, 0.35)',
                    borderWidth: 1,
                    borderDash: [5, 5],
                    fill: false,
                    tension: 0.4,
                    pointRadius: 0,
                    order: 4
                },
                {
                    label: 'BB Lower',
                    data: [],
                    borderColor: 'rgba(255, 51, 102, 0.35)',
                    borderWidth: 1,
                    borderDash: [5, 5],
                    fill: false,
                    tension: 0.4,
                    pointRadius: 0,
                    order: 5
                },
                {
                    label: 'Volume',
                    data: [],
                    type: 'bar',
                    backgroundColor: 'rgba(6, 182, 212, 0.12)',
                    borderColor: 'rgba(6, 182, 212, 0.25)',
                    borderWidth: 1,
                    yAxisID: 'y2',
                    order: 6,
                    barPercentage: 0.6
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false
            },
            plugins: {
                legend: { display: false },
                tooltip: {
                    backgroundColor: 'rgba(17, 24, 39, 0.95)',
                    titleColor: '#e2e8f0',
                    bodyColor: '#94a3b8',
                    borderColor: 'rgba(255, 255, 255, 0.1)',
                    borderWidth: 1,
                    cornerRadius: 8,
                    padding: 12,
                    titleFont: { family: 'Inter', weight: '600' },
                    bodyFont: { family: 'JetBrains Mono', size: 12 },
                    usePointStyle: true,
                    callbacks: {
                        label: function(context) {
                            if (context.dataset.label === 'Volume') {
                                return ` Volume: ${formatNumber(context.raw)}`;
                            }
                            if (context.raw === null) return null;
                            return ` ${context.dataset.label}: ₹${formatNumber(context.raw)}`;
                        }
                    }
                }
            },
            scales: {
                x: {
                    grid: {
                        color: 'rgba(255, 255, 255, 0.03)',
                        drawBorder: false
                    },
                    ticks: {
                        color: '#64748b',
                        font: { size: 10, family: 'JetBrains Mono' },
                        maxTicksLimit: 10,
                        maxRotation: 0
                    }
                },
                y: {
                    position: 'right',
                    grid: {
                        color: 'rgba(255, 255, 255, 0.03)',
                        drawBorder: false
                    },
                    ticks: {
                        color: '#64748b',
                        font: { size: 10, family: 'JetBrains Mono' },
                        callback: function(value) {
                            return '₹' + formatCompact(value);
                        }
                    }
                },
                y2: {
                    position: 'left',
                    grid: { display: false },
                    ticks: { display: false },
                    beginAtZero: true
                }
            }
        }
    });
}

function initEquityChart() {
    const canvas = document.getElementById('equity-chart');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    const gradientGreen = ctx.createLinearGradient(0, 0, 0, 200);
    gradientGreen.addColorStop(0, 'rgba(0, 255, 136, 0.25)');
    gradientGreen.addColorStop(1, 'rgba(0, 255, 136, 0)');

    equityChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Balance',
                data: [],
                borderColor: '#00ff88',
                backgroundColor: gradientGreen,
                fill: true,
                tension: 0.4,
                borderWidth: 2,
                pointRadius: 0,
                pointHoverRadius: 4,
                pointHoverBackgroundColor: '#00ff88',
                pointHoverBorderColor: '#fff',
                pointHoverBorderWidth: 2
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false },
                tooltip: {
                    backgroundColor: 'rgba(17, 24, 39, 0.95)',
                    titleColor: '#e2e8f0',
                    bodyColor: '#94a3b8',
                    borderColor: 'rgba(255, 255, 255, 0.1)',
                    borderWidth: 1,
                    cornerRadius: 8,
                    padding: 10,
                    bodyFont: { family: 'JetBrains Mono', size: 11 },
                    callbacks: {
                        label: function(context) {
                            return `Balance: ₹${formatNumber(context.raw)}`;
                        }
                    }
                },
                annotation: {
                    annotations: {
                        baseline: {
                            type: 'line',
                            yMin: INITIAL_BALANCE,
                            yMax: INITIAL_BALANCE,
                            borderColor: 'rgba(148, 163, 184, 0.4)',
                            borderWidth: 1,
                            borderDash: [6, 4],
                            label: {
                                content: '₹5,000',
                                display: true,
                                position: 'start',
                                backgroundColor: 'rgba(17, 24, 39, 0.8)',
                                color: '#94a3b8',
                                font: { size: 10, family: 'JetBrains Mono' },
                                padding: 4
                            }
                        }
                    }
                }
            },
            scales: {
                x: {
                    grid: { display: false },
                    ticks: {
                        color: '#64748b',
                        font: { size: 9, family: 'JetBrains Mono' },
                        maxTicksLimit: 6,
                        maxRotation: 0
                    }
                },
                y: {
                    grid: { color: 'rgba(255, 255, 255, 0.03)' },
                    ticks: {
                        color: '#64748b',
                        font: { size: 9, family: 'JetBrains Mono' },
                        callback: function(value) {
                            return '₹' + formatCompact(value);
                        }
                    }
                }
            }
        }
    });
}

// ============================================================
// Event Listeners
// ============================================================

function initEventListeners() {
    // Control buttons
    const startBtn = document.getElementById('start-btn');
    const stopBtn = document.getElementById('stop-btn');
    const resetBtn = document.getElementById('reset-btn');
    const symbolSelect = document.getElementById('symbol-select');

    if (startBtn) startBtn.addEventListener('click', startSimulation);
    if (stopBtn) stopBtn.addEventListener('click', stopSimulation);
    if (resetBtn) resetBtn.addEventListener('click', resetSimulation);
    if (symbolSelect) symbolSelect.addEventListener('change', (e) => changeSymbol(e.target.value));

    // Time range buttons
    document.querySelectorAll('.time-range button').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.time-range button').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            currentTimeRange = btn.dataset.range;
            fetchChartData(currentSymbol);
        });
    });
}

// ============================================================
// API Fetch Functions
// ============================================================

async function fetchAllData() {
    await Promise.allSettled([
        fetchMarketData(),
        fetchSimulationStatus(),
        fetchTradeHistory(),
        fetchEquityCurve(),
        fetchSignals(),
        fetchChartData(currentSymbol)
    ]);
}

async function fetchMarketData() {
    try {
        const res = await fetch(`${API_BASE}/api/market-data`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        updateMarketCards(data.symbols);
        setConnected(true);
    } catch (err) {
        console.error('Market data error:', err);
        setConnected(false);
    }
}

async function fetchSimulationStatus() {
    try {
        const res = await fetch(`${API_BASE}/api/simulation/status`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        updateSimulationState(data);
        if (data.portfolio) {
            updatePortfolioCard(data.portfolio);
            updateMetricsGrid(data.portfolio);
        }
        setConnected(true);
    } catch (err) {
        console.error('Simulation status error:', err);
        setConnected(false);
    }
}

async function fetchTradeHistory() {
    try {
        const res = await fetch(`${API_BASE}/api/simulation/trades`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        updateTradeTable(data.trades);
        setConnected(true);
    } catch (err) {
        console.error('Trade history error:', err);
        setConnected(false);
    }
}

async function fetchEquityCurve() {
    try {
        const res = await fetch(`${API_BASE}/api/simulation/equity-curve`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        updateEquityChart(data.curve);
        setConnected(true);
    } catch (err) {
        console.error('Equity curve error:', err);
        setConnected(false);
    }
}

async function fetchSignals() {
    try {
        const res = await fetch(`${API_BASE}/api/simulation/signals`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        updateSignalCards(data.signals);
        setConnected(true);
    } catch (err) {
        console.error('Signals error:', err);
        setConnected(false);
    }
}

async function fetchChartData(symbol) {
    try {
        const res = await fetch(`${API_BASE}/api/market-data/${symbol}/chart`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        updatePriceChart(data.candles);
        setConnected(true);
    } catch (err) {
        console.error('Chart data error:', err);
        setConnected(false);
    }
}

// ============================================================
// Control Functions
// ============================================================

async function startSimulation() {
    const strategyEl = document.getElementById('strategy-select');
    const symbolEl = document.getElementById('symbol-select');
    if (!strategyEl || !symbolEl) return;

    const strategy = strategyEl.value;
    const symbol = symbolEl.value;

    try {
        const res = await fetch(`${API_BASE}/api/simulation/start`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ strategy, symbol })
        });
        const data = await res.json();

        if (res.ok || data.status === 'success' || data.status === 'ok') {
            showToast('Simulation started! 🚀', 'success');
            simulationRunning = true;
            updateControlButtons();
            // Immediately fetch fresh data
            setTimeout(fetchAllData, 500);
        } else {
            showToast(data.message || 'Failed to start simulation', 'error');
        }
    } catch (err) {
        showToast('Connection error: ' + err.message, 'error');
    }
}

async function stopSimulation() {
    try {
        const res = await fetch(`${API_BASE}/api/simulation/stop`, { method: 'POST' });
        await res.json();
        showToast('Simulation stopped ⏹', 'info');
        simulationRunning = false;
        updateControlButtons();
    } catch (err) {
        showToast('Error stopping simulation: ' + err.message, 'error');
    }
}

async function resetSimulation() {
    if (!confirm('Reset simulation? All trade history and portfolio data will be cleared.')) return;

    try {
        const res = await fetch(`${API_BASE}/api/simulation/reset`, { method: 'POST' });
        await res.json();
        showToast('Simulation reset! ↻', 'info');
        simulationRunning = false;
        previousBalance = INITIAL_BALANCE;
        previousTrades = [];
        updateControlButtons();
        resetUI();
        fetchAllData();
    } catch (err) {
        showToast('Error resetting: ' + err.message, 'error');
    }
}

function changeSymbol(symbol) {
    currentSymbol = symbol;
    fetchChartData(symbol);
}

function updateControlButtons() {
    const startBtn = document.getElementById('start-btn');
    const stopBtn = document.getElementById('stop-btn');

    if (!startBtn || !stopBtn) return;

    if (simulationRunning) {
        startBtn.disabled = true;
        startBtn.innerHTML = '<span class="status-dot" style="width:6px;height:6px;margin:0"></span> Running';
        stopBtn.disabled = false;
    } else {
        startBtn.disabled = false;
        startBtn.innerHTML = '▶ Start Simulation';
        stopBtn.disabled = true;
    }
}

function resetUI() {
    // Reset portfolio
    const balanceEl = document.getElementById('current-balance');
    if (balanceEl) {
        balanceEl.textContent = '₹10,000.00';
        balanceEl.className = 'balance-value mono';
    }

    const pnlEl = document.getElementById('total-pnl');
    if (pnlEl) { pnlEl.textContent = '₹0.00'; pnlEl.className = 'pnl-value mono'; }

    const pctEl = document.getElementById('pnl-pct');
    if (pctEl) { pctEl.textContent = '(0.00%)'; pctEl.className = 'pnl-pct mono'; }

    const barEl = document.getElementById('balance-bar');
    if (barEl) { barEl.style.width = '50%'; barEl.className = 'progress-fill'; }

    // Reset metrics
    ['total-trades', 'win-rate', 'max-drawdown', 'sharpe-ratio', 'profit-factor', 'best-trade'].forEach(id => {
        const el = document.getElementById(id);
        if (el) el.textContent = id === 'best-trade' ? '₹0.00' : (id === 'sharpe-ratio' || id === 'profit-factor' ? '0.00' : '0');
    });

    // Clear trades
    const tbody = document.getElementById('trades-body');
    if (tbody) tbody.innerHTML = '';
    const tradesEmpty = document.getElementById('trades-empty');
    if (tradesEmpty) tradesEmpty.style.display = 'block';
    const tradeCount = document.getElementById('trade-count');
    if (tradeCount) tradeCount.textContent = '';

    // Clear signals
    const sigContainer = document.getElementById('signals-container');
    if (sigContainer) sigContainer.innerHTML = '';
    const sigEmpty = document.getElementById('signals-empty');
    if (sigEmpty) sigEmpty.style.display = 'block';

    // Reset charts
    if (priceChart) {
        priceChart.data.labels = [];
        priceChart.data.datasets.forEach(ds => ds.data = []);
        priceChart.update('none');
    }
    if (equityChart) {
        equityChart.data.labels = [];
        equityChart.data.datasets[0].data = [];
        equityChart.update('none');
    }
}

// ============================================================
// UI Update Functions
// ============================================================

function updateSimulationState(data) {
    if (!data.simulation) return;
    
    if (data.simulation.running !== undefined) {
        simulationRunning = data.simulation.running;
        updateControlButtons();
    }
    
    if (data.simulation.authenticated !== undefined) {
        const btn = document.getElementById('upstox-login-btn');
        if (btn) {
            if (data.simulation.authenticated) {
                btn.textContent = '✅ Upstox Authenticated';
                btn.style.pointerEvents = 'none';
                btn.style.borderColor = '#10b981';
                btn.style.color = '#10b981';
            } else {
                btn.textContent = '🔑 Login to Upstox';
                btn.style.pointerEvents = 'auto';
                btn.style.borderColor = '#3b82f6';
                btn.style.color = '#3b82f6';
            }
        }
    }

    if (data.simulation.symbol) {
        const symbolEl = document.getElementById('symbol-select');
        if (symbolEl && symbolEl.value !== data.simulation.symbol) {
            symbolEl.value = data.simulation.symbol;
            currentSymbol = data.simulation.symbol;
        }
    }

    if (data.simulation.strategy_name) {
        const stratEl = document.getElementById('strategy-select');
        if (stratEl) stratEl.value = data.simulation.strategy_name;
    }
}

function updatePortfolioCard(portfolio) {
    if (!portfolio) return;

    const balance = portfolio.balance != null ? portfolio.balance : INITIAL_BALANCE;
    const pnl = portfolio.total_pnl || 0;
    const pnlPct = portfolio.pnl_percentage || 0;

    // Animate balance counter
    const balanceEl = document.getElementById('current-balance');
    if (balanceEl) {
        animateValue(balanceEl, previousBalance, balance, 800, true);
        previousBalance = balance;
        balanceEl.className = 'balance-value mono' + (balance >= INITIAL_BALANCE ? ' profit' : ' loss');
    }

    // Update P&L
    const pnlEl = document.getElementById('total-pnl');
    if (pnlEl) {
        pnlEl.textContent = formatCurrency(pnl);
        pnlEl.className = 'pnl-value mono ' + (pnl >= 0 ? 'positive' : 'negative');
    }

    const pnlPctEl = document.getElementById('pnl-pct');
    if (pnlPctEl) {
        pnlPctEl.textContent = `(${formatPercent(pnlPct)})`;
        pnlPctEl.className = 'pnl-pct mono ' + (pnl >= 0 ? 'positive' : 'negative');
    }

    // Update progress bar
    const progressPct = Math.min(Math.max((balance / (INITIAL_BALANCE * 2)) * 100, 2), 100);
    const barEl = document.getElementById('balance-bar');
    if (barEl) {
        barEl.style.width = progressPct + '%';
        barEl.className = 'progress-fill' + (balance < INITIAL_BALANCE ? ' loss' : '');
    }
}

function updateMetricsGrid(portfolio) {
    if (!portfolio) return;

    const setMetric = (id, value) => {
        const el = document.getElementById(id);
        if (el) el.textContent = value;
    };

    setMetric('total-trades', portfolio.total_trades || 0);
    setMetric('win-rate', formatPercent(portfolio.win_rate || 0));
    setMetric('max-drawdown', formatPercent(portfolio.max_drawdown || 0));
    setMetric('sharpe-ratio', (portfolio.sharpe_ratio || 0).toFixed(2));
    setMetric('additional-cost', formatCurrency(portfolio.total_fees || 0));

    // Best trade - can be derived from open_positions or separate field
    const bestTrade = portfolio.best_trade || 0;
    setMetric('best-trade', formatCurrency(bestTrade));

    // Color the win rate based on value
    const winRateEl = document.getElementById('win-rate');
    if (winRateEl) {
        const wr = portfolio.win_rate || 0;
        if (wr >= 60) winRateEl.className = 'metric-value mono text-green';
        else if (wr >= 40) winRateEl.className = 'metric-value mono text-cyan';
        else winRateEl.className = 'metric-value mono text-red';
    }

    // Color max drawdown
    const ddEl = document.getElementById('max-drawdown');
    if (ddEl) ddEl.className = 'metric-value mono text-red';

    // Color best trade
    const btEl = document.getElementById('best-trade');
    if (btEl) {
        btEl.className = 'metric-value mono ' + (bestTrade >= 0 ? 'text-green' : 'text-red');
    }
}

function updateTradeTable(trades) {
    const tbody = document.getElementById('trades-body');
    const emptyState = document.getElementById('trades-empty');
    const countEl = document.getElementById('trade-count');

    if (!tbody) return;

    if (!trades || trades.length === 0) {
        tbody.innerHTML = '';
        if (emptyState) emptyState.style.display = 'block';
        if (countEl) countEl.textContent = '';
        return;
    }

    if (emptyState) emptyState.style.display = 'none';
    if (countEl) countEl.textContent = `(${trades.length})`;

    // Check for new trades
    const hasNewTrades = trades.length !== previousTrades.length;
    previousTrades = trades;

    // Render trades in reverse chronological order
    const sortedTrades = trades.slice().reverse();

    tbody.innerHTML = sortedTrades.map((trade, i) => {
        const type = (trade.type || '').toUpperCase();
        const isBuy = type === 'BUY' || type === 'CALL';
        const typeClass = isBuy ? 'badge-buy' : 'badge-sell';
        const pnl = trade.pnl || 0;
        const pnlClass = pnl >= 0 ? 'trade-profit' : 'trade-loss';

        const entryTime = trade.entry_time
            ? new Date(trade.entry_time).toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' })
            : '-';

        const animClass = hasNewTrades && i === 0 ? ' style="animation: slide-in 0.4s ease"' : '';

        return `<tr${animClass}>
            <td class="mono">${entryTime}</td>
            <td><span class="badge ${typeClass}">${type || 'N/A'}</span></td>
            <td>${trade.symbol || '-'}</td>
            <td class="mono">₹${formatNumber(trade.entry_price || 0)}</td>
            <td class="mono">${trade.exit_price != null ? '₹' + formatNumber(trade.exit_price) : '-'}</td>
            <td class="mono ${pnlClass}">${formatCurrency(pnl)}</td>
            <td class="text-muted">${trade.strategy || '-'}</td>
        </tr>`;
    }).join('');
}

function updateOpenPositionsTable(positions) {
    const tbody = document.getElementById('open-positions-body');
    const emptyState = document.getElementById('open-positions-empty');
    const countEl = document.getElementById('open-positions-count');

    if (!tbody) return;

    if (!positions || positions.length === 0) {
        tbody.innerHTML = '';
        if (emptyState) emptyState.style.display = 'block';
        if (countEl) countEl.textContent = '';
        return;
    }

    if (emptyState) emptyState.style.display = 'none';
    if (countEl) countEl.textContent = `(${positions.length})`;

    tbody.innerHTML = positions.map((pos) => {
        const type = (pos.type || '').toUpperCase();
        const typeClass = type === 'CALL' ? 'badge-buy' : 'badge-sell';
        const pnl = pos.unrealized_pnl || 0;
        const pnlClass = pnl >= 0 ? 'trade-profit' : 'trade-loss';

        const entryTime = pos.timestamp
            ? new Date(pos.timestamp).toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' })
            : '-';

        return `<tr>
            <td class="mono">${entryTime}</td>
            <td><span class="badge ${typeClass}">${type || 'N/A'}</span></td>
            <td>${pos.symbol || '-'}</td>
            <td class="mono">₹${formatNumber(pos.entry_price || 0)}</td>
            <td class="mono ${pnlClass}">${formatCurrency(pnl)}</td>
            <td class="text-muted">${pos.strategy || '-'}</td>
        </tr>`;
    }).join('');
}

function updateMarketCards(symbols) {
    const container = document.getElementById('market-cards');
    if (!container) return;

    if (!symbols || symbols.length === 0) {
        container.innerHTML = '<div class="empty-state">No market data available</div>';
        return;
    }

    container.innerHTML = symbols.map(s => {
        const change = s.change || 0;
        const changePct = s.change_pct || 0;
        const isPositive = change >= 0;
        const changeClass = isPositive ? 'positive' : 'negative';

        const rsiVal = s.indicators?.rsi;
        const trendVal = s.indicators?.supertrend;
        const rsiText = rsiVal != null ? `RSI: ${Math.round(rsiVal)}` : '';
        const trendText = trendVal != null ? `Trend: ${trendVal}` : '';

        return `<div class="market-card ${changeClass}" onclick="changeSymbolFromCard('${s.symbol}')">
            <div>
                <div class="market-card-name">${s.name || s.symbol}</div>
                <div class="market-card-indicators">
                    ${rsiText ? `<span class="indicator-badge">${rsiText}</span>` : ''}
                    ${trendText ? `<span class="indicator-badge">${trendText}</span>` : ''}
                </div>
            </div>
            <div style="text-align: right">
                <div class="market-card-price mono">₹${formatNumber(s.price || 0)}</div>
                <div class="market-card-change ${changeClass} mono">${isPositive ? '+' : ''}${changePct.toFixed(2)}%</div>
            </div>
        </div>`;
    }).join('');
}

// Helper for market card click
function changeSymbolFromCard(symbol) {
    const select = document.getElementById('symbol-select');
    if (select) select.value = symbol;
    changeSymbol(symbol);
}

function updateSignalCards(signals) {
    const container = document.getElementById('signals-container');
    const emptyState = document.getElementById('signals-empty');
    const countEl = document.getElementById('signal-count');

    if (!container) return;

    if (!signals || signals.length === 0) {
        container.innerHTML = '';
        if (emptyState) emptyState.style.display = 'block';
        if (countEl) countEl.textContent = '';
        return;
    }

    if (emptyState) emptyState.style.display = 'none';
    if (countEl) countEl.textContent = `(${signals.length})`;

    container.innerHTML = signals.map(sig => {
        const signal = (sig.signal || '').toUpperCase();
        const isBuy = signal === 'BUY';
        const dirClass = isBuy ? 'buy' : 'sell';
        const confidence = sig.confidence || 0;
        const confPct = (confidence * 100).toFixed(0);
        const confClass = confidence >= 0.7 ? 'high' : confidence >= 0.4 ? 'medium' : 'low';

        const timeStr = sig.timestamp
            ? new Date(sig.timestamp).toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' })
            : '';

        return `<div class="signal-card ${dirClass}-signal">
            <div class="signal-header">
                <span class="signal-symbol">${sig.symbol || '-'}</span>
                <span class="signal-direction ${dirClass}">${isBuy ? '▲ BUY' : '▼ SELL'}</span>
            </div>
            <div class="signal-details">
                <span>${sig.strategy || '-'}</span>
                <span class="mono">₹${formatNumber(sig.price || 0)}</span>
            </div>
            <div class="signal-details" style="margin-top: 4px">
                <span class="text-muted">${timeStr}</span>
                <span class="text-muted">${confPct}% confidence</span>
            </div>
            <div class="confidence-bar">
                <div class="confidence-fill ${confClass}" style="width: ${confPct}%"></div>
            </div>
        </div>`;
    }).join('');
}

// ============================================================
// Chart Update Functions
// ============================================================

function updatePriceChart(candles) {
    if (!candles || candles.length === 0 || !priceChart) return;

    const labels = candles.map(c => {
        const d = new Date(c.time);
        return d.toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' });
    });

    const closes = candles.map(c => c.close);
    const volumes = candles.map(c => c.volume || 0);

    // Calculate technical indicators client-side
    const ema9 = calculateEMA(closes, 9);
    const ema21 = calculateEMA(closes, 21);
    const bb = calculateBollingerBands(closes, 20, 2);

    // Dynamic gradient based on price direction
    const ctx = priceChart.ctx;
    const chartArea = priceChart.chartArea;
    const height = chartArea ? chartArea.bottom - chartArea.top : 350;
    const top = chartArea ? chartArea.top : 0;

    const lastPrice = closes[closes.length - 1];
    const firstPrice = closes[0];
    const gradient = ctx.createLinearGradient(0, top, 0, top + height);

    if (lastPrice >= firstPrice) {
        gradient.addColorStop(0, 'rgba(0, 255, 136, 0.2)');
        gradient.addColorStop(0.6, 'rgba(0, 255, 136, 0.05)');
        gradient.addColorStop(1, 'rgba(0, 255, 136, 0)');
        priceChart.data.datasets[0].borderColor = '#00ff88';
    } else {
        gradient.addColorStop(0, 'rgba(255, 51, 102, 0.2)');
        gradient.addColorStop(0.6, 'rgba(255, 51, 102, 0.05)');
        gradient.addColorStop(1, 'rgba(255, 51, 102, 0)');
        priceChart.data.datasets[0].borderColor = '#ff3366';
    }
    priceChart.data.datasets[0].backgroundColor = gradient;

    // Set volume y2 max so bars take only ~20% of chart
    const maxVol = Math.max(...volumes, 1);
    priceChart.options.scales.y2.max = maxVol * 5;

    // Update data
    priceChart.data.labels = labels;
    priceChart.data.datasets[0].data = closes;
    priceChart.data.datasets[1].data = ema9;
    priceChart.data.datasets[2].data = ema21;
    priceChart.data.datasets[3].data = bb.upper;
    priceChart.data.datasets[4].data = bb.lower;
    priceChart.data.datasets[5].data = volumes;

    priceChart.update('none');
}

function updateEquityChart(curve) {
    if (!curve || curve.length === 0 || !equityChart) return;

    const labels = curve.map(c => {
        const d = new Date(c.timestamp);
        return d.toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' });
    });
    const balances = curve.map(c => c.balance);

    // Dynamic gradient
    const ctx = equityChart.ctx;
    const chartArea = equityChart.chartArea;
    const height = chartArea ? chartArea.bottom - chartArea.top : 200;
    const top = chartArea ? chartArea.top : 0;
    const lastBalance = balances[balances.length - 1];

    const gradient = ctx.createLinearGradient(0, top, 0, top + height);

    if (lastBalance >= INITIAL_BALANCE) {
        gradient.addColorStop(0, 'rgba(0, 255, 136, 0.25)');
        gradient.addColorStop(1, 'rgba(0, 255, 136, 0)');
        equityChart.data.datasets[0].borderColor = '#00ff88';
    } else {
        gradient.addColorStop(0, 'rgba(255, 51, 102, 0.25)');
        gradient.addColorStop(1, 'rgba(255, 51, 102, 0)');
        equityChart.data.datasets[0].borderColor = '#ff3366';
    }
    equityChart.data.datasets[0].backgroundColor = gradient;

    equityChart.data.labels = labels;
    equityChart.data.datasets[0].data = balances;
    equityChart.update('none');
}

// ============================================================
// Technical Indicator Calculations
// ============================================================

/**
 * Calculate Exponential Moving Average
 * @param {number[]} data - Price data array
 * @param {number} period - EMA period
 * @returns {(number|null)[]} EMA values (null for insufficient data points)
 */
function calculateEMA(data, period) {
    const k = 2 / (period + 1);
    const result = new Array(data.length).fill(null);

    if (data.length < period) return result;

    // Seed with SMA for the first period
    let sum = 0;
    for (let i = 0; i < period; i++) sum += data[i];
    result[period - 1] = sum / period;

    // Calculate EMA for remaining points
    for (let i = period; i < data.length; i++) {
        result[i] = data[i] * k + result[i - 1] * (1 - k);
    }

    return result;
}

/**
 * Calculate Bollinger Bands
 * @param {number[]} data - Price data array
 * @param {number} period - Moving average period (default 20)
 * @param {number} stdDevMultiplier - Standard deviation multiplier (default 2)
 * @returns {{upper: (number|null)[], lower: (number|null)[]}}
 */
function calculateBollingerBands(data, period = 20, stdDevMultiplier = 2) {
    const upper = new Array(data.length).fill(null);
    const lower = new Array(data.length).fill(null);

    if (data.length < period) return { upper, lower };

    for (let i = period - 1; i < data.length; i++) {
        const slice = data.slice(i - period + 1, i + 1);
        const mean = slice.reduce((a, b) => a + b, 0) / period;
        const variance = slice.reduce((a, b) => a + Math.pow(b - mean, 2), 0) / period;
        const std = Math.sqrt(variance);

        upper[i] = mean + stdDevMultiplier * std;
        lower[i] = mean - stdDevMultiplier * std;
    }

    return { upper, lower };
}

// ============================================================
// Formatting Helpers
// ============================================================

/**
 * Format amount as Indian Rupees: ₹X,XX,XXX.XX
 */
function formatCurrency(amount) {
    const abs = Math.abs(amount);
    const formatted = abs.toLocaleString('en-IN', {
        minimumFractionDigits: 2,
        maximumFractionDigits: 2
    });
    return (amount < 0 ? '-₹' : '₹') + formatted;
}

/**
 * Format as percentage with sign: +X.XX% or -X.XX%
 */
function formatPercent(value) {
    const sign = value >= 0 ? '+' : '';
    return sign + value.toFixed(2) + '%';
}

/**
 * Format number with Indian locale and 2 decimal places
 */
function formatNumber(num) {
    if (num === null || num === undefined || isNaN(num)) return '0.00';
    return Number(num).toLocaleString('en-IN', {
        minimumFractionDigits: 2,
        maximumFractionDigits: 2
    });
}

/**
 * Compact number format for axis labels
 */
function formatCompact(num) {
    if (num >= 100000) return (num / 100000).toFixed(1) + 'L';
    if (num >= 1000) return (num / 1000).toFixed(1) + 'K';
    return num.toLocaleString('en-IN', { maximumFractionDigits: 0 });
}

// ============================================================
// Animation
// ============================================================

/**
 * Animate a numeric value transition with easing
 * @param {HTMLElement} element - Target DOM element
 * @param {number} start - Start value
 * @param {number} end - End value
 * @param {number} duration - Animation duration in ms
 * @param {boolean} isCurrency - Format as currency
 */
function animateValue(element, start, end, duration, isCurrency = false) {
    if (!element) return;
    if (Math.abs(start - end) < 0.01) {
        element.textContent = isCurrency ? formatCurrency(end) : end.toString();
        return;
    }

    const startTime = performance.now();
    const diff = end - start;

    function update(currentTime) {
        const elapsed = currentTime - startTime;
        const progress = Math.min(elapsed / duration, 1);

        // Ease out cubic for smooth deceleration
        const eased = 1 - Math.pow(1 - progress, 3);
        const current = start + diff * eased;

        element.textContent = isCurrency ? formatCurrency(current) : Math.round(current).toString();

        if (progress < 1) {
            requestAnimationFrame(update);
        } else {
            // Ensure final value is exact
            element.textContent = isCurrency ? formatCurrency(end) : end.toString();
        }
    }

    requestAnimationFrame(update);
}

// ============================================================
// Connection Status
// ============================================================

function setConnected(status) {
    if (connected === status) return;
    connected = status;

    const dot = document.querySelector('.status-dot');
    const statusEl = document.getElementById('connection-status');

    if (dot) {
        dot.className = 'status-dot' + (status ? '' : ' disconnected');
    }

    if (statusEl) {
        const spans = statusEl.querySelectorAll('span:not(.status-dot)');
        spans.forEach(span => {
            span.textContent = status ? 'Connected' : 'Disconnected';
        });
    }
}

// ============================================================
// Toast Notifications
// ============================================================

/**
 * Show a toast notification
 * @param {string} message - Notification message
 * @param {'success'|'error'|'info'} type - Toast type
 */
function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;

    const icons = {
        success: '✓',
        error: '✕',
        info: 'ℹ'
    };

    toast.innerHTML = `<span style="font-weight:700">${icons[type] || 'ℹ'}</span> ${escapeHtml(message)}`;
    container.appendChild(toast);

    // Auto-remove after 4 seconds
    setTimeout(() => {
        toast.classList.add('removing');
        setTimeout(() => {
            if (toast.parentNode) toast.remove();
        }, 300);
    }, 4000);
}

/**
 * Escape HTML to prevent XSS
 */
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// ============================================================
// Data Streams & Polling
// ============================================================

function initEventStream() {
    const eventSource = new EventSource(`${API_BASE}/api/simulation/stream`);

    eventSource.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            
            updateSimulationState(data);
            
            if (data.portfolio) {
                updatePortfolioCard(data.portfolio);
                updateMetricsGrid(data.portfolio);
            }
            
            if (data.trades) {
                updateTradeTable(data.trades);
            }
            
            if (data.equity_curve) {
                updateEquityChart(data.equity_curve);
            }
            
            if (data.open_positions) {
                updateOpenPositionsTable(data.open_positions);
            }
            
            if (data.market_data) {
                updateMarketCards(data.market_data);
            }
            
            setConnected(true);
        } catch (err) {
            console.error('SSE data parse error:', err);
        }
    };

    eventSource.onerror = function(err) {
        console.error('SSE connection error:', err);
        setConnected(false);
    };

    // Keep polling chart data separately since we don't want to re-render the whole chart every second
    setInterval(() => {
        fetchChartData(currentSymbol);
    }, POLL_INTERVAL_MARKET);
}

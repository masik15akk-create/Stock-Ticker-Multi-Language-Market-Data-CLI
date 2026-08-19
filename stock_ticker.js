// stock_ticker.js
const axios = require('axios');
const chalk = require('chalk');
const readline = require('readline');
const fs = require('fs');

const API_KEY = process.env.ALPHA_VANTAGE_KEY || 'demo';
const BASE_URL = 'https://www.alphavantage.co/query';

const cache = new Map();
const CACHE_TTL = 60000; // 60 seconds

async function fetchQuote(symbol) {
    const now = Date.now();
    if (cache.has(symbol) && (now - cache.get(symbol).timestamp) < CACHE_TTL) {
        return cache.get(symbol).data;
    }
    try {
        const resp = await axios.get(BASE_URL, {
            params: {
                function: 'GLOBAL_QUOTE',
                symbol: symbol,
                apikey: API_KEY
            },
            timeout: 10000
        });
        const quote = resp.data['Global Quote'];
        if (!quote || !quote['01. symbol']) {
            throw new Error(`No data for ${symbol}`);
        }
        const result = {
            symbol: quote['01. symbol'],
            open: parseFloat(quote['02. open']),
            high: parseFloat(quote['03. high']),
            low: parseFloat(quote['04. low']),
            price: parseFloat(quote['05. price']),
            volume: parseInt(quote['06. volume']),
            change: parseFloat(quote['09. change']),
            changePercent: parseFloat(quote['10. change percent'].replace('%', ''))
        };
        cache.set(symbol, { data: result, timestamp: now });
        return result;
    } catch (err) {
        console.error(`Error fetching ${symbol}: ${err.message}`);
        return null;
    }
}

function printQuotes(quotes, alertPrice) {
    console.log('\n' + chalk.cyan('Symbol    Price   Change   %Change   Open    High     Low      Volume'));
    console.log('-'.repeat(80));
    for (const q of quotes) {
        if (!q) continue;
        const color = q.change >= 0 ? chalk.green : chalk.red;
        const arrow = q.change >= 0 ? '▲' : '▼';
        console.log(
            `${q.symbol.padEnd(8)} ${chalk.yellow(q.price.toFixed(2).padStart(8))} ` +
            `${color(q.change >= 0 ? '+' : '') + q.change.toFixed(2).padStart(8)} ` +
            `${color((q.changePercent >= 0 ? '+' : '') + q.changePercent.toFixed(2) + '%').padStart(8)} ` +
            `${q.open.toFixed(2).padStart(8)} ${q.high.toFixed(2).padStart(8)} ${q.low.toFixed(2).padStart(8)} ` +
            `${q.volume.toLocaleString().padStart(12)}`
        );
        if (alertPrice && q.price >= alertPrice) {
            console.log(chalk.red(`*** ALERT: ${q.symbol} crossed $${alertPrice} (current $${q.price})`));
        }
    }
}

async function main() {
    const args = require('minimist')(process.argv.slice(2), {
        string: ['s', 'o'],
        default: { i: 5, a: 0 },
        alias: { s: 'symbols', i: 'interval', a: 'alert' }
    });
    if (!args.s) {
        console.error('Error: -s symbols required');
        process.exit(1);
    }
    const symbols = args.s.split(',').map(s => s.trim().toUpperCase());
    const interval = parseInt(args.i) || 5;
    const alertPrice = parseFloat(args.a) || 0;

    // clear screen and move cursor
    readline.cursorTo(process.stdout, 0, 0);
    readline.clearScreenDown(process.stdout);

    while (true) {
        const quotes = [];
        for (const sym of symbols) {
            const q = await fetchQuote(sym);
            quotes.push(q);
        }
        printQuotes(quotes, alertPrice);
        if (interval <= 0) break;
        await new Promise(r => setTimeout(r, interval * 1000));
    }
}

main().catch(console.error);

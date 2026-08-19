# stock_ticker.py
import sys, os, time, json, argparse, threading
from datetime import datetime
try:
    import requests
except ImportError:
    print("Install requests: pip install requests")
    sys.exit(1)

try:
    from colorama import init, Fore, Style
    init()
    COLORS = True
except ImportError:
    COLORS = False
    Fore = Style = type('', (), {'RESET_ALL':'', 'GREEN':'', 'RED':'', 'YELLOW':'', 'CYAN':''})()

API_KEY = os.environ.get("ALPHA_VANTAGE_KEY", "demo")
BASE_URL = "https://www.alphavantage.co/query"

CACHE = {}
CACHE_TTL = 60  # seconds

def fetch_quote(symbol):
    now = time.time()
    if symbol in CACHE and now - CACHE[symbol]['time'] < CACHE_TTL:
        return CACHE[symbol]['data']
    params = {
        'function': 'GLOBAL_QUOTE',
        'symbol': symbol,
        'apikey': API_KEY
    }
    try:
        resp = requests.get(BASE_URL, params=params, timeout=10)
        data = resp.json()
        quote = data.get('Global Quote', {})
        if not quote:
            print(f"Error: No data for {symbol}", file=sys.stderr)
            return None
        # Parse fields
        result = {
            'symbol': quote.get('01. symbol', symbol),
            'open': float(quote.get('02. open', 0)),
            'high': float(quote.get('03. high', 0)),
            'low': float(quote.get('04. low', 0)),
            'price': float(quote.get('05. price', 0)),
            'volume': int(float(quote.get('06. volume', 0))),
            'change': float(quote.get('09. change', 0)),
            'change_percent': float(quote.get('10. change percent', '0%').replace('%', ''))
        }
        CACHE[symbol] = {'data': result, 'time': now}
        return result
    except Exception as e:
        print(f"Error fetching {symbol}: {e}", file=sys.stderr)
        return None

def print_quotes(quotes, alert_price=None):
    # Header
    print(f"\n{Fore.CYAN}{'Symbol':<8} {'Price':>8} {'Change':>8} {'%Change':>8} {'Open':>8} {'High':>8} {'Low':>8} {'Volume':>12}{Style.RESET_ALL}")
    print("-" * 80)
    for q in quotes:
        if not q: continue
        sym = q['symbol']
        price = q['price']
        change = q['change']
        pct = q['change_percent']
        color = Fore.GREEN if change >= 0 else Fore.RED
        arrow = '▲' if change >= 0 else '▼'
        if COLORS:
            print(f"{sym:<8} {Fore.YELLOW}{price:>8.2f}{Style.RESET_ALL} {color}{change:>+8.2f}{Style.RESET_ALL} {color}{pct:>+7.2f}%{Style.RESET_ALL} {q['open']:>8.2f} {q['high']:>8.2f} {q['low']:>8.2f} {q['volume']:>12,}")
        else:
            print(f"{sym:<8} {price:>8.2f} {change:>+8.2f} {pct:>+7.2f}% {q['open']:>8.2f} {q['high']:>8.2f} {q['low']:>8.2f} {q['volume']:>12,}")
        # Alert check
        if alert_price and price >= alert_price:
            print(f"{Fore.RED}*** ALERT: {sym} crossed ${alert_price} (current ${price}){Style.RESET_ALL}")

def main():
    parser = argparse.ArgumentParser(description="Stock Ticker CLI")
    parser.add_argument('-s', '--symbols', required=True, help='Comma-separated symbols')
    parser.add_argument('-i', '--interval', type=int, default=5, help='Refresh interval (seconds)')
    parser.add_argument('-a', '--alert', type=float, help='Alert threshold price')
    parser.add_argument('--spark', action='store_true', help='Show sparkline (not implemented)')
    parser.add_argument('--csv', action='store_true', help='Append to history.csv')
    args = parser.parse_args()

    symbols = [s.strip().upper() for s in args.symbols.split(',')]

    try:
        while True:
            quotes = []
            for sym in symbols:
                q = fetch_quote(sym)
                if q:
                    quotes.append(q)
                    # Optionally save to CSV
                    if args.csv:
                        with open('history.csv', 'a') as f:
                            f.write(f"{datetime.now()},{sym},{q['price']},{q['change']},{q['change_percent']}\n")
                else:
                    quotes.append(None)
            print_quotes(quotes, args.alert)
            if args.interval <= 0:
                break
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nExiting...")

if __name__ == '__main__':
    main()

// stock_ticker.go
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"
)

type Quote struct {
	Symbol           string  `json:"01. symbol"`
	Open             string  `json:"02. open"`
	High             string  `json:"03. high"`
	Low              string  `json:"04. low"`
	Price            string  `json:"05. price"`
	Volume           string  `json:"06. volume"`
	Change           string  `json:"09. change"`
	ChangePercent    string  `json:"10. change percent"`
}

type APIResponse struct {
	GlobalQuote Quote `json:"Global Quote"`
}

var apiKey = os.Getenv("ALPHA_VANTAGE_KEY")
var cache = make(map[string]cacheEntry)
const cacheTTL = 60 * time.Second

type cacheEntry struct {
	data      Quote
	timestamp time.Time
}

func fetchQuote(symbol string) (*Quote, error) {
	if apiKey == "" {
		apiKey = "demo"
	}
	if entry, ok := cache[symbol]; ok && time.Since(entry.timestamp) < cacheTTL {
		return &entry.data, nil
	}
	url := fmt.Sprintf("https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=%s&apikey=%s", symbol, apiKey)
	resp, err := http.Get(url)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var result APIResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, err
	}
	if result.GlobalQuote.Symbol == "" {
		return nil, fmt.Errorf("no data for %s", symbol)
	}
	cache[symbol] = cacheEntry{data: result.GlobalQuote, timestamp: time.Now()}
	return &result.GlobalQuote, nil
}

func printQuotes(quotes []*Quote, alert float64) {
	fmt.Printf("\n%-8s %8s %8s %8s %8s %8s %8s %12s\n", "Symbol", "Price", "Change", "%Change", "Open", "High", "Low", "Volume")
	fmt.Println(strings.Repeat("-", 80))
	for _, q := range quotes {
		if q == nil { continue }
		price, _ := strconv.ParseFloat(q.Price, 64)
		change, _ := strconv.ParseFloat(q.Change, 64)
		pct, _ := strconv.ParseFloat(strings.TrimSuffix(q.ChangePercent, "%"), 64)
		volume, _ := strconv.ParseFloat(q.Volume, 64)
		color := "\033[32m" // green
		if change < 0 {
			color = "\033[31m" // red
		}
		fmt.Printf("%-8s \033[33m%8.2f\033[0m %s%+8.2f\033[0m %s%+7.2f%%\033[0m %8.2f %8.2f %8.2f %12.0f\n",
			q.Symbol, price, color, change, color, pct,
			mustFloat(q.Open), mustFloat(q.High), mustFloat(q.Low), volume)
		if alert > 0 && price >= alert {
			fmt.Printf("\033[31m*** ALERT: %s crossed $%.2f (current $%.2f)\033[0m\n", q.Symbol, alert, price)
		}
	}
}

func mustFloat(s string) float64 {
	f, _ := strconv.ParseFloat(s, 64)
	return f
}

func main() {
	symbolsStr := flag.String("s", "", "Comma-separated symbols")
	interval := flag.Int("i", 5, "Refresh interval (seconds)")
	alert := flag.Float64("a", 0, "Alert threshold")
	flag.Parse()
	if *symbolsStr == "" {
		fmt.Fprintln(os.Stderr, "Error: -s symbols required")
		os.Exit(1)
	}
	symbols := strings.Split(*symbolsStr, ",")
	for i := range symbols {
		symbols[i] = strings.TrimSpace(strings.ToUpper(symbols[i]))
	}
	for {
		quotes := []*Quote{}
		for _, sym := range symbols {
			q, err := fetchQuote(sym)
			if err != nil {
				fmt.Fprintf(os.Stderr, "Error: %v\n", err)
				quotes = append(quotes, nil)
			} else {
				quotes = append(quotes, q)
			}
		}
		printQuotes(quotes, *alert)
		if *interval <= 0 {
			break
		}
		time.Sleep(time.Duration(*interval) * time.Second)
	}
}

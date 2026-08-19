// StockTicker.cs
using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;
using System.Linq;

class StockTicker
{
    private static readonly string ApiKey = Environment.GetEnvironmentVariable("ALPHA_VANTAGE_KEY") ?? "demo";
    private static readonly string BaseUrl = "https://www.alphavantage.co/query";
    private static readonly HttpClient client = new HttpClient() { Timeout = TimeSpan.FromSeconds(10) };
    private static readonly Dictionary<string, CacheEntry> cache = new Dictionary<string, CacheEntry>();
    private static readonly int CacheTtl = 60; // seconds

    class CacheEntry
    {
        public Quote Data { get; set; }
        public DateTime Timestamp { get; set; }
    }

    class Quote
    {
        public string Symbol { get; set; }
        public double Open { get; set; }
        public double High { get; set; }
        public double Low { get; set; }
        public double Price { get; set; }
        public long Volume { get; set; }
        public double Change { get; set; }
        public double ChangePercent { get; set; }
    }

    class ApiResponse
    {
        public Quote GlobalQuote { get; set; }
    }

    static async Task<Quote> FetchQuote(string symbol)
    {
        lock (cache)
        {
            if (cache.ContainsKey(symbol) && (DateTime.Now - cache[symbol].Timestamp).TotalSeconds < CacheTtl)
                return cache[symbol].Data;
        }
        string url = $"{BaseUrl}?function=GLOBAL_QUOTE&symbol={symbol}&apikey={ApiKey}";
        var resp = await client.GetAsync(url);
        resp.EnsureSuccessStatusCode();
        string json = await resp.Content.ReadAsStringAsync();
        var options = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };
        var data = JsonSerializer.Deserialize<Dictionary<string, JsonElement>>(json);
        if (!data.ContainsKey("Global Quote") || data["Global Quote"].ValueKind == JsonValueKind.Null)
            throw new Exception($"No data for {symbol}");
        var quoteElem = data["Global Quote"];
        var quote = new Quote
        {
            Symbol = quoteElem.GetProperty("01. symbol").GetString(),
            Open = double.Parse(quoteElem.GetProperty("02. open").GetString()),
            High = double.Parse(quoteElem.GetProperty("03. high").GetString()),
            Low = double.Parse(quoteElem.GetProperty("04. low").GetString()),
            Price = double.Parse(quoteElem.GetProperty("05. price").GetString()),
            Volume = long.Parse(quoteElem.GetProperty("06. volume").GetString()),
            Change = double.Parse(quoteElem.GetProperty("09. change").GetString()),
            ChangePercent = double.Parse(quoteElem.GetProperty("10. change percent").GetString().Replace("%", ""))
        };
        lock (cache)
        {
            cache[symbol] = new CacheEntry { Data = quote, Timestamp = DateTime.Now };
        }
        return quote;
    }

    static void PrintQuotes(List<Quote> quotes, double alert)
    {
        Console.WriteLine("\n\u001b[36mSymbol    Price   Change   %Change   Open    High     Low      Volume\u001b[0m");
        Console.WriteLine(new string('-', 80));
        foreach (var q in quotes)
        {
            if (q == null) continue;
            string color = q.Change >= 0 ? "\u001b[32m" : "\u001b[31m";
            string sign = q.Change >= 0 ? "+" : "";
            Console.WriteLine($"{q.Symbol,-8} \u001b[33m{q.Price,8:F2}\u001b[0m {color}{sign}{q.Change,8:F2}\u001b[0m {color}{sign}{q.ChangePercent,7:F2}%\u001b[0m {q.Open,8:F2} {q.High,8:F2} {q.Low,8:F2} {q.Volume,12:N0}");
            if (alert > 0 && q.Price >= alert)
                Console.WriteLine($"\u001b[31m*** ALERT: {q.Symbol} crossed ${alert} (current ${q.Price})\u001b[0m");
        }
    }

    static async Task Main(string[] args)
    {
        var parsed = ParseArgs(args);
        if (!parsed.ContainsKey("s"))
        {
            Console.Error.WriteLine("Error: -s symbols required");
            return;
        }
        var symbols = parsed["s"].Split(',').Select(s => s.Trim().ToUpper()).ToList();
        int interval = int.Parse(parsed.GetValueOrDefault("i", "5"));
        double alert = double.Parse(parsed.GetValueOrDefault("a", "0"));

        while (true)
        {
            var tasks = symbols.Select(sym => FetchQuote(sym).ContinueWith(t => t.Result));
            var quotes = (await Task.WhenAll(tasks)).ToList();
            PrintQuotes(quotes, alert);
            if (interval <= 0) break;
            await Task.Delay(interval * 1000);
        }
    }

    static Dictionary<string, string> ParseArgs(string[] args)
    {
        var dict = new Dictionary<string, string>();
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i].StartsWith("-"))
            {
                string key = args[i].TrimStart('-');
                if (i + 1 < args.Length && !args[i + 1].StartsWith("-"))
                    dict[key] = args[++i];
                else
                    dict[key] = "";
            }
        }
        return dict;
    }
}

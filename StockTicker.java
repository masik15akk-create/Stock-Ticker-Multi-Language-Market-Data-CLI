// StockTicker.java
import java.net.http.*;
import java.net.URI;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.*;
import java.util.stream.*;
import com.google.gson.*; // or use org.json, but we'll use Gson for simplicity (add via Maven)

public class StockTicker {
    private static final String API_KEY = System.getenv().getOrDefault("ALPHA_VANTAGE_KEY", "demo");
    private static final String BASE_URL = "https://www.alphavantage.co/query";
    private static final int CACHE_TTL = 60;
    private static final Map<String, CacheEntry> cache = new ConcurrentHashMap<>();
    private static final HttpClient client = HttpClient.newHttpClient();
    private static final Gson gson = new Gson();

    static class CacheEntry {
        Quote data;
        long timestamp;
        CacheEntry(Quote data) { this.data = data; this.timestamp = System.currentTimeMillis(); }
    }

    static class Quote {
        String symbol; double open, high, low, price, change, changePercent; long volume;
        // Gson deserialization
        public void setSymbol(String s) { this.symbol = s; }
        public void setOpen(String s) { this.open = Double.parseDouble(s); }
        public void setHigh(String s) { this.high = Double.parseDouble(s); }
        public void setLow(String s) { this.low = Double.parseDouble(s); }
        public void setPrice(String s) { this.price = Double.parseDouble(s); }
        public void setVolume(String s) { this.volume = Long.parseLong(s); }
        public void setChange(String s) { this.change = Double.parseDouble(s); }
        public void setChangePercent(String s) { this.changePercent = Double.parseDouble(s.replace("%", "")); }
    }

    static class ApiResponse {
        Quote GlobalQuote;
    }

    private static Quote fetchQuote(String symbol) throws Exception {
        long now = System.currentTimeMillis();
        if (cache.containsKey(symbol) && (now - cache.get(symbol).timestamp) < CACHE_TTL * 1000) {
            return cache.get(symbol).data;
        }
        String url = BASE_URL + "?function=GLOBAL_QUOTE&symbol=" + symbol + "&apikey=" + API_KEY;
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(url))
                .timeout(Duration.ofSeconds(10))
                .build();
        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
        ApiResponse resp = gson.fromJson(response.body(), ApiResponse.class);
        if (resp.GlobalQuote == null || resp.GlobalQuote.symbol == null) {
            throw new Exception("No data for " + symbol);
        }
        cache.put(symbol, new CacheEntry(resp.GlobalQuote));
        return resp.GlobalQuote;
    }

    private static void printQuotes(List<Quote> quotes, double alert) {
        System.out.println("\n\033[36mSymbol    Price   Change   %Change   Open    High     Low      Volume\033[0m");
        System.out.println("-".repeat(80));
        for (Quote q : quotes) {
            if (q == null) continue;
            String color = q.change >= 0 ? "\033[32m" : "\033[31m";
            String sign = q.change >= 0 ? "+" : "";
            System.out.printf("%-8s \033[33m%8.2f\033[0m %s%+8.2f\033[0m %s%+7.2f%%\033[0m %8.2f %8.2f %8.2f %12d%n",
                q.symbol, q.price, color, sign + q.change, color, sign + q.changePercent,
                q.open, q.high, q.low, q.volume);
            if (alert > 0 && q.price >= alert) {
                System.out.println("\033[31m*** ALERT: " + q.symbol + " crossed $" + alert + " (current $" + q.price + ")\033[0m");
            }
        }
    }

    public static void main(String[] args) throws Exception {
        Map<String, String> params = parseArgs(args);
        if (!params.containsKey("s")) {
            System.err.println("Error: -s symbols required");
            System.exit(1);
        }
        List<String> symbols = Arrays.stream(params.get("s").split(","))
                .map(String::trim).map(String::toUpperCase).collect(Collectors.toList());
        int interval = Integer.parseInt(params.getOrDefault("i", "5"));
        double alert = Double.parseDouble(params.getOrDefault("a", "0"));

        while (true) {
            List<Quote> quotes = new ArrayList<>();
            for (String sym : symbols) {
                try {
                    quotes.add(fetchQuote(sym));
                } catch (Exception e) {
                    System.err.println("Error: " + e.getMessage());
                    quotes.add(null);
                }
            }
            printQuotes(quotes, alert);
            if (interval <= 0) break;
            Thread.sleep(interval * 1000L);
        }
    }

    private static Map<String, String> parseArgs(String[] args) {
        Map<String, String> map = new HashMap<>();
        for (int i = 0; i < args.length; i++) {
            if (args[i].startsWith("-")) {
                String key = args[i].replaceFirst("^-+", "");
                if (i + 1 < args.length && !args[i + 1].startsWith("-")) {
                    map.put(key, args[++i]);
                } else {
                    map.put(key, "");
                }
            }
        }
        return map;
    }
}

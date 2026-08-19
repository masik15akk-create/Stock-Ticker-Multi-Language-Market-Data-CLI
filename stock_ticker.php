# stock_ticker.php
<?php
$apiKey = getenv('ALPHA_VANTAGE_KEY') ?: 'demo';
$baseUrl = "https://www.alphavantage.co/query";

$opts = getopt("s:i:a:", ["symbols:", "interval:", "alert:"]);
if (!isset($opts['s']) && !isset($opts['symbols'])) {
    fwrite(STDERR, "Error: -s symbols required\n");
    exit(1);
}
$symbolsStr = $opts['s'] ?? $opts['symbols'];
$symbols = array_map('trim', explode(',', $symbolsStr));
$interval = (int)($opts['i'] ?? $opts['interval'] ?? 5);
$alert = (float)($opts['a'] ?? $opts['alert'] ?? 0);

$cache = [];
$cacheTTL = 60;

function fetchQuote($symbol) {
    global $apiKey, $baseUrl, $cache, $cacheTTL;
    $now = time();
    if (isset($cache[$symbol]) && ($now - $cache[$symbol]['time']) < $cacheTTL) {
        return $cache[$symbol]['data'];
    }
    $url = "$baseUrl?function=GLOBAL_QUOTE&symbol=$symbol&apikey=$apiKey";
    $resp = file_get_contents($url);
    if ($resp === false) return null;
    $data = json_decode($resp, true);
    $quote = $data['Global Quote'] ?? null;
    if (!$quote || empty($quote['01. symbol'])) return null;
    $result = [
        'symbol' => $quote['01. symbol'],
        'open' => (float)$quote['02. open'],
        'high' => (float)$quote['03. high'],
        'low' => (float)$quote['04. low'],
        'price' => (float)$quote['05. price'],
        'volume' => (int)$quote['06. volume'],
        'change' => (float)$quote['09. change'],
        'change_percent' => (float)str_replace('%', '', $quote['10. change percent'])
    ];
    $cache[$symbol] = ['data' => $result, 'time' => $now];
    return $result;
}

function printQuotes($quotes, $alert) {
    printf("\n\033[36m%-8s %8s %8s %8s %8s %8s %8s %12s\033[0m\n",
        "Symbol", "Price", "Change", "%Change", "Open", "High", "Low", "Volume");
    echo str_repeat("-", 80) . "\n";
    foreach ($quotes as $q) {
        if (!$q) continue;
        $color = $q['change'] >= 0 ? "\033[32m" : "\033[31m";
        $sign = $q['change'] >= 0 ? '+' : '';
        printf("%-8s \033[33m%8.2f\033[0m %s%+8.2f\033[0m %s%+7.2f%%\033[0m %8.2f %8.2f %8.2f %12d\n",
            $q['symbol'], $q['price'],
            $color, $sign . $q['change'],
            $color, $sign . $q['change_percent'],
            $q['open'], $q['high'], $q['low'], $q['volume']
        );
        if ($alert > 0 && $q['price'] >= $alert) {
            echo "\033[31m*** ALERT: {$q['symbol']} crossed $${alert} (current $${q['price']})\033[0m\n";
        }
    }
}

while (true) {
    $quotes = array_map('fetchQuote', $symbols);
    printQuotes($quotes, $alert);
    if ($interval <= 0) break;
    sleep($interval);
}
?>

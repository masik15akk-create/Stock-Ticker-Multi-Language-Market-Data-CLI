// stock_ticker.cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <curl/curl.h>      // libcurl
#include <nlohmann/json.hpp> // https://github.com/nlohmann/json

using namespace std;
using json = nlohmann::json;

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string fetchUrl(const string& url) {
    CURL *curl = curl_easy_init();
    string response;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) return "";
    }
    return response;
}

struct Quote {
    string symbol;
    double open, high, low, price, change, changePercent;
    long volume;
};

map<string, pair<Quote, time_t>> cache;
const int CACHE_TTL = 60;

Quote fetchQuote(const string& symbol) {
    time_t now = time(nullptr);
    if (cache.count(symbol) && difftime(now, cache[symbol].second) < CACHE_TTL) {
        return cache[symbol].first;
    }
    string apiKey = getenv("ALPHA_VANTAGE_KEY") ? getenv("ALPHA_VANTAGE_KEY") : "demo";
    string url = "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=" + symbol + "&apikey=" + apiKey;
    string resp = fetchUrl(url);
    if (resp.empty()) throw runtime_error("Failed to fetch " + symbol);
    auto data = json::parse(resp);
    if (!data.contains("Global Quote") || data["Global Quote"].is_null()) {
        throw runtime_error("No data for " + symbol);
    }
    auto q = data["Global Quote"];
    Quote quote;
    quote.symbol = q["01. symbol"];
    quote.open = stod(q["02. open"].get<string>());
    quote.high = stod(q["03. high"].get<string>());
    quote.low = stod(q["04. low"].get<string>());
    quote.price = stod(q["05. price"].get<string>());
    quote.volume = stol(q["06. volume"].get<string>());
    quote.change = stod(q["09. change"].get<string>());
    string pct = q["10. change percent"].get<string>();
    pct.pop_back(); // remove '%'
    quote.changePercent = stod(pct);
    cache[symbol] = make_pair(quote, now);
    return quote;
}

void printQuotes(const vector<Quote>& quotes, double alert) {
    cout << "\n\033[36mSymbol    Price   Change   %Change   Open    High     Low      Volume\033[0m\n";
    cout << string(80, '-') << "\n";
    for (const auto& q : quotes) {
        string color = q.change >= 0 ? "\033[32m" : "\033[31m";
        string sign = q.change >= 0 ? "+" : "";
        cout << setw(8) << left << q.symbol
             << " \033[33m" << fixed << setprecision(2) << setw(8) << q.price << "\033[0m "
             << color << sign << setw(8) << q.change << "\033[0m "
             << color << sign << setw(7) << q.changePercent << "%\033[0m "
             << setw(8) << q.open << " " << setw(8) << q.high << " " << setw(8) << q.low << " "
             << setw(12) << q.volume << "\n";
        if (alert > 0 && q.price >= alert) {
            cout << "\033[31m*** ALERT: " << q.symbol << " crossed $" << alert << " (current $" << q.price << ")\033[0m\n";
        }
    }
}

int main(int argc, char* argv[]) {
    string symbolsStr, intervalStr = "5", alertStr = "0";
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if ((arg == "-s" || arg == "--symbols") && i+1 < argc) symbolsStr = argv[++i];
        else if ((arg == "-i" || arg == "--interval") && i+1 < argc) intervalStr = argv[++i];
        else if ((arg == "-a" || arg == "--alert") && i+1 < argc) alertStr = argv[++i];
    }
    if (symbolsStr.empty()) {
        cerr << "Error: -s symbols required\n";
        return 1;
    }
    vector<string> symbols;
    stringstream ss(symbolsStr);
    string token;
    while (getline(ss, token, ',')) {
        token.erase(0, token.find_first_not_of(" "));
        token.erase(token.find_last_not_of(" ") + 1);
        transform(token.begin(), token.end(), token.begin(), ::toupper);
        symbols.push_back(token);
    }
    int interval = stoi(intervalStr);
    double alert = stod(alertStr);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    while (true) {
        vector<Quote> quotes;
        for (const auto& sym : symbols) {
            try {
                quotes.push_back(fetchQuote(sym));
            } catch (const exception& e) {
                cerr << "Error: " << e.what() << "\n";
            }
        }
        printQuotes(quotes, alert);
        if (interval <= 0) break;
        this_thread::sleep_for(chrono::seconds(interval));
    }
    curl_global_cleanup();
    return 0;
}

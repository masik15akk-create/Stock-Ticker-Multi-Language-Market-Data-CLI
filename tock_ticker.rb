# stock_ticker.rb
require 'httparty'
require 'colorize'
require 'optparse'
require 'json'

API_KEY = ENV['ALPHA_VANTAGE_KEY'] || 'demo'
BASE_URL = 'https://www.alphavantage.co/query'
CACHE = {}
CACHE_TTL = 60

def fetch_quote(symbol)
  now = Time.now.to_i
  if CACHE[symbol] && (now - CACHE[symbol][:time]) < CACHE_TTL
    return CACHE[symbol][:data]
  end
  response = HTTParty.get(BASE_URL, query: {
    function: 'GLOBAL_QUOTE',
    symbol: symbol,
    apikey: API_KEY
  })
  quote = response['Global Quote']
  return nil unless quote && quote['01. symbol']
  result = {
    symbol: quote['01. symbol'],
    open: quote['02. open'].to_f,
    high: quote['03. high'].to_f,
    low: quote['04. low'].to_f,
    price: quote['05. price'].to_f,
    volume: quote['06. volume'].to_i,
    change: quote['09. change'].to_f,
    change_percent: quote['10. change percent'].to_f
  }
  CACHE[symbol] = { data: result, time: now }
  result
rescue => e
  warn "Error fetching #{symbol}: #{e.message}"
  nil
end

def print_quotes(quotes, alert_price)
  puts "\n#{'Symbol'.ljust(8)} #{'Price'.rjust(8)} #{'Change'.rjust(8)} #{'%Change'.rjust(8)} #{'Open'.rjust(8)} #{'High'.rjust(8)} #{'Low'.rjust(8)} #{'Volume'.rjust(12)}".cyan
  puts '-' * 80
  quotes.each do |q|
    next unless q
    color = q[:change] >= 0 ? :green : :red
    print "#{q[:symbol].ljust(8)} "
    print "#{q[:price].round(2).to_s.rjust(8)} ".yellow
    print "#{(q[:change] >= 0 ? '+' : '') + q[:change].round(2).to_s.rjust(8)} ".colorize(color)
    print "#{(q[:change_percent] >= 0 ? '+' : '') + q[:change_percent].round(2).to_s + '%'} ".rjust(8).colorize(color)
    print "#{q[:open].round(2).to_s.rjust(8)} "
    print "#{q[:high].round(2).to_s.rjust(8)} "
    print "#{q[:low].round(2).to_s.rjust(8)} "
    puts "#{q[:volume].to_s.rjust(12)}"
    if alert_price && q[:price] >= alert_price
      puts "*** ALERT: #{q[:symbol]} crossed $#{alert_price} (current $#{q[:price]})".red
    end
  end
end

options = {}
OptionParser.new do |opts|
  opts.banner = "Usage: ruby stock_ticker.rb -s SYMBOLS [options]"
  opts.on("-s SYMBOLS", "--symbols SYMBOLS", "Comma-separated symbols") { |v| options[:symbols] = v }
  opts.on("-i SECONDS", "--interval SECONDS", Integer, "Refresh interval") { |v| options[:interval] = v }
  opts.on("-a PRICE", "--alert PRICE", Float, "Alert threshold") { |v| options[:alert] = v }
end.parse!

unless options[:symbols]
  warn "Error: -s symbols required"
  exit 1
end

symbols = options[:symbols].split(',').map(&:strip).map(&:upcase)
interval = options[:interval] || 5
alert_price = options[:alert] || 0

loop do
  quotes = symbols.map { |sym| fetch_quote(sym) }
  print_quotes(quotes, alert_price)
  break if interval <= 0
  sleep interval
end

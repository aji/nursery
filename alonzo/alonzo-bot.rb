#!/usr/bin/env ruby

require 'socket'
require 'getoptlong'

def runex(ex)
  p = IO.popen('./alonzo', 'w+')
  p.write "#{ex}\n"
  out = p.gets
  out = "(no data)" if out == nil
  p.close
  out = "#{out[0..$limit]}..." if out.length > $limit
  return out
end

$host = 'irc.staticbox.net'
$port = 6667
$nick = 'alonzo'
$user = 'ac'
$gecos = 'lambda calculus bot'
$channels = ['#alonzo', '#programming']

$limit = 100

GetoptLong.new(
  [ '--help', GetoptLong::NO_ARGUMENT ],
  [ '--host', '-h', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--port', '-p', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--join', '-j', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--nick', '-n', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--limit', '-l', GetoptLong::REQUIRED_ARGUMENT ],
).each do |opt, arg|
  case opt
    when '--help'
      puts "usage: alonzo-bot.rb [OPTIONS]"
      puts "options:"
      puts "  --host HOST      connect to HOST"
      puts "  --port PORT      use PORT"
      puts "  --join CHANS     join channels (comma-separated)"
      puts "  --nick NICK      use nickname NICK"
      puts "  --limit N        limit output to N chars"
    when '--host'
      $host = arg
    when '--port'
      $port = arg.to_i
    when '--join'
      $channels = arg.split ','
    when '--nick'
      $nick = arg
    when '--limit'
      $limit = arg.to_i
  end
end

$sock = TCPSocket.new $host, $port

def outl(ln)
  puts "<- #{ln}"
  $sock.write "#{ln}\r\n"
end

outl "NICK #{$nick}"
outl "USER #{$user} 0 * :#{$gecos}"

while line = $sock.gets
  puts "-> #{line}"
  case line
    when /:[^ ]* 001/
      $channels.each { |x| outl "JOIN #{x}" }
    when /^:(.*)!.* PRIVMSG (.*) :>> (.*)/
      reply = ($2.start_with? "#") ? $2 : $1
      ex = $3
      outl "PRIVMSG #{reply} :   #{runex ex}"
    when /^PING (.*)/
      outl "PONG #{$1}"
  end
end

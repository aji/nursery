
--
-- aji can't live without partition
--

local smtbl = getmetatable("")["__index"]

-- works like python's partition
smtbl["partition"] = function (s, pat)
  local at = { s:find(pat) }
  if not at[1] then return s, "", "" end
  return s:sub(1, at[1]-1), s:sub(at[1], at[2]), s:sub(at[2]+1)
end

-- retrieves the substring at the position
smtbl["at"] = function (s, i)
  return s:sub(i,i)
end

-- determines if this string starts with a given string
smtbl["startswith"] = function (s, oth)
  return oth == s:sub(1, #oth)
end


--
-- socket support functions
--

local socket = require("socket")

-- opens a connection to port p on host h
function connect(h, p)
  print("Connecting to " .. h .. "/" .. tostring(p))
  local s = socket.tcp()
  local status, msg = s:connect(h, p)

  if not status then
    print("Connect failed: " .. msg)
    return nil
  end

  return s
end

-- splits an RFC 1459 message
function irc_split(ln)
  local src, srcextra, cmd
  local args = { }

  if ln:at(1) == ":" then
    src, _, ln = (ln:sub(2)):partition("%s+")
    src, _, srcextra = src:partition("!")
  end

  cmd, _, ln = ln:partition("%s+")
  cmd = cmd:upper()

  ln, sep, trailing = ln:partition("%s+:")
  if sep == "" then trailing = nil end

  while ln ~= "" do
    arg, _, ln = ln:partition("%s+")
    table.insert(args, arg)
  end

  if trailing then table.insert(args, trailing) end

  return src, srcextra, cmd, args
end


--
-- irc helpers
--

local my = { }

-- write a line to the socket
function out(ln)
  print("<- " .. ln)
  my["sock"]:send(ln .. "\r\n")
end

-- process a line
function process(ln)
  src, srcextra, cmd, args = irc_split(ln)
  if my["msgs"][cmd] then
    my["msgs"][cmd](my, ln, src, srcextra, cmd, args)
  end
end

-- reply from within a command handler
function reply(s)
  out(my["reply"]:format(s))
end


--
-- irc message handlers
--

my["msgs"] = {
  ["001"] = function(my, ln, src, ex, cmd, args)
    my["nick"] = args[1]
    print("Have nickname: " .. my["nick"])
    out("JOIN " .. my["chan"])
  end,

  ["PING"] = function(my, ln, src, ex, cmd, args)
    x, _, data = ln:partition("PING ")
    out("PONG " .. data)
  end,

  ["NICK"] = function(my, ln, src, ex, cmd, args)
    if src ~= my["nick"] then return end
    my["nick"] = args[1]
    print("New nickname: " .. my["nick"])
  end,

  ["PRIVMSG"] = function(my, ln, src, ex, cmd, args)
    local cname, cargs
    local text = args[2]

    if text:at(1) == "\1" then
      cname, _, cargs = text:sub(2,-2):partition("%s+")
      if my["ctcp"][cname] then
        my["reply"] = ("NOTICE %s :\1%s %%s\1"):format(src, cname)
        my["ctcp"][cname](cname, cargs)
      else
        print(("%s used unknown ctcp %s"):format(src, cname))
      end
      return
    end

    if args[1]:at(1) == '#' then
      if text:at(1) == my["cmdpfx"] then
        -- .command args...
        cname, _, cargs = text:sub(2):partition("%s+")
      elseif text:startswith(my["nick"]) then
        -- foobot: command args...
        x, _, cname = text:partition("%s+")
        cname, _, cargs = cname:partition("%s+")
      end

      if cname then
        my["reply"] = ("PRIVMSG %s :%s: %%s"):format(args[1], src)
      end

    else
      -- /msg foobot command args...
      cname, _, cargs = text:partition("%s+")
      my["reply"] = ("NOTICE %s :%%s"):format(src)
    end

    if cname then
      cname = cname:lower()
      if my["cmds"][cname] then
        my["cmds"][cname](cname, cargs)
      else
        print(("%s used unknown command %s"):format(src, cname))
      end
    end
  end,
}


--
-- user command handlers
--

function choice(t)
  return t[math.random(#t)]
end

my["cmds"] = {
  ["hi"] = function (n, args)
    reply(choice({
      "hello", "hi", "ohai", "hi", "lolsup", "c:", ":D",
      "herro", "hullo", "heyyyy", "hiiii", "lol", "ohithur"
    }))
  end,

  ["echo"] = function (n, args)
    reply(args)
  end,

  ["add"] = function (n, args)
    local sum = 0
    for w in args:gmatch("%d+") do
      sum = sum + tonumber(w)
    end
    reply("sum = "..sum)
  end
}

my["ctcp"] = {
  ["VERSION"] = function (n, args)
    reply("aji's lua foobot: https://github.com/aji/bot/tree/master/lua")
  end
}


--
-- "entry", basically
--

local args = { ... }

if #args < 2 then
  print("Usage: bot.lua SERVER CHANNEL [PORT]")
  print("CHANNEL has no #, e.g. my-chan to join #my-chan")
  return
end

my["nick"] = "foobot"
my["ident"] = "foo"
my["gecos"] = "lua5.1"
my["cmdpfx"]  = "."
my["reply"] = { }

my["host"] = args[1]
my["chan"] = "#" .. args[2]
my["port"] = tonumber(args[3] or "6667")

-- make connection
my["sock"] = connect(my["host"], my["port"])
if not my["sock"] then return end

print("Registering...")
out("NICK " .. my["nick"])
out("USER " .. my["ident"] .. " * 0 :" .. my["gecos"])

-- main loop
while true do
  local ln, msg = my["sock"]:receive("*l")

  if not ln then
    print("Receive failed: " .. msg)
    break
  end

  print("-> " .. ln)
  process(ln)
end

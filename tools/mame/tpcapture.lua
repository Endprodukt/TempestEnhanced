-- tpcapture.lua - record Tempest frames from MAME for tpcompare.
--
--   TPCAP_OUT=attract.tpcap mame tempest -autoboot_script tools/mame/tpcapture.lua
--
-- One FRAM record each time the main loop starts a frame (its store of 0
-- into the frame timer at C7AF): CPU registers, RAM, colour RAM, vector
-- RAM, math box, EAROM. Before each FRAM, what happened since the one
-- before: every read of an I/O address (READ) and every interrupt taken
-- (IRQS). After each FRAM, the vector generator's state (VGST). Format:
-- src/capture/capture.h.
--
-- Environment:
--   TPCAP_OUT     output file (default tpcapture.tpcap)
--   TPCAP_FRAMES  stop after this many frames (default: until MAME exits)
--   TPCAP_PLAY    a played script, see below
--   TPCAP_INFO    1: print the input fields and the devices' items

local FRTIMR   = 0x53
local MAIN_STA = 0xC7AF
local CLOCK    = 1512000.0

local out_path = os.getenv("TPCAP_OUT") or "tpcapture.tpcap"
local max_frames = tonumber(os.getenv("TPCAP_FRAMES") or "0") or 0
local info = os.getenv("TPCAP_INFO")

local machine = manager.machine
if machine.system.name ~= "tempest" then
    emu.print_error("tpcapture: this is " .. machine.system.name ..
        ", the addresses are for tempest (revision 3); recording anyway")
end

local cpu = machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local f = assert(io.open(out_path, "wb"))
f:write("TPCAP1\0\0")

-- Device state by save state items.
local function find_items(tag, wanted)
    local dev = machine.devices[tag]
    local got = {}
    if not dev then
        emu.print_error("tpcapture: no device " .. tag)
        return got
    end
    for name, id in pairs(dev.items) do
        if info then emu.print_info("tpcapture: " .. tag .. " item " .. name) end
        for key, pat in pairs(wanted) do
            if name:find(pat) then got[key] = emu.item(id) end
        end
    end
    for key, _ in pairs(wanted) do
        if not got[key] then emu.print_error("tpcapture: " .. tag .. " item " .. key .. " not found") end
    end
    return got
end
local mb = find_items(":mathbox", { reg = "m_reg", result = "m_result" })
local ea = find_items(":earom", { rom = "m_rom_data", address = "m_address", data = "/m_data$" })
-- the vector generator's state machine, for tplockstep's look at its timing
local vg = find_items(":avg", { pc = "/m_pc$", sp = "/m_sp$", latch = "m_state_latch", op = "/m_op$",
    halt = "/m_halt$", sync = "m_sync_halt", timer = "m_timer", dvy = "/m_dvy$", dvx = "/m_dvx$",
    bin = "m_bin_scale", scale = "/m_scale$", stack = "m_stack" })

local colorram = machine.memory.shares[":avg:colorram"]
if not colorram then emu.print_error("tpcapture: colour RAM share not found") end
if info then
    for tag, _ in pairs(machine.memory.shares) do emu.print_info("tpcapture: share " .. tag) end
    for tag, port in pairs(machine.ioport.ports) do
        for name, field in pairs(port.fields) do
            emu.print_info(string.format("tpcapture: port %s field '%s' mask %02X", tag, name, field.mask))
        end
    end
end

local quiet = false
local function peek(a)
    quiet = true
    local v = space:read_u8(a)
    quiet = false
    return v
end

local function now_cycle()
    return math.floor(machine.time:as_double() * CLOCK + 0.5)
end

local function state()
    quiet = true
    local d = space:read_range(0x0000, 0x07FF, 8)
    local c = {}
    for i = 0, 15 do c[#c + 1] = string.char(colorram and (colorram:read_u8(i) & 0xFF) or 0) end
    d = d .. table.concat(c) .. space:read_range(0x2000, 0x2FFF, 8)
    quiet = false
    local t = {}
    for i = 0, 15 do t[#t + 1] = string.pack("<I2", (mb.reg and mb.reg:read(i) or 0) & 0xFFFF) end
    t[#t + 1] = string.pack("<I2", (mb.result and mb.result:read(0) or 0) & 0xFFFF)
    for i = 0, 63 do t[#t + 1] = string.char(ea.rom and (ea.rom:read(i) & 0xFF) or 0) end
    t[#t + 1] = string.char(ea.address and (ea.address:read(0) & 0xFF) or 0)
    t[#t + 1] = string.char(ea.data and (ea.data:read(0) & 0xFF) or 0)
    return d .. table.concat(t)
end

-- A played script: TPCAP_PLAY is a list of "FRAME WHAT [ARGS]" separated
-- by ';', FRAME counted in game frames from power-on:
--   coin N | start N | start2 N | fire N | zap N   close the switch for N frames
--   spin D N        turn the spinner D steps per frame for N frames
--   auto N          from here on: turn about at random, fire every N frames,
--                   zap now and then (auto 0: stop)
--   test 1 | test 0 the self test switch on or off
--   poke ADDR VAL   write a byte (hex) into RAM
--   dip PORT VAL    set a bank of option switches (DSW1, DSW2) to VAL (hex)
local play = {}
for entry in (os.getenv("TPCAP_PLAY") or ""):gmatch("[^;]+") do
    local w = {}
    for word in entry:gmatch("%S+") do w[#w + 1] = word end
    if #w >= 2 then play[#play + 1] = { frame = tonumber(w[1]), what = w[2], a = w[3], b = w[4] } end
end
local ports = machine.ioport.ports
local function field(port, name)
    local p = ports[port]
    local fl = p and p.fields[name]
    if not fl then emu.print_error("tpcapture: no field '" .. name .. "' in " .. port) end
    return fl
end
local switches = {
    coin = field(":IN0", "Coin 1"),
    start = field(":IN2", "1 Player Start"), start2 = field(":IN2", "2 Players Start"),
    fire = field(":BUTTONSP1", "P1 Button 1"), zap = field(":BUTTONSP1", "P1 Button 2"),
}
local held = {}
local knob = 0              -- the spinner's position, as the script turns it
local spin_d, spin_left = 0, 0
local auto, auto_left = 0, 0
local seed = 12345
local frames = 0

local function rnd(n)   -- the script's own, so that a recording can be made again
    seed = (seed * 1103515245 + 12345) & 0x7fffffff
    return (seed >> 8) % n
end
local function set(fl, on) if fl then fl:set_value(on and 1 or 0) end end
local function press(name, n)
    set(switches[name], true)
    held[name] = n
end

local function play_frame()
    for name, left in pairs(held) do
        if left <= 0 then
            set(switches[name], false)
            held[name] = nil
        else
            held[name] = left - 1
        end
    end
    for _, e in ipairs(play) do
        if e.frame == frames then
            if switches[e.what] then
                press(e.what, tonumber(e.a or "3"))
            elseif e.what == "spin" then
                spin_d, spin_left = tonumber(e.a or "1"), tonumber(e.b or "10")
            elseif e.what == "test" then
                -- the self test switch: test 1 on, test 0 off
                set(field(":IN0", "Service Mode"), tonumber(e.a or "1") ~= 0)
            elseif e.what == "auto" then
                auto = tonumber(e.a or "6")
            elseif e.what == "dip" then
                local port = ports[":" .. e.a]
                local v = tonumber(e.b, 16)
                if port then
                    for _, fl in pairs(port.fields) do fl.user_value = v & fl.mask end
                end
            elseif e.what == "poke" then
                quiet = true
                space:write_u8(tonumber(e.a, 16), tonumber(e.b, 16))
                quiet = false
                f:write("POKE", string.pack("<I4I2B", 3, tonumber(e.a, 16), tonumber(e.b, 16)))
            end
        end
    end
    if auto > 0 then
        if auto_left <= 0 then
            local ds = { -3, -2, -1, 0, 1, 2, 3 }
            spin_d, spin_left = ds[rnd(7) + 1], 3 + rnd(20)
            auto_left = spin_left
        end
        auto_left = auto_left - 1
        if frames % auto == 0 and not held.fire then press("fire", 1) end
        if rnd(400) == 0 and not held.zap then press("zap", 2) end
    end
    if spin_left > 0 then
        knob = (knob + spin_d) & 0x0F
        spin_left = spin_left - 1
    end
end

taps = {}                -- keep the taps referenced: they are removed when collected
local frame_start = nil  -- cycle of the FRAM before
local reads, irqs = {}, {}

-- Every read of an I/O address, with the cycle. The spinner is the
-- script's: POKEY 1's ALLPOT shows its position inverted in D0-D3.
local function on_read(offset, data, mask)
    if quiet then return end
    local v = data & 0xFF
    if offset == 0x60C8 then v = (v & 0xF0) | ((~knob) & 0x0F) end
    if frame_start then
        reads[#reads + 1] = string.pack("<I4I2B", now_cycle() - frame_start, offset, v)
    end
    if offset == 0x60C8 then return (data & ~0xFF) | v end
end
taps.io1 = space:install_read_tap(0x0800, 0x1FFF, "tpcap_io1", on_read)
taps.io2 = space:install_read_tap(0x4000, 0x8FFF, "tpcap_io2", on_read)

-- An interrupt: the CPU fetches the vector after it pushed PC and P. (BRK
-- uses the same vector; the program only does that to restart.)
taps.vector = space:install_read_tap(0xFFFE, 0xFFFE, "tpcap_vector",
    function(offset, data, mask)
        if quiet or not frame_start then return end
        local sp = cpu.state["SP"].value & 0xFF
        local ret = peek(0x100 + ((sp + 2) & 0xFF)) | (peek(0x100 + ((sp + 3) & 0xFF)) << 8)
        irqs[#irqs + 1] = string.pack("<I4I2", now_cycle() - frame_start, ret)
    end)

local done = false
taps.frame = space:install_write_tap(FRTIMR, FRTIMR, "tpcap_frame",
    function(offset, data, mask)
        if quiet or done or (data & 0xFF) ~= 0 then return end
        local pc = cpu.state["PC"].value
        if pc < MAIN_STA or pc > MAIN_STA + 2 then return end
        local cyc = now_cycle()
        frames = frames + 1
        local r = table.concat(reads)
        f:write("READ", string.pack("<I4", #r), r)
        local q = table.concat(irqs)
        f:write("IRQS", string.pack("<I4", #q), q)
        reads, irqs = {}, {}
        local body = string.pack("<I4I4I4BBBBB", frames, cyc & 0xFFFFFFFF, cyc >> 32,
            cpu.state["A"].value & 0xFF, cpu.state["X"].value & 0xFF, cpu.state["Y"].value & 0xFF,
            cpu.state["SP"].value & 0xFF, cpu.state["P"].value & 0xFF) .. state()
        f:write("FRAM", string.pack("<I4", #body), body)
        frame_start = cyc
        play_frame()
        local function it(x, i) return x and (x:read(i or 0) & 0xFFFF) or 0 end
        local vgs = string.pack("<I2I2BBBBI2I2I2BBI2I2I2I2",
            it(vg.pc), it(vg.sp), it(vg.latch) & 0xFF, it(vg.op) & 0xFF, it(vg.halt) & 0xFF, it(vg.sync) & 0xFF,
            it(vg.timer), it(vg.dvy), it(vg.dvx), it(vg.bin) & 0xFF, it(vg.scale) & 0xFF,
            it(vg.stack, 0), it(vg.stack, 1), it(vg.stack, 2), it(vg.stack, 3))
        f:write("VGST", string.pack("<I4", #vgs), vgs)
        if max_frames > 0 and frames >= max_frames then
            done = true
            f:flush()
        end
    end)

stop_sub = emu.add_machine_stop_notifier(function()
    f:close()
    emu.print_info(string.format("tpcapture: %d frames written to %s", frames, out_path))
end)

#!/usr/bin/env lua
-- =============================================================================
-- debug_console.lua - On-target debug console for mmWave RF test firmware
--
-- Interactive debug console that communicates with the firmware via UART.
-- Provides command history, auto-completion, register peek/poke, and
-- scripted test sequences.
--
-- Usage:
--   lua debug_console.lua --port COM3
--   lua debug_console.lua --port /dev/ttyUSB0 --baud 921600
-- =============================================================================

local VERSION = "1.0.0"

-- Configuration
local config = {
    port = "COM3",
    baud = 921600,
    timeout_ms = 3000,
    prompt = "dbg> ",
    history_file = ".debug_history",
}

-- Parse command-line arguments
for i = 1, #arg do
    if arg[i] == "--port" and arg[i+1] then
        config.port = arg[i+1]
    elseif arg[i] == "--baud" and arg[i+1] then
        config.baud = tonumber(arg[i+1])
    end
end

-- Command history
local history = {}
local history_idx = 0

-- Register map for quick access
local register_map = {
    synth_ctrl    = 0x02001000,
    synth_freq_lo = 0x02001004,
    synth_status  = 0x02001024,
    tx_ctrl       = 0x02002000,
    tx_ch0_power  = 0x02002004,
    rx_ctrl       = 0x02003000,
    pga_ctrl      = 0x02004000,
    adc_ctrl      = 0x02006000,
    adc_status    = 0x02006014,
    gpio_out      = 0x02120000,
}

-- Utility functions
local function printf(fmt, ...)
    io.write(string.format(fmt, ...))
    io.flush()
end

local function send_command(cmd)
    -- In a real implementation, this writes to the serial port
    -- For now, we simulate the interaction
    printf("[TX] %s\n", cmd)
    return "OK"
end

local function read_response()
    -- Simulate reading from serial port
    return "> "
end

-- Built-in debug commands
local commands = {}

commands.help = function()
    printf("Debug Console v%s\n", VERSION)
    printf("Commands:\n")
    printf("  help                  - Show this help\n")
    printf("  peek <addr|name>      - Read 32-bit register\n")
    printf("  poke <addr|name> <val>- Write 32-bit register\n")
    printf("  regs                  - Dump key registers\n")
    printf("  test <name> [args]    - Run firmware test command\n")
    printf("  cal <name> [args]     - Run calibration command\n")
    printf("  status                - Query firmware status\n")
    printf("  freq <ghz>            - Set synthesizer frequency\n")
    printf("  tx <on|off> [ch]      - Enable/disable TX channel\n")
    printf("  rx <on|off> [ch]      - Enable/disable RX channel\n")
    printf("  capture <samples>     - Trigger ADC capture\n")
    printf("  reset                 - Reset device\n")
    printf("  quit                  - Exit console\n")
end

commands.peek = function(addr_str)
    if not addr_str then
        printf("Usage: peek <address|register_name>\n")
        return
    end

    local addr = register_map[addr_str]
    if not addr then
        addr = tonumber(addr_str, 16) or tonumber(addr_str)
    end

    if not addr then
        printf("Error: Invalid address '%s'\n", addr_str)
        return
    end

    local resp = send_command(string.format("peek 0x%08X", addr))
    printf("  [0x%08X] = %s\n", addr, resp)
end

commands.poke = function(addr_str, val_str)
    if not addr_str or not val_str then
        printf("Usage: poke <address|name> <value>\n")
        return
    end

    local addr = register_map[addr_str] or tonumber(addr_str, 16) or tonumber(addr_str)
    local val = tonumber(val_str, 16) or tonumber(val_str)

    if not addr or not val then
        printf("Error: Invalid address or value\n")
        return
    end

    send_command(string.format("poke 0x%08X 0x%08X", addr, val))
    printf("  [0x%08X] <- 0x%08X\n", addr, val)
end

commands.regs = function()
    printf("Key Register Dump:\n")
    printf("%-20s %-12s %s\n", "Name", "Address", "Value")
    printf(string.rep("-", 50) .. "\n")
    for name, addr in pairs(register_map) do
        local resp = send_command(string.format("peek 0x%08X", addr))
        printf("%-20s 0x%08X   %s\n", name, addr, resp)
    end
end

commands.freq = function(ghz_str)
    if not ghz_str then
        printf("Usage: freq <frequency_ghz>\n")
        return
    end
    send_command(string.format("test freq_set --freq %s", ghz_str))
end

commands.tx = function(state, channel)
    channel = channel or "0"
    if state == "on" then
        send_command(string.format("tx_enable --channel %s", channel))
    elseif state == "off" then
        send_command(string.format("tx_disable --channel %s", channel))
    else
        printf("Usage: tx <on|off> [channel]\n")
    end
end

commands.quit = function()
    printf("Goodbye.\n")
    os.exit(0)
end

-- Main REPL loop
local function main()
    printf("\nmmWave RF Debug Console v%s\n", VERSION)
    printf("Port: %s @ %d baud\n", config.port, config.baud)
    printf("Type 'help' for available commands.\n\n")

    while true do
        io.write(config.prompt)
        io.flush()
        local line = io.read("*l")

        if not line then
            break
        end

        -- Parse command and arguments
        local parts = {}
        for token in line:gmatch("%S+") do
            table.insert(parts, token)
        end

        if #parts > 0 then
            local cmd_name = parts[1]
            table.insert(history, line)

            if commands[cmd_name] then
                commands[cmd_name](parts[2], parts[3], parts[4])
            else
                -- Forward unknown commands directly to firmware
                local resp = send_command(line)
                printf("%s\n", resp)
            end
        end
    end
end

main()

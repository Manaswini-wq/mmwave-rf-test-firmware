#!/usr/bin/env lua
-- =============================================================================
-- quick_test.lua - Rapid RF verification script
--
-- Runs a quick subset of RF tests for rapid pass/fail determination
-- during development or incoming inspection.  Communicates with firmware
-- via UART and reports results in a compact format.
--
-- Usage:
--   lua quick_test.lua --port COM3
--   lua quick_test.lua --port COM3 --freq 77.0 --channels 0,1,2
-- =============================================================================

local VERSION = "1.0.0"

-- Configuration
local config = {
    port = "COM3",
    baud = 921600,
    center_freq_ghz = 62.0,
    channels = {0, 1, 2},
    timeout_ms = 5000,
}

-- Parse arguments
for i = 1, #arg do
    if arg[i] == "--port" and arg[i+1] then
        config.port = arg[i+1]
    elseif arg[i] == "--freq" and arg[i+1] then
        config.center_freq_ghz = tonumber(arg[i+1])
    end
end

-- Test limits
local limits = {
    tx_power    = { lower = 10.0, upper = 13.0, unit = "dBm" },
    rx_noise    = { lower = -120.0, upper = -100.0, unit = "dBm" },
    phase_noise = { lower = -120.0, upper = -85.0, unit = "dBc/Hz" },
    freq_acc    = { lower = -50.0, upper = 50.0, unit = "kHz" },
}

-- Serial communication (simplified)
local function send_cmd(cmd)
    -- In production, this sends via serial port
    io.write(string.format("[CMD] %s\n", cmd))
    return "11.5"  -- Simulated response
end

local function parse_value(response)
    local val = response:match("([-]?%d+%.?%d*)")
    return tonumber(val) or 0
end

-- Test execution
local results = {}
local total_pass = 0
local total_fail = 0

local function run_test(test_name, channel, freq_ghz)
    local cmd = string.format("test %s --channel %d --freq %.1f",
                               test_name, channel, freq_ghz)
    local response = send_cmd(cmd)
    local measured = parse_value(response)

    local lim = limits[test_name]
    local pass = lim and measured >= lim.lower and measured <= lim.upper

    local result = {
        test = test_name,
        channel = channel,
        freq = freq_ghz,
        measured = measured,
        pass = pass,
        unit = lim and lim.unit or "?",
    }
    table.insert(results, result)

    if pass then
        total_pass = total_pass + 1
    else
        total_fail = total_fail + 1
    end

    local status = pass and "PASS" or "FAIL"
    io.write(string.format("  %-15s CH%d @ %.1f GHz: %7.2f %-8s [%s]\n",
        test_name, channel, freq_ghz, measured, result.unit, status))
end

-- Main
local function main()
    io.write(string.format("\n=== Quick RF Test v%s ===\n", VERSION))
    io.write(string.format("Port: %s | Freq: %.1f GHz | Channels: %s\n\n",
        config.port, config.center_freq_ghz,
        table.concat(config.channels, ",")))

    -- TX power on all channels
    io.write("TX Power Tests:\n")
    for _, ch in ipairs(config.channels) do
        run_test("tx_power", ch, config.center_freq_ghz)
    end

    -- RX noise floor on CH0
    io.write("\nRX Tests:\n")
    run_test("rx_noise", 0, config.center_freq_ghz)

    -- Phase noise
    io.write("\nPhase Noise:\n")
    run_test("phase_noise", 0, config.center_freq_ghz)

    -- Frequency accuracy
    io.write("\nFrequency Accuracy:\n")
    run_test("freq_acc", 0, config.center_freq_ghz)

    -- Summary
    local total = total_pass + total_fail
    io.write(string.format("\n=== Results: %d/%d PASS", total_pass, total))
    if total_fail == 0 then
        io.write(" - OVERALL PASS ===\n")
    else
        io.write(string.format(" (%d FAIL) - OVERALL FAIL ===\n", total_fail))
    end

    return total_fail == 0 and 0 or 1
end

os.exit(main())

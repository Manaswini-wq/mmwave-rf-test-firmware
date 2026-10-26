# Firmware-Hardware Interface Specification

## 1. Overview

This document describes the firmware interface to the IWR6843 mixed-signal analog and digital SoC hardware. It covers register-level access conventions, driver architecture, interrupt handling, and DMA configuration for the RF subsystem.

## 2. Memory Map

### 2.1 RF Subsystem Base Addresses
| Block | Base Address | Size | Description |
|-------|-------------|------|-------------|
| Synthesizer | 0x0200_1000 | 4 KB | PLL, VCO, LO distribution |
| TX Frontend | 0x0200_2000 | 4 KB | PA, phase shifters, loopback |
| RX Frontend | 0x0200_3000 | 4 KB | LNA, mixer, DC offsets |
| PGA | 0x0200_4000 | 4 KB | Programmable gain amplifier |
| IF Filter | 0x0200_5000 | 4 KB | HPF/LPF bandwidth control |
| ADC | 0x0200_6000 | 4 KB | ADC control and status |
| DAC | 0x0200_7000 | 4 KB | DAC output and calibration |
| DMA | 0x0210_0000 | 64 KB | DMA controller |
| UART | 0x0211_0000 | 4 KB | UART0 for host communication |
| OTP | 0x0300_0000 | 4 KB | One-time programmable memory |

### 2.2 Memory Regions
| Region | Address Range | Size | Usage |
|--------|-------------|------|-------|
| TCMA (ATCM) | 0x0000_0000 | 128 KB | Code + read-only data |
| TCMB (BTCM) | 0x0008_0000 | 128 KB | Variables, stack, heap |
| L3 SRAM | 0x5100_0000 | 512 KB | ADC buffers, FFT output |
| ADC Buffer | 0x5200_0000 | 32 KB | Raw ADC capture memory |
| HWA RAM | 0x5203_0000 | 64 KB | Hardware accelerator |

## 3. Register Access Conventions

### 3.1 Read/Write Macros
```c
#define REG_WR32(addr, val)  (*(volatile uint32_t *)(addr) = (uint32_t)(val))
#define REG_RD32(addr)       (*(volatile uint32_t *)(addr))
```

### 3.2 Bit Manipulation
```c
#define BIT_SET(reg, mask)   ((reg) |= (mask))
#define BIT_CLR(reg, mask)   ((reg) &= ~(mask))
#define BIT_TST(reg, mask)   (((reg) & (mask)) != 0U)
```

### 3.3 Access Rules
- All register accesses are 32-bit aligned
- Read-modify-write sequences must be atomic (disable interrupts if needed)
- Status bits are cleared by writing 1 to the corresponding bit position
- Reserved bits must be written as 0

## 4. Driver Architecture

### 4.1 Driver Layers
```
Application (RF Tests, Calibration)
         |
    Driver API (synth_driver, pga_driver, etc.)
         |
    Register Access (REG_WR32, REG_RD32)
         |
    Hardware (MMIO registers)
```

### 4.2 Driver API Conventions
- `xxx_init()` - Initialize the hardware block
- `xxx_enable()` / `xxx_disable()` - Power on/off
- `xxx_set_xxx()` - Configure parameters
- `xxx_get_xxx()` - Read back state
- All functions return `Status_t` (0 = success, negative = error)

## 5. DMA Data Path

### 5.1 ADC Capture Flow
```
ADC -> ADC Buffer (0x5200_0000) -> DMA -> L3 SRAM Ping/Pong Buffers
```

### 5.2 DMA Configuration
- Source: ADC buffer memory (fixed address)
- Destination: L3 SRAM (ping or pong buffer)
- Transfer size: num_samples * 4 bytes (I16 + Q16 per sample)
- Trigger: ADC capture complete interrupt
- Mode: Single-shot or circular (for continuous capture)

### 5.3 Ping-Pong Buffering
- While DMA fills ping buffer, CPU processes pong buffer
- DMA completion interrupt toggles active buffer
- Zero-copy operation for maximum throughput

## 6. Interrupt Handling

| IRQ | Source | Handler |
|-----|--------|---------|
| 0 | ADC capture done | Start DMA transfer |
| 1 | DMA transfer done | Signal capture engine |
| 2 | UART RX | Buffer received byte |
| 3 | Timer | System tick counter |

## 7. Clock and Power

### 7.1 Clock Tree
- System clock: 200 MHz (from ROM PLL init)
- ADC clock: Derived from system clock via divider
- UART clock: 48 MHz (fixed)

### 7.2 Power Sequencing
1. ROM initializes core clocks
2. Firmware enables RF subsystem power domains
3. Individual blocks enabled on demand (synth, TX, RX)
4. Idle blocks disabled to reduce power consumption

/* ==========================================================================
 * IWR6843 R5F Linker Script
 * Target: ARM Cortex-R5F core on TI IWR6843 mmWave Radar SoC
 *
 * Memory Map:
 *   TCMA (ATCM)  : 0x00000000 - 0x0001FFFF  (128 KB) - Tightly Coupled Memory A
 *   TCMB (BTCM)  : 0x00080000 - 0x0009FFFF  (128 KB) - Tightly Coupled Memory B
 *   L3 SRAM      : 0x51000000 - 0x5107FFFF  (512 KB) - Shared L3 RAM
 *   HWA RAM      : 0x52030000 - 0x5203FFFF  (64 KB)  - Hardware Accelerator RAM
 *   ADC BUF      : 0x52000000 - 0x52007FFF  (32 KB)  - ADC buffer memory
 *   QSPI Flash   : 0xC0000000 - 0xC01FFFFF  (2 MB)   - External flash
 *   ROM           : 0xFFFF0000 - 0xFFFFFFFF  (64 KB)  - Boot ROM
 * ========================================================================== */

MEMORY
{
    VECTORS  (RX)  : ORIGIN = 0x00000000, LENGTH = 0x00000100
    TCMA     (RWX) : ORIGIN = 0x00000100, LENGTH = 0x0001FF00
    TCMB     (RW)  : ORIGIN = 0x00080000, LENGTH = 0x00020000
    L3_SRAM  (RWX) : ORIGIN = 0x51000000, LENGTH = 0x00080000
    HWA_RAM  (RW)  : ORIGIN = 0x52030000, LENGTH = 0x00010000
    ADC_BUF  (RW)  : ORIGIN = 0x52000000, LENGTH = 0x00008000
    QSPI     (RX)  : ORIGIN = 0xC0000000, LENGTH = 0x00200000
}

/* Entry point */
ENTRY(_reset_handler)

/* Stack and heap sizes */
_stack_size = 0x2000;   /* 8 KB main stack */
_irq_stack_size = 0x400; /* 1 KB IRQ stack */
_heap_size  = 0x4000;   /* 16 KB heap */

SECTIONS
{
    /* ---- Interrupt Vector Table (TCMA base) ---- */
    .vectors : ALIGN(256)
    {
        _vectors_start = .;
        KEEP(*(.vectors))
        KEEP(*(.vectors.*))
        _vectors_end = .;
    } > VECTORS

    /* ---- Code sections in TCMA for fast execution ---- */
    .text : ALIGN(4)
    {
        _text_start = .;
        *(.text.reset_handler)
        *(.text.startup)
        *(.text.isr*)

        /* Critical path code in TCM */
        *(.text.rf_test*)
        *(.text.calibration*)
        *(.text.capture*)

        /* Remaining code */
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)

        /* ARM exception handling */
        *(.ARM.extab*)
        *(.gnu.linkonce.armextab.*)

        KEEP(*(.init))
        KEEP(*(.fini))

        _text_end = .;
    } > TCMA

    /* ---- Read-only data ---- */
    .rodata : ALIGN(4)
    {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)

        /* Calibration lookup tables */
        *(.rodata.cal_tables)
        *(.rodata.test_limits)

        /* ROM function pointer table */
        KEEP(*(.rodata.rom_func_table))

        _rodata_end = .;
    } > TCMA

    /* ---- ARM unwind tables ---- */
    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        *(.gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > TCMA

    /* ---- Initialized data (load from TCMA, run from TCMB) ---- */
    .data : ALIGN(4)
    {
        _data_start = .;
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)

        /* Driver state variables */
        *(.data.driver_state)

        /* Test result storage */
        *(.data.test_results)

        _data_end = .;
    } > TCMB AT > TCMA

    _data_load_start = LOADADDR(.data);

    /* ---- Uninitialized data (BSS) ---- */
    .bss (NOLOAD) : ALIGN(4)
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)

        /* Large buffers */
        *(.bss.adc_buffer)
        *(.bss.fft_buffer)
        *(.bss.dma_descriptors)

        _bss_end = .;
    } > TCMB

    /* ---- Heap ---- */
    .heap (NOLOAD) : ALIGN(8)
    {
        _heap_start = .;
        . += _heap_size;
        _heap_end = .;
    } > TCMB

    /* ---- Stack ---- */
    .stack (NOLOAD) : ALIGN(8)
    {
        _stack_bottom = .;
        . += _stack_size;
        _stack_top = .;
    } > TCMB

    /* ---- IRQ Stack ---- */
    .irq_stack (NOLOAD) : ALIGN(8)
    {
        _irq_stack_bottom = .;
        . += _irq_stack_size;
        _irq_stack_top = .;
    } > TCMB

    /* ---- L3 SRAM sections for large data buffers ---- */
    .l3_data (NOLOAD) : ALIGN(4)
    {
        _l3_data_start = .;

        /* ADC capture buffers (ping-pong) */
        *(.l3.adc_capture_ping)
        *(.l3.adc_capture_pong)

        /* FFT output buffers */
        *(.l3.fft_output)

        /* Calibration coefficient storage */
        *(.l3.cal_coefficients)

        /* Test result log buffer */
        *(.l3.test_log)

        _l3_data_end = .;
    } > L3_SRAM

    /* ---- HWA parameter RAM ---- */
    .hwa_params (NOLOAD) : ALIGN(4)
    {
        _hwa_params_start = .;
        *(.hwa.param_set)
        _hwa_params_end = .;
    } > HWA_RAM

    /* ---- ADC buffer memory ---- */
    .adc_buf (NOLOAD) : ALIGN(4)
    {
        _adc_buf_start = .;
        *(.adc_buffer)
        _adc_buf_end = .;
    } > ADC_BUF

    /* ---- Factory calibration data in QSPI ---- */
    .factory_cal : ALIGN(4)
    {
        _factory_cal_start = .;
        KEEP(*(.factory_cal))
        KEEP(*(.factory_cal.*))
        _factory_cal_end = .;
    } > QSPI

    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
    }
}

/* Provide symbols for startup code */
PROVIDE(_stack_pointer = _stack_top);
PROVIDE(_irq_stack_pointer = _irq_stack_top);
PROVIDE(_data_size = _data_end - _data_start);
PROVIDE(_bss_size = _bss_end - _bss_start);

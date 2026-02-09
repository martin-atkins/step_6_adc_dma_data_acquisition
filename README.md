# step_6_adc_dma_data_acquisition
Building from step 5, adding ADC + DMA (Analog-to-Digital Converter Direct Memory Access) on top of what we've built; exercises a different muscle than UART while reusing our scheduler, console, and discipline.

ADC DMA is a powerful technique in microcontrollers that lets the ADC automatically transfer its converted analog data directly into memory (RAM) without constantly involving the CPU, freeing the processor to handle other tasks while efficiently collecting continuous, high-speed sensor data. It's crucial for performance in applications needing fast, uninterrupted data streams, like monitoring multiple sensors. 

### Using these versions:
* `STM32CubeIDE` : `1.18.1`
* `STM32CubeMX` : `6.14.1`

### Why ADC + DMA next?
**We already have:**
* A system tick
* A cooperative scheduler
* DMA RX experience
* A console for observability

**ADC + DMA adds:**
* Continuous data acquisition
* Producer/consumer
* Buffer management under load
* Time-based reasoning (sampling rate vs processing rate)

### Goal
* **Sample an analog signal continuously using ADC + DMA, and expose it via the console**

## Proposed Feature Set (incremental)
**Stage 1 — ADC continuous sampling (DMA, circular)**
* ADC1
* Single channel (e.g. PA0 / A0)
* Continuous conversion
* DMA circular buffer
* Fixed sampling rate (e.g. 1 kHz via timer trigger)

**Stage 2 — Data processing task**

Add a scheduled task:
```c
void task_adc(void);
```

Responsibilities:
* Track DMA write position
* Consume new samples
* Update statistics:
  * min
  * max
  * average
  * last sample

No printing here

**Stage 3 — Console commands**

Add commands:

```
adc start
adc stop
adc latest
adc avg
adc temp
```

Example output:

```
> adc stop
adc stopped
ok
> adc start
adc started
ok
> adc latest
ADC latest=942  [759 mV]
ok
> adc avg
ADC avg=927  [747 mV]
ok
> adc temp
ADC=898  Rntc=2809 ohm  Temp=19.60 C
ok
```

Now the console becomes a debug instrument

### How this fits our existing architecture

Existing       | Reused for ADC
---------------|----------------------------
DMA RX logic   | DMA ADC buffer tracking
Scheduler      | ADC processing task
Console        | Configuration & observability
State machines | ADC start/stop control

## Step 1 — CubeMX / .ioc changes (do this first)
**ADC1**
* Resolution: 12-bit
* Scan Conv Mode: Disabled
* Continuous Conversion Mode: Enabled
* Discontinuous: Disabled
* External Trigger: Software start
* Data Alignment: Right
* DMA Continuous Requests: Enabled
* Overrun: Data preserved

**ADC Channel**
* Channel: our existing analog pin
* Sampling time: 84 cycles (or longer)

**DMA (linked to ADC1)**
* Direction: Peripheral → Memory
* Mode: Circular
* Data width: Half-word (16-bit)
* Memory increment: Enabled
* Priority: Low (fine)

**NVIC**
* ADC global interrupt: disabled (not needed)
* DMA interrupt: optional (we won’t use callbacks yet)

## Step 2 — Create a small ADC “service” layer

`adc_app.h`
``` c
#pragma once
#include <stdint.h>

void     adc_app_init(void);
uint16_t adc_get_avg(void);
```
`adc_app.c`
``` c
#include "adc_app.h"
#include "adc.h"

#define ADC_BUF_LEN 64

static uint16_t adc_buf[ADC_BUF_LEN];

void adc_app_init(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, ADC_BUF_LEN);
}

uint16_t adc_get_avg(void)
{
    uint32_t sum = 0;

    for (int i = 0; i < ADC_BUF_LEN; i++)
        sum += adc_buf[i];

    return (uint16_t)(sum / ADC_BUF_LEN);
}
```

* DMA runs forever
* No blocking
* No polling
* CPU only reads memory

## Step 3 — Initialize it once

In `main.c`, add:

**Includes**

``` c
#include "adc_app.h"
```

In `main()` after peripheral init...

``` c
adc_app_init();
```

That’s it. ADC is now live forever.

## Step 4 — Update our console command
`cmd_adc()`

``` c
static void cmd_adc(int argc, char **argv)
{
    uint16_t raw = adc_get_avg();

    uint32_t mv = (raw * 3300UL) / 4095UL;

    console_printf(
        "ADC raw=%u  [%lu mV]\r\n",
        raw,
        mv
    );

    console_prompt();
}
```

* No floats
* No HAL calls
* No timing dependencies

This is deliberate

## Step 5 — What I should now see

At idle:

```
> adc
ADC raw=930  [750 mV]
```

Touching sensor:

```
ADC raw=880  [710 mV]
ADC raw=840  [675 mV]
```

Smooth, stable, low noise.

## Step 6 — Why this architecture matters

We now have:

* Non-blocking ADC
* DMA circular acquisition
* Task-safe access
* Console observability

# Add a live ADC stream command
Continuous ADC acquisition using DMA, with zero blocking, controlled from our existing console.

We will end up with:
```
adc start     -> start ADC + DMA
adc stop      -> stop ADC + DMA
adc latest    -> print most recent sample
adc avg       -> print averaged value
```

ADC runs continuously in the background.
Console only *observes* the data

## Step 1 — Decide the ADC data model

### Design choice (good for learning)

* DMA writes into a small circular buffer
* We compute:
*   latest sample
*   running average

No interrupts needed for ADC.
No polling loops.
DMA does the work.

## Step 2 — Create `adc_app.h`
`Core/Inc/adc_app.h`

``` c
#pragma once

#include <stdint.h>

void     adc_app_init(void);
void     adc_app_start(void);
void     adc_app_stop(void);

uint16_t adc_app_latest(void);
uint16_t adc_app_average(void);
```

This keeps ADC logic out of `main.c` — exactly like our console module.

## Step 3 — Create `adc_app.c`

`Core/Src/adc_app.c`

``` c
#include "adc_app.h"
#include "adc.h"

#define ADC_DMA_BUF_LEN 32

static uint16_t adc_dma_buf[ADC_DMA_BUF_LEN];
static uint8_t  adc_running = 0;

void adc_app_init(void)
{
    // Nothing required yet
}

void adc_app_start(void)
{
    if (adc_running)
        return;

    HAL_ADC_Start_DMA(
        &hadc1,
        (uint32_t *)adc_dma_buf,
        ADC_DMA_BUF_LEN
    );

    adc_running = 1;
}

void adc_app_stop(void)
{
    if (!adc_running)
        return;

    HAL_ADC_Stop_DMA(&hadc1);
    adc_running = 0;
}

uint16_t adc_app_latest(void)
{
    if (!adc_running)
        return 0;

    // DMA is circular; most recent sample is "last written"
    uint32_t idx =
        ADC_DMA_BUF_LEN - __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);

    if (idx >= ADC_DMA_BUF_LEN)
        idx = 0;

    return adc_dma_buf[idx];
}

uint16_t adc_app_average(void)
{
    if (!adc_running)
        return 0;

    uint32_t sum = 0;

    for (int i = 0; i < ADC_DMA_BUF_LEN; i++)
        sum += adc_dma_buf[i];

    return (uint16_t)(sum / ADC_DMA_BUF_LEN);
}
```

**Why this is solid**

* No blocking
* No ADC interrupts
* DMA ownership is clear
* Console never touches HAL directly

## Step 4 — Add console commands

In our console command table:

``` c
static void cmd_adc(int argc, char **argv)
{
	if (argc < 2) {
        console_write("usage: adc start|stop|volts|latest|avg|temp\r\n");
        console_prompt();
		return;
	}

    if (!strcmp(argv[1], "start"))
    {
        adc_app_start();
        console_write("adc started\r\n");
    }
    else if (!strcmp(argv[1], "stop"))
    {
        adc_app_stop();
        console_write("adc stopped\r\n");
    }
    else if (!strcmp(argv[1], "volts"))
    {
        uint16_t raw = adc_app_average();
        uint32_t mv = adc_to_voltage(raw);
      		console_printf("ADC volts=%lu mV\r\n", mv);
    }
    else if (!strcmp(argv[1], "latest"))
    {
      		uint16_t raw = adc_read_avg(16);
		    		uint32_t mv = (raw * 3300UL) / 4095;
		    		console_printf("ADC latest=%u (raw) [%lu mV]\r\n", raw, mv);
    }
    else if (!strcmp(argv[1], "avg"))
    {
        uint16_t raw = adc_app_average();
        uint32_t mv = adc_to_voltage(raw);
    				console_printf("ADC avg=%u  [%lu mV]\r\n", raw, mv);
    }
    else
    {
        console_write("unknown adc command\r\n");
    }

	console_write("ok\r\n");
    console_prompt();
}
```

## Step 5 — Initialize ADC app

In main.c:
``` c
#include "adc_app.h"
```

After peripheral init:
``` c
adc_app_init();
```

Do **not** auto-start it — make it console-controlled

## We now have:

* A non-blocking data acquisition pipeline
* DMA feeding memory continuously
* A console that observes, not controls hardware
* A structure that scales to:
  * filters
  * logging
  * thresholds
  * multi-channel ADC

# Temperature conversion

## Step 1 — Decide conversion method

Our sensor is a 2-wire analog sensor between GND and 3.3 V

## Step 2 — Add conversion function

In `adc_app.h`, add:

``` c
float thermistor_beta_to_celsius(uint16_t raw);
```

In `adc_app.c`, implement:

``` c
float thermistor_beta_to_celsius(uint16_t raw)
{
    if (raw <= 0 || raw >= 4095)
        return -273.15f;

    const float VREF = 3.3f;
    const float R_PULLUP = 10000.0f;
    const float R0 = 2200.0f;      // 2.2k @ 25C
    const float BETA = 3950.0f;
    const float T0 = 298.15f;      // 25C in Kelvin

    float v = (raw / 4095.0f) * VREF;
    float r_ntc = R_PULLUP * v / (VREF - v);

    float temp_k =
        1.0f / ( (1.0f / T0) + (1.0f / BETA) * logf(r_ntc / R0) );

    float retval = temp_k - 273.15f;
    return retval;
}
```

## Step 3 — Add console commands

Extend `cmd_adc()`:

``` c
    else if (!strcmp(argv[1], "temp"))
    {
    				uint16_t raw = adc_read_avg(16);
        float temp_c = thermistor_beta_to_celsius(raw);
        if (!isfinite(temp_c)) {
            console_printf("TEMP ERROR (NaN/Inf)\r\n");
            return;
        }

        float r_ntc = adc_to_ntc_resistance(raw, 10000.0f);
        console_printf("ADC=%u  Rntc=%.0f ohm  Temp=%.2f C\r\n",
                       raw, r_ntc, temp_c);
    }
```

Example console session:

```
> adc start
adc started
ok
> adc temp
ADC=949  Rntc=3017 ohm  Temp=18.06 C
ok
```

**Use averaging for stability**

Use the DMA buffer average instead of latest sample. This smooths out small hand/thermal noise.

## Step 5 — Test & calibrate

1. Power sensor correctly (3.3 V and GND).
2. Measure actual room temp vs reading.
3. Adjust offset/scale if our sensor differs
4. Other: adjust formula

# I have the following wiring:

```
3.3 V ---[R]---+--- PA1 (ADC)
               |
              Sensor
               |
              GND
```

## Using an analog temperature sensor for data acquisition

A temperature sensor forces us to think about **units, scaling, noise, and sanity checks**, which is exactly what ADC work is really about. It naturally introduces:
* oversampling vs noise
* moving averages
* min/max tracking
* rate-of-change detection
* calibration offsets




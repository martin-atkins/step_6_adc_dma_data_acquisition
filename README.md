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
* Producer/consumer thinking
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

## STM32CubeMX (.ioc) changes (high-level)

1. Enable **ADC1**
2. Enable **DMA for ADC**
3. Set:
   * Scan mode: disabled
   * Continuous: disabled (we’ll use timer)
   * External trigger: TIMx TRGO
4. Enable **TIM trigger output**

### Timer-triggered ADC matters!

Without a timer:
* Sampling jitter
* CPU-dependent timing
* Non-repeatable behaviour

With a timer:
* Fixed sampling interval
* Hardware-driven
* Testable
* Scales to real systems

## Using an analog temperature sensor for data acquisition

A temperature sensor forces us to think about **units, scaling, noise, and sanity checks**, which is exactly what ADC work is really about. It naturally introduces:
* oversampling vs noise
* moving averages
* min/max tracking
* rate-of-change detection
* calibration offsets

I'm gonig to be using an NTC thermistor

## Proposed build plan (with temperature sensor)

### Stage 1 — Raw acquisition

* ADC + DMA circular buffer
* Sample at 10–50 Hz (temperature is slow)
* Capture raw ADC counts only

Console:

```
adc raw
```

Example:

```
last=2049 min=2041 max=2056
```

### Stage 2 — Voltage conversion

Add:

``` c
uint32_t adc_to_voltage(uint16_t raw)
```

Console:

```
adc volts
```

Output:

```
> adc volts
ADC volts=756 mV
ok
```

### Stage 3 — Temperature conversion

Console:

```
adc temp
```

Output:

```
> adc temp
ADC=941  Rntc=2984 ohm  Temp=18.30 C
ok
```

### Stage 4 — Status & health

Add:

```
adc status
```

Includes:

* running / stopped
* sample rate
* buffer overruns (should stay zero)
* sensor sanity range

* 


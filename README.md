# step_6_adc_dma_data_acquisition
Building from step 5, adding ADC + DMA on top of what we've built; exercises a different muscle than UART while reusing our scheduler, console, and discipline

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

**This is exactly the path used in:**
* sensor systems
* motor control
* medical instrumentation
* data loggers

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
adc status
adc start
adc stop
adc rate
```

Example output:

```
adc: running
rate: 1000 Hz
last: 2048
min: 2031
max: 2072
avg: 2049
```

Now the console becomes a debug instrument

### How this fits your existing architecture

Existing       | Reused for ADC
---------------|----------------------------
DMA RX logic   | DMA ADC buffer tracking
Scheduler      | ADC processing task
Console        | Configuration & observability
State machines | ADC run/stop control

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


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



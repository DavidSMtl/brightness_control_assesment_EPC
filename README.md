# Brightness Control Assessment (Zephyr RTOS)

### Subject: 

Embedded Platforms and Communication for IoT, MIoT (UPM)

### Authors: 
- David Suárez
- David Márquez

### Summary

This repository consists on the implementation of an IoT system designed to monitor light levels and control an RGB LED using **Zephyr Real-Time Operating System (RTOS)**. The board system used is the **STM32WL55JC** microcontroller (Nucleo-WL55JC board).

## Project task

Since the function of the application is to control an RGB LED mode of operation based on light, we use a Light Dependent Resistor sensor (LDR) to obtain light data. Furthermore, the user can interact with Button 2 (B2) integrated in the board to select a mode of operation.

### Key Features
* A **State Machine (FSM)** thread dedicated to switch two separate operating modes (Normal and blue).
* **Button Handling**: Distinguish between a short press (< 1s) and a long press (>= 1s) via timers.
* **Periodic Sensor Sampling**: One thread dedicated to read LDR sensor data via ADC pin every 2 seconds.
* **Event-Driven Architecture**: Registrated information turns into an `event_t` datatype to be managed in a message queue. This way, Zephyr can manage information without conflicts.
* **Low-Power Optimizations**: Thread suspension and hardware interrupts enabling automatic CPU sleep (). 


##  Hardware Setup

### Common Anode RGB LED
The RGB LED uses a **common anode** configuration connected to 3.3V. Thus, the control logic is **inverted** (Activates at low level):
*  -> LED Color ON
*  -> LED Color OFF

| LED Channel | STM32WL55 Pin | Devicetree Node Alias | Active State |
| :--- | :--- | :--- | :--- |
| **Red** | B3 (Arduino D3) | `led_red_node` | GPIO_ACTIVE_LOW |
| **Green** | B4 (Arduino A3) | `led_green_node` | GPIO_ACTIVE_LOW |
| **Blue** | B8 (Arduino D5) | `led_blue_node` | GPIO_ACTIVE_LOW |

### LDR Light Sensor (Voltage Divider)
* **Sensor Type**: LDR connected in a voltage divider circuit with a fixed resistor.
* **ADC Input**: Channel mapped to `analog-in-pin` alias in the Devicetree overlay.
* **Resolution**: Native 12-bit ADC (range 0 to 4095).
* **Pin Configuration**: Connected to ADC1, channel 5(A0 Arduino). 

### Push Button (B2)
* **Pin**: Mapped to `sw0` alias in Devicetree.
* **Triggering**: Button in input mode, interruption in both flanks (`GPIO_INT_EDGE_BOTH`).



## System Architecture

### Operational Modes
1. **Normal mode**:
   * Brightness percentage is calculated as:
     
   * **Low Light (< 33.3%)**: Red LED turned ON.
   * **Medium Light (33.3% - 66.6%)**: Green LED turned ON.
   * **High Light (> 66.6%)**: White LED turned ON (Red + Green + Blue).
2. **Blue mode**:
   * The RGB LED remains solid **Blue** regardless of ambient light readings.
3. **OFF mode**:
   * All LED channels are turned OFF.


## Thread Decoupling

The application considers a modular and decoupled approach consisting of producers, a central message queue for events (`event_t`), and a consumer. The steps are the following:

1. **Button B2 Service**:
   * Uses a GPIO Callback, Interrupt Service Routine (ISR) and a hardware timer.
   * When pressed, the 1-second timer starts. If 1s elapses while held, it posts inmediatly.
   * When released before 1s, it stops the timer and posts.
2. **LDR Sensor Thread**:
   * A dedicated thread with 1024-byte stack and priority 7 executes a periodic loop with a sequence to measure ADC levels every 2 seconds and posts .
3. **Main Thread**:
   * The main thread receives all the events from the two other modules
   * It stays in an idle state until an event or interruption occurs, as is commanded using `K_FOREVER`. 

## Power Efficiency Considerations

* **Event-Driven Execution**: By blocking threads with  when idle, Zephyr's scheduler places the Cortex-M4 CPU into low-power sleep ( - Wait For Interrupt) automatically when no events are pending ().
* **Zero Polling**: No busy-wait loops are used for button input or timing.
* **Future Optimization**: Ultra-low-power operation can be achieved by powering the LDR voltage divider from a GPIO pin (sensor duty cycling), eliminating static current draw between 2-second sampling intervals.


## Execution Guide

The following steps are necessary in order to execute the application.

### Prerequisites
* Zephyr SDK installed and environment configured. ([Official Installation Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html))
*  STM32CubeProgrammer ([Download from ST site](https://www.st.com/en/development-tools/stm32cubeprog.html))
*  PuTTY ([Download PuTTY](https://putty.org/index.html))

### Steps

1. **Clone this repository**:
   `git clone https://github.com/DavidSMtl/brightness_control_assesment_EPC`

2. **Build the application**: 

    - Using VSCode: 
        1. Ctrl + Shift + P.
        2. Select 'Run Task' option.
        3. Select 'West Build (auto)' and wait for the compilation process to finish.
    - Using Terminal:
        1. Use `west build -p pristine -b nucleo\_wl55jc -- -DDTC\_OVERLAY\_FILE="./boards/nucleo\_wl55jc.overlay" -DCONF\_FILE="./confs/prj\_nucleo\_wl55jc.conf"`
   
3. **Flash to the Nucleo-WL55JC board**:
   - Using VSCode:
        1. Ctrl + Shift + P.
        2. Select 'Run Task' option.
        3. Select 'Flash(CubeProgrammer, UR+HWrst)'.
    - Using Terminal:
        1. west flash

4. **Monitor serial terminal output** (115200 baud):
   1. With a terminal open, execute: `plink -serial COM7 -sercfg 115200,8,n,1,N`


## Authors
* **David Suárez**
* **David Márquez**

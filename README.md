# STM32-Audio-Effects-Unit

![Board v1](https://raw.githubusercontent.com/ouras/STM32-Audio-Effects-Unit/pcb-rtos-port/board/STM32AudioEffects/STM32AudioEffectsUnit.jpg)

Audio effects unit designed for STM32. Includes:
- Library of effects implemented in C (including distortion/overdrive, reverb/echo, and compression)
- Python GUI demo/testbench for effects (based on PyQt, PyPlot, NumPy, and ctypes)
- PCB layout based on STM32F3RETx
- CubeMX file for board (including FreeRTOS, peripheral, and pin configuration)
- main.c (FreeRTOS-based firmware)

Firmware features:
- FreeRTOS for task management
- ADC and DAC with DMA for efficient audio processing (note: sample rate = 40.5 kHz)
- I2C for OLED display and EEPROM
- Interrupts/handlers for user inputs

Board includes:
- 1/4" (6.35mm) input/output jacks
- OLED display
- 4 input potentiometers + 2 volume potentiometers
- 3 buttons
- 3 switches
- EEPROM (for user configuration backups)
- Input and output active gain and noise filter networks
- 9V DC wall adapter, 9V battery, and 5V USB power support (converted to 3.3V)
- Status and power LEDs

A working document for this project (which include requirements, milestones, and technical notes) can be found [here](https://docs.google.com/document/d/13bzRhMHOJ_USe95iWY26JIxKYx1gLJ1tkpusW1AXwGo/edit?usp=sharing).

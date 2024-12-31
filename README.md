# NUCLEO-H503RB-MasterController

## Prerequisites
- CMake >= 3.20
- STM32CubeProgrammer >= 2.18.0 (Any version that supports flashing to STM32H5 should works fine)
- External compiler/toolchain is not needed

## Building the project

Follow the instruction from [https://docs.zephyrproject.org/latest/develop/getting_started/index.html] to install west and Zephyr SDK.

Then, the project can be cloned and build with the following commands.

``` bash
git clone https://github.com/seasea128/NUCLEO-H503-MasterController-Zephyr
west build -b nucleo_h503rb
west flash
```


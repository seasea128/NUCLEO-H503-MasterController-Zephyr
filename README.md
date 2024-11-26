# NUCLEO-H503RB-MasterController

## Building the project

Install west from [https://docs.zephyrproject.org/latest/develop/getting_started/index.html]

Then, install custom branch of Zephyr with:

```bash
west init -m http://github.com/seasea128/zephyr
cd zephyr
git switch stm32h503rb-sd-spi-fix
west sdk install
west update
```

This branch of Zephyr forces SD SPI driver to communicate at 1MHz at start instead of 400kHz. So, some microSD might not work with this project since the SD specs specifies initialization to be at 400kHz.

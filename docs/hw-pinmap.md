# Hardware pin map — to be completed

| Signal | ESP32-S3 GPIO | Notes |
|---|---:|---|
| MCLK | TBD | Shared to DACs |
| BCLK | TBD | Shared |
| LRCLK / WS | TBD | Shared |
| DOUT | TBD | TDM data |
| I2C SDA | TBD | DAC control if required |
| I2C SCL | TBD | DAC control if required |

| DAC | TDM slots | Output assignment |
|---|---|---|
| DAC 1 | TBD | Left low / left high |
| DAC 2 | TBD | Right low / right high |
| DAC 3 | TBD | Sub channels |

Verify the actual module pinout, DAC board configuration and PCM5122 TDM capability before wiring. Do not treat TBD values as validated hardware instructions.

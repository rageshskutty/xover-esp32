# ESP32-S3 Biquad Throughput Benchmark

Measures biquad filter throughput for 6-channel and 2-channel audio
processing on the ESP32-S3, to estimate how many crossover and EQ
stages an active crossover can afford.

`sdkconfig.defaults` assumes a 16 MB flash and octal PSRAM module. Change it if your module differs.

## Target build

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor | tee bench_log.txt
```

## Prebuilt binary

Run the `Firmware binaries` workflow (Actions tab, Run workflow), then download the
`biquad-bench-esp32s3` artifact and flash the merged image:

```bash
esptool.py --chip esp32s3 -p /dev/ttyUSB0 write_flash 0x0 biquad-bench-merged.bin
```

## Host test

```bash
gcc -std=gnu11 -Wall -Wextra -Werror host_test/test_host.c -lm -o host_test && ./host_test
```

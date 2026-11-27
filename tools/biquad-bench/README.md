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

## Wi-Fi load build

Same benchmark code, same matrix, with a Wi-Fi SoftAP and full-speed TCP traffic on core 0
(Wi-Fi task, lwIP and the traffic tasks are pinned to core 0; the benchmark stays on core 1).
The artifact is `biquad-bench-wifi-esp32s3`. Local build:

```bash
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.wifi" build
```

Run it:

1. Flash `biquad-bench-wifi-merged.bin` at 0x0 and open the serial monitor. Use a short, good
   USB cable: Wi-Fi transmit peaks are what browned out the 1.5 m cable before.
2. The board prints `WIFI_STATUS,AP_UP` and waits up to 240 s for traffic.
3. Join the network `xover-bench` from a laptop (password in `main/Kconfig.projbuild`; the AP
   has no uplink, so the laptop will complain about no internet).
4. Run `python tools/biquad-bench/wifi_load_client.py`. When the board has seen three seconds
   of at least 256 kB/s it prints `WIFI_TRAFFIC,YES` and starts the benchmark.
5. Keep the client running until `BENCH_DONE`, then Ctrl-C it.

The log has the usual `CSV,` rows plus `WIFI,<ms>,<rx kB>,<tx kB>,<clients>` lines once per
second (kB per second received from and sent to the client). Check that the rate stayed up for
the whole run. If `WIFI_TRAFFIC,NO_CLIENT` appears, the run is a Wi-Fi-idle run and must be
reported as such.

Limits: SoftAP rather than a station joined to a router, one TCP connection, no audio
streaming or HTTP server, no I2S. It loads core 0, the radio and the shared memory buses; it
is not the ADR 0009 measurement.

## Host test

```bash
gcc -std=gnu11 -Wall -Wextra -Werror host_test/test_host.c -lm -o host_test && ./host_test
```

# Host tests

Build DSP primitives on a desktop compiler and compare their impulse/frequency responses with trusted reference calculations (for example scipy.signal). Keep test vectors and expected results versioned. Host tests do not replace on-target timing and memory tests.

## Running host tests

```sh
cmake -S host -B host/build
cmake --build host/build
ctest --test-dir host/build --output-on-failure -C Debug
```

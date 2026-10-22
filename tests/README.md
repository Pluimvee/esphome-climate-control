# Host tests

Unit tests that compile the component sources on a PC, with minimal stubs for
the ESPHome headers in `stub/` (no ESPHome toolchain needed).

```bash
cd tests
g++ -std=c++17 -Wall -I stub -I ../components/climate_control \
    test_trend.cpp ../components/climate_control/temperature.cpp -o test_trend
./test_trend
```

`test_trend.cpp` feeds `Temperature` known ramps (with noise, jitter, a
`millis()` wrap and a sensor gap) and checks `trend()` in °C/hour.

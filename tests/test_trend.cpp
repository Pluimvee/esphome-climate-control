// Test harness for Temperature::trend() (esphome-climate-control) with stubbed ESPHome headers.
#include <cstdio>
#include <cmath>
#include <random>
#include "climate_control.h"
uint32_t g_millis = 1;
using esphome::climate_control::Temperature;

// Feed a ramp of `rate` K/h for `minutes`, one sample every `interval_s` (+/- jitter), return final trend().
static float run(float rate, float interval_s, float minutes, float noise = 0.0f, float jitter = 0.0f,
                 uint32_t start_ms = 1000) {
  Temperature t;
  t.setup(-50.0f, 100.0f, 5);
  std::mt19937 rng(42);
  std::normal_distribution<float> n(0.0f, noise > 0 ? noise : 1e-9f);
  std::uniform_real_distribution<float> j(-jitter, jitter);
  g_millis = start_ms;
  float elapsed_s = 0.0f;
  while (elapsed_s < minutes * 60.0f) {
    float dt = interval_s + (jitter > 0 ? j(rng) : 0.0f);
    elapsed_s += dt;
    g_millis += (uint32_t) (dt * 1000.0f);
    t.set(20.0f + rate * elapsed_s / 3600.0f + (noise > 0 ? n(rng) : 0.0f));
  }
  return t.trend();
}

int main() {
  int fail = 0;
  auto check = [&](const char *name, float got, float want, float tol) {
    bool ok = std::fabs(got - want) <= tol;
    if (!ok) fail++;
    printf("%-46s trend %6.3f K/h  expected %6.3f +/- %.3f  %s\n", name, got, want, tol, ok ? "ok" : "FAIL");
  };
  check("ramp 1.0 K/h, 10 s samples",          run(1.0f, 10, 60),        1.0f, 0.05f);
  check("ramp -1.0 K/h, 10 s samples",         run(-1.0f, 10, 60),      -1.0f, 0.05f);
  check("ramp 1.0 K/h, 60 s samples",          run(1.0f, 60, 240),       1.0f, 0.05f);
  check("ramp 0.5 K/h, 10 s samples",          run(0.5f, 10, 60),        0.5f, 0.05f);
  check("ramp 0.3 K/h, 10 s samples",          run(0.3f, 10, 60),        0.3f, 0.05f);
  check("ramp 0.1 K/h (below 0.2 deadband)",   run(0.1f, 10, 60),        0.0f, 0.001f);
  check("flat 20.0 with 0.05 K noise",         run(0.0f, 10, 60, 0.05f), 0.0f, 0.001f);
  check("ramp 1.0 K/h, 10 s +/- 5 s jitter",   run(1.0f, 10, 60, 0.0f, 5.0f), 1.0f, 0.08f);
  check("ramp 1.0 K/h with 0.05 K noise",       run(1.0f, 10, 60, 0.05f), 1.0f, 0.15f);
  check("ramp 1.0 K/h across millis() wrap",    run(1.0f, 10, 60, 0.0f, 0.0f, 0xFFFFFFFFu - 600000u), 1.0f, 0.05f);
  {
    Temperature t;
    t.setup(-50.0f, 100.0f, 5);
    g_millis = 1000;
    for (int i = 0; i < 200; i++) { g_millis += 10000; t.set(20.0f + i * 0.01f); }
    g_millis += 6 * 60 * 1000;  // 6 min without samples: older than max_age 5 min
    float v = t.trend();
    bool ok = std::isnan(v);
    if (!ok) fail++;
    printf("%-46s trend %6.3f            expected NAN  %s\n", "expired after 6 min without samples", v, ok ? "ok" : "FAIL");
  }
  printf("%s\n", fail ? "FAILED" : "ALL OK");
  return fail ? 1 : 0;
}

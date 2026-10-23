#pragma once
#include <algorithm>
namespace esphome { class PollingComponent { public: virtual ~PollingComponent() = default; virtual void setup() {} virtual void update() {} }; }

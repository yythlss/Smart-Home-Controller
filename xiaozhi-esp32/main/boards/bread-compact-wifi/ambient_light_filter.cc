#include "ambient_light_filter.h"

#include <algorithm>

namespace {
constexpr float kFilterWeight = 0.25f;
}

AmbientLightFilter::AmbientLightFilter(int dark_raw, int bright_raw)
    : dark_raw_(dark_raw), bright_raw_(bright_raw) {
}

float AmbientLightFilter::Normalize(int raw) const {
    if (dark_raw_ == bright_raw_) {
        return raw >= bright_raw_ ? 100.0f : 0.0f;
    }

    const float percent = static_cast<float>(raw - dark_raw_) * 100.0f /
                          static_cast<float>(bright_raw_ - dark_raw_);
    return std::max(0.0f, std::min(100.0f, percent));
}

float AmbientLightFilter::PushSample(int raw) {
    const float normalized = Normalize(raw);
    if (!has_filtered_value_) {
        filtered_percent_ = normalized;
        has_filtered_value_ = true;
        return filtered_percent_;
    }

    filtered_percent_ += (normalized - filtered_percent_) * kFilterWeight;
    return filtered_percent_;
}

void AmbientLightFilter::Reset() {
    has_filtered_value_ = false;
    filtered_percent_ = 0.0f;
}

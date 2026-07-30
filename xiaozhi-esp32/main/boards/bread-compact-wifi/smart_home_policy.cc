#include "smart_home_policy.h"

#include <algorithm>

namespace {
int ClampLevel(int level) {
    return std::max(0, std::min(3, level));
}

bool IsKnownVacant(const SmartHomePolicyInput& input) {
    return input.occupancy_known && !input.occupied;
}

int AutoPurifierLevel(const SmartHomePolicyInput& input) {
    if (!input.sample.has_air_quality) {
        return 0;
    }
    if (input.sample.mq135_raw >= 2000) {
        return 3;
    }
    if (input.current_purifier_level >= 3 && input.sample.mq135_raw >= 1800) {
        return 3;
    }
    if (input.sample.mq135_raw >= 1000) {
        return 2;
    }
    if (input.current_purifier_level >= 2 && input.sample.mq135_raw >= 900) {
        return 2;
    }
    return 0;
}

int AutoFreshAirLevel(const SmartHomePolicyInput& input) {
    if ((input.sample.has_air_quality && input.sample.mq135_raw >= 2000) ||
        (input.sample.has_temperature && input.sample.temperature_c > 30.0f)) {
        return 2;
    }
    if (input.current_fresh_air_level > 0 &&
        ((input.sample.has_air_quality && input.sample.mq135_raw >= 900) ||
         (input.sample.has_temperature && input.sample.temperature_c > 29.0f))) {
        return 1;
    }
    return 0;
}

int AutoHumidifierLevel(const SmartHomePolicyInput& input) {
    if (!input.sample.has_humidity) {
        return 0;
    }
    if (input.sample.humidity_percent < 40.0f) {
        return 2;
    }
    if (input.current_humidifier_level > 0 && input.sample.humidity_percent < 45.0f) {
        return 1;
    }
    return 0;
}

void ApplyAutomationRule(const SmartHomePolicyInput& input, SmartHomePolicyOutput& output) {
    if (!input.rule.enabled) {
        return;
    }

    if (input.sample.has_air_quality && input.sample.air_score < input.rule.air_score_below) {
        output.automation_rule_active = true;
        if (!input.purifier_override_active) {
            output.purifier_level = std::max(output.purifier_level,
                                             ClampLevel(input.rule.purifier_level));
        }
        if (!input.fresh_air_override_active) {
            output.fresh_air_level = std::max(output.fresh_air_level,
                                              ClampLevel(input.rule.fresh_air_level));
        }
    }
    if (input.sample.has_humidity && input.sample.humidity_percent < input.rule.humidity_below) {
        output.automation_rule_active = true;
        if (!input.humidifier_override_active) {
            output.humidifier_level = std::max(output.humidifier_level,
                                               ClampLevel(input.rule.humidifier_level));
        }
    }
    if (input.sample.has_temperature && input.sample.temperature_c > input.rule.temperature_above) {
        output.automation_rule_active = true;
        if (!input.fresh_air_override_active) {
            output.fresh_air_level = std::max(output.fresh_air_level,
                                              ClampLevel(input.rule.fresh_air_level));
        }
    }
}
} // namespace

SmartHomePolicyOutput EvaluateSmartHomeAutoPolicy(const SmartHomePolicyInput& input) {
    SmartHomePolicyOutput output = {};
    if (IsKnownVacant(input)) {
        output.no_occupancy_shutdown = true;
        return output;
    }

    output.purifier_level = input.purifier_override_active
        ? ClampLevel(input.current_purifier_level) : AutoPurifierLevel(input);
    output.fresh_air_level = input.fresh_air_override_active
        ? ClampLevel(input.current_fresh_air_level) : AutoFreshAirLevel(input);
    output.humidifier_level = input.humidifier_override_active
        ? ClampLevel(input.current_humidifier_level) : AutoHumidifierLevel(input);
    ApplyAutomationRule(input, output);
    return output;
}

SmartHomePolicyOutput EvaluateSmartHomeEcoPolicy(const SmartHomePolicyInput& input) {
    SmartHomePolicyOutput output = {};
    if (IsKnownVacant(input)) {
        output.no_occupancy_shutdown = true;
        return output;
    }

    int purifier_level = 0;
    int fresh_air_level = 0;
    int humidifier_level = 0;
    if (input.sample.has_air_quality) {
        if (input.sample.mq135_raw >= 2000 || input.sample.air_score < 40) {
            purifier_level = 2;
            fresh_air_level = 1;
        } else if (input.sample.mq135_raw >= 1000 || input.sample.air_score < 65) {
            purifier_level = 1;
        }
    }
    if (input.sample.has_humidity && input.sample.humidity_percent < 35.0f) {
        humidifier_level = 1;
    }
    if (input.sample.has_temperature && input.sample.temperature_c > 30.0f) {
        fresh_air_level = std::max(fresh_air_level, 1);
    }

    output.purifier_level = input.purifier_override_active
        ? ClampLevel(input.current_purifier_level) : purifier_level;
    output.fresh_air_level = input.fresh_air_override_active
        ? ClampLevel(input.current_fresh_air_level) : fresh_air_level;
    output.humidifier_level = input.humidifier_override_active
        ? ClampLevel(input.current_humidifier_level) : humidifier_level;
    return output;
}

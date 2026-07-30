#include "smart_home_policy.h"

#include <cassert>

namespace {
SmartHomePolicyInput BaseInput() {
    SmartHomePolicyInput input = {};
    input.occupancy_known = true;
    input.occupied = true;
    input.sample.has_temperature = true;
    input.sample.temperature_c = 26.0f;
    input.sample.has_humidity = true;
    input.sample.humidity_percent = 55.0f;
    input.sample.has_air_quality = true;
    input.sample.mq135_raw = 500;
    input.sample.air_score = 88;
    return input;
}
} // namespace

int main() {
    auto input = BaseInput();
    input.sample.mq135_raw = 2200;
    auto output = EvaluateSmartHomeAutoPolicy(input);
    assert(output.purifier_level == 3);
    assert(output.fresh_air_level == 2);

    input.current_purifier_level = output.purifier_level;
    input.current_fresh_air_level = output.fresh_air_level;
    input.sample.mq135_raw = 1400;
    input.sample.air_score = 60;
    output = EvaluateSmartHomeAutoPolicy(input);
    assert(output.purifier_level == 2);
    assert(output.fresh_air_level == 1);

    input.current_purifier_level = output.purifier_level;
    input.current_fresh_air_level = output.fresh_air_level;
    input.sample.mq135_raw = 500;
    input.sample.air_score = 88;
    output = EvaluateSmartHomeAutoPolicy(input);
    assert(output.purifier_level == 0);
    assert(output.fresh_air_level == 0);

    input = BaseInput();
    input.current_humidifier_level = 2;
    input.sample.humidity_percent = 44.0f;
    output = EvaluateSmartHomeAutoPolicy(input);
    assert(output.humidifier_level == 1);
    input.current_humidifier_level = output.humidifier_level;
    input.sample.humidity_percent = 45.0f;
    output = EvaluateSmartHomeAutoPolicy(input);
    assert(output.humidifier_level == 0);

    input = BaseInput();
    input.rule.enabled = true;
    input.rule.humidity_below = 60;
    input.rule.humidifier_level = 3;
    output = EvaluateSmartHomeAutoPolicy(input);
    assert(output.automation_rule_active);
    assert(output.humidifier_level == 3);

    input.current_humidifier_level = output.humidifier_level;
    input.sample.humidity_percent = 65.0f;
    output = EvaluateSmartHomeAutoPolicy(input);
    assert(!output.automation_rule_active);
    assert(output.humidifier_level == 0);

    input = BaseInput();
    input.purifier_override_active = true;
    input.current_purifier_level = 1;
    input.sample.mq135_raw = 2400;
    output = EvaluateSmartHomeAutoPolicy(input);
    assert(output.purifier_level == 1);

    input.occupied = false;
    output = EvaluateSmartHomeAutoPolicy(input);
    assert(output.no_occupancy_shutdown);
    assert(output.purifier_level == 0);

    input = BaseInput();
    input.sample.has_humidity = false;
    input.sample.has_air_quality = false;
    input.sample.has_temperature = false;
    output = EvaluateSmartHomeAutoPolicy(input);
    assert(output.purifier_level == 0);
    assert(output.fresh_air_level == 0);
    assert(output.humidifier_level == 0);

    input = BaseInput();
    input.sample.mq135_raw = 2200;
    input.sample.air_score = 30;
    output = EvaluateSmartHomeEcoPolicy(input);
    assert(output.purifier_level == 2);
    assert(output.fresh_air_level == 1);

    input.purifier_override_active = true;
    input.current_purifier_level = 3;
    output = EvaluateSmartHomeEcoPolicy(input);
    assert(output.purifier_level == 3);
    return 0;
}

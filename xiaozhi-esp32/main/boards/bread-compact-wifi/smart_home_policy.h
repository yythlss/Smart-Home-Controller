#ifndef SMART_HOME_POLICY_H
#define SMART_HOME_POLICY_H

struct SmartHomePolicySample {
    bool has_temperature = false;
    float temperature_c = 0.0f;
    bool has_humidity = false;
    float humidity_percent = 0.0f;
    bool has_air_quality = false;
    int mq135_raw = 0;
    int air_score = 0;
};

struct SmartHomePolicyRule {
    bool enabled = false;
    int air_score_below = 60;
    int humidity_below = 35;
    int temperature_above = 30;
    int purifier_level = 3;
    int fresh_air_level = 2;
    int humidifier_level = 2;
};

struct SmartHomePolicyInput {
    SmartHomePolicySample sample;
    SmartHomePolicyRule rule;
    bool occupancy_known = false;
    bool occupied = false;
    int current_purifier_level = 0;
    int current_fresh_air_level = 0;
    int current_humidifier_level = 0;
    bool purifier_override_active = false;
    bool fresh_air_override_active = false;
    bool humidifier_override_active = false;
};

struct SmartHomePolicyOutput {
    int purifier_level = 0;
    int fresh_air_level = 0;
    int humidifier_level = 0;
    bool automation_rule_active = false;
    bool no_occupancy_shutdown = false;
};

SmartHomePolicyOutput EvaluateSmartHomeAutoPolicy(const SmartHomePolicyInput& input);
SmartHomePolicyOutput EvaluateSmartHomeEcoPolicy(const SmartHomePolicyInput& input);

#endif // SMART_HOME_POLICY_H

#ifndef AMBIENT_LIGHT_FILTER_H
#define AMBIENT_LIGHT_FILTER_H

class AmbientLightFilter {
public:
    AmbientLightFilter(int dark_raw, int bright_raw);

    float Normalize(int raw) const;
    float PushSample(int raw);
    void Reset();

private:
    int dark_raw_;
    int bright_raw_;
    bool has_filtered_value_ = false;
    float filtered_percent_ = 0.0f;
};

#endif // AMBIENT_LIGHT_FILTER_H

#ifndef LD2450_SENSOR_H
#define LD2450_SENSOR_H

#include "ld2450_protocol.h"

#include <driver/gpio.h>
#include <driver/uart.h>

#include <cstddef>
#include <cstdint>

class Ld2450Sensor {
public:
    Ld2450Sensor(uart_port_t uart_port, gpio_num_t tx_pin, gpio_num_t rx_pin, int baud_rate);
    ~Ld2450Sensor();

    bool Initialize();
    bool Poll(Ld2450Snapshot& snapshot, TickType_t timeout);
    int SendCommand(const uint8_t* data, size_t size);
    uint32_t GetReceivedByteCount() const { return received_byte_count_; }
    uint32_t GetValidFrameCount() const { return valid_frame_count_; }
    uint32_t GetRejectedFrameCount() const { return rejected_frame_count_; }
    bool HasBufferedData() const { return receive_size_ > 0; }
    size_t GetBufferedDataSize() const { return receive_size_; }
    const uint8_t* GetBufferedData() const { return receive_buffer_; }
    bool HasRawSample() const { return raw_sample_size_ > 0; }
    size_t GetRawSampleSize() const { return raw_sample_size_; }
    const uint8_t* GetRawSample() const { return raw_sample_buffer_; }

private:
    static constexpr size_t kReceiveBufferSize = 160;
    static constexpr size_t kRawSampleBufferSize = 64;

    bool TryExtractFrame(Ld2450Snapshot& snapshot);
    void Append(const uint8_t* data, size_t size);
    void SaveRawSample(const uint8_t* data, size_t size);

    uart_port_t uart_port_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    int baud_rate_;
    uint8_t receive_buffer_[kReceiveBufferSize] = {};
    uint8_t raw_sample_buffer_[kRawSampleBufferSize] = {};
    size_t receive_size_ = 0;
    size_t raw_sample_size_ = 0;
    uint32_t received_byte_count_ = 0;
    uint32_t valid_frame_count_ = 0;
    uint32_t rejected_frame_count_ = 0;
    bool initialized_ = false;
};

#endif // LD2450_SENSOR_H

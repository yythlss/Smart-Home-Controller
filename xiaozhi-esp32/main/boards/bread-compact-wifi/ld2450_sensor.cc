#include "ld2450_sensor.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>

#define TAG "LD2450"

Ld2450Sensor::Ld2450Sensor(uart_port_t uart_port, gpio_num_t tx_pin, gpio_num_t rx_pin,
                           int baud_rate)
    : uart_port_(uart_port), tx_pin_(tx_pin), rx_pin_(rx_pin), baud_rate_(baud_rate) {
}

Ld2450Sensor::~Ld2450Sensor() {
    if (initialized_) {
        uart_driver_delete(uart_port_);
    }
}

int Ld2450Sensor::SendCommand(const uint8_t* data, size_t size) {
    if (!initialized_ || data == nullptr || size == 0) {
        return -1;
    }
    uart_flush_input(uart_port_);
    return uart_write_bytes(uart_port_, data, size);
}

bool Ld2450Sensor::Initialize() {
    if (initialized_) {
        return true;
    }

    uart_config_t config = {};
    config.baud_rate = baud_rate_;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_param_config(uart_port_, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        return false;
    }
    err = uart_set_pin(uart_port_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        return false;
    }
    err = uart_driver_install(uart_port_, 2048, 0, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Initialized UART%d tx=%d rx=%d baud=%d", static_cast<int>(uart_port_),
             static_cast<int>(tx_pin_), static_cast<int>(rx_pin_), baud_rate_);
    return true;
}

bool Ld2450Sensor::Poll(Ld2450Snapshot& snapshot, TickType_t timeout) {
    if (!initialized_ && !Initialize()) {
        return false;
    }
    if (TryExtractFrame(snapshot)) {
        return true;
    }

    uint8_t incoming[64] = {};
    const int received = uart_read_bytes(uart_port_, incoming, sizeof(incoming), timeout);
    if (received <= 0) {
        return false;
    }

    received_byte_count_ += static_cast<uint32_t>(received);
    SaveRawSample(incoming, static_cast<size_t>(received));
    Append(incoming, static_cast<size_t>(received));
    return TryExtractFrame(snapshot);
}

void Ld2450Sensor::SaveRawSample(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return;
    }
    raw_sample_size_ = size < kRawSampleBufferSize ? size : kRawSampleBufferSize;
    std::memcpy(raw_sample_buffer_, data, raw_sample_size_);
}

void Ld2450Sensor::Append(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return;
    }
    if (size >= kReceiveBufferSize) {
        std::memcpy(receive_buffer_, data + size - kReceiveBufferSize, kReceiveBufferSize);
        receive_size_ = kReceiveBufferSize;
        return;
    }
    if (receive_size_ + size > kReceiveBufferSize) {
        const size_t keep = kReceiveBufferSize - size;
        std::memmove(receive_buffer_, receive_buffer_ + receive_size_ - keep, keep);
        receive_size_ = keep;
    }
    std::memcpy(receive_buffer_ + receive_size_, data, size);
    receive_size_ += size;
}

bool Ld2450Sensor::TryExtractFrame(Ld2450Snapshot& snapshot) {
    while (receive_size_ >= 4) {
        size_t header_index = 0;
        while (header_index + 4 <= receive_size_ &&
               !Ld2450Protocol::HasTargetFrameHeader(receive_buffer_ + header_index,
                                                      receive_size_ - header_index)) {
            ++header_index;
        }

        if (header_index + 4 > receive_size_) {
            const size_t keep = receive_size_ > 3 ? 3 : receive_size_;
            std::memmove(receive_buffer_, receive_buffer_ + receive_size_ - keep, keep);
            receive_size_ = keep;
            return false;
        }
        if (header_index > 0) {
            std::memmove(receive_buffer_, receive_buffer_ + header_index, receive_size_ - header_index);
            receive_size_ -= header_index;
        }
        const size_t frame_size = Ld2450Protocol::GetTargetFrameSize(receive_buffer_, receive_size_);
        if (frame_size == 0) {
            return false;
        }

        if (Ld2450Protocol::DecodeTargetFrame(receive_buffer_, frame_size, snapshot)) {
            std::memmove(receive_buffer_, receive_buffer_ + frame_size, receive_size_ - frame_size);
            receive_size_ -= frame_size;
            ++valid_frame_count_;
            return true;
        }

        ++rejected_frame_count_;
        std::memmove(receive_buffer_, receive_buffer_ + 1, receive_size_ - 1);
        --receive_size_;
    }
    return false;
}

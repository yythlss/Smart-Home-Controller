#include "ld2450_protocol.h"

namespace {

constexpr uint8_t kFrameHeader[] = {0xAA, 0xFF, 0x03, 0x00};
constexpr uint8_t kFrameFooter[] = {0x55, 0xCC};

/// 目标数据在帧内的起始偏移 (紧跟帧头)
constexpr size_t kTargetStartOffset = 4;

/// 每个目标数据块固定 8 字节: X(2) + Y(2) + Speed(2) + Resolution(2)
constexpr size_t kTargetStride = 8;

/// 小端序读取 uint16
uint16_t ReadU16Le(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

/// V1.1 15-bit 符号-幅度解码
///
/// HLK-LD2450 使用非标准编码:
///   bit15 = 1 → 正数, 值 = raw & 0x7FFF
///   bit15 = 0 → 负数, 值 = -(raw & 0x7FFF)
/// 等价于: (int16_t)(raw - 0x8000)
///
/// 参考: HLK-LD2450 使用教程 V1.1, 坐标编码章节
int16_t DecodeS15(uint16_t raw) {
    if (raw & 0x8000) {
        return static_cast<int16_t>(raw & 0x7FFF);
    }
    return -static_cast<int16_t>(raw & 0x7FFF);
}

}  // namespace

bool Ld2450Protocol::HasTargetFrameHeader(const uint8_t* data, size_t size) {
    if (data == nullptr || size < sizeof(kFrameHeader)) {
        return false;
    }
    for (size_t i = 0; i < sizeof(kFrameHeader); ++i) {
        if (data[i] != kFrameHeader[i]) {
            return false;
        }
    }
    return true;
}

size_t Ld2450Protocol::GetTargetFrameSize(const uint8_t* data, size_t size) {
    if (!HasTargetFrameHeader(data, size)) {
        return 0;
    }
    if (size < kTargetFrameSize) {
        return 0;  // 数据不够一帧
    }
    return kTargetFrameSize;
}

bool Ld2450Protocol::DecodeTargetFrame(const uint8_t* data, size_t size,
                                       Ld2450Snapshot& snapshot) {
    snapshot = {};

    // 1. 校验帧大小
    if (size != kTargetFrameSize) {
        return false;
    }

    // 2. 校验帧头
    if (!HasTargetFrameHeader(data, size)) {
        return false;
    }

    // 3. 校验帧尾
    if (data[kTargetFrameSize - 2] != kFrameFooter[0] ||
        data[kTargetFrameSize - 1] != kFrameFooter[1]) {
        return false;
    }

    // 4. 解码三个目标
    snapshot.valid = true;
    for (size_t index = 0; index < 3; ++index) {
        const size_t offset = kTargetStartOffset + index * kTargetStride;
        Ld2450Target& target = snapshot.targets[index];

        const uint16_t x_raw = ReadU16Le(&data[offset]);
        const uint16_t y_raw = ReadU16Le(&data[offset + 2]);
        const uint16_t speed_raw = ReadU16Le(&data[offset + 4]);
        const uint16_t resolution = ReadU16Le(&data[offset + 6]);

        // V1.1: X、Y、Speed 均为 15-bit 符号-幅度编码
        target.x_mm = DecodeS15(x_raw);
        target.y_mm = DecodeS15(y_raw);
        // 速度原始单位 cm/s, 转为 mm/s (×10)
        target.speed_mm_per_s = static_cast<int16_t>(DecodeS15(speed_raw) * 10);
        target.resolution_mm = resolution;

        // V1.1: X 和 Y 同时为 0 表示空槽位
        target.active = (x_raw != 0 || y_raw != 0);
        if (target.active) {
            ++snapshot.active_target_count;
        }
    }

    return true;
}

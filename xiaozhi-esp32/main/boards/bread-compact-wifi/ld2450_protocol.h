#ifndef LD2450_PROTOCOL_H
#define LD2450_PROTOCOL_H

#include <cstddef>
#include <cstdint>

/// HLK-LD2450 单目标数据 (V1.1 协议)
struct Ld2450Target {
    bool active = false;           // 该目标槽位是否有效 (X、Y 不全为 0)
    int16_t x_mm = 0;              // X 坐标 (mm), 负=左, 正=右
    int16_t y_mm = 0;              // Y 坐标 (mm), 负=后, 正=前
    int16_t speed_mm_per_s = 0;    // 速度 (mm/s), 负=靠近, 正=远离
    uint16_t resolution_mm = 0;    // 距离分辨率 (mm)
};

/// HLK-LD2450 一帧快照 (V1.1 协议, 最多 3 个目标)
struct Ld2450Snapshot {
    bool valid = false;
    uint8_t active_target_count = 0;
    Ld2450Target targets[3] = {};
};

/// HLK-LD2450 串口协议解析 (V1.1)
///
/// 数据帧格式 (固定 30 字节):
///   Bytes 0-3:   帧头 AA FF 03 00
///   Bytes 4-11:  目标1 (X/Y/速度/分辨率, 各 2 字节 LE)
///   Bytes 12-19: 目标2
///   Bytes 20-27: 目标3
///   Bytes 28-29: 帧尾 55 CC
///
/// 坐标编码 (15-bit 符号-幅度):
///   bit15=1 → 正数: value & 0x7FFF
///   bit15=0 → 负数: -(value & 0x7FFF)
class Ld2450Protocol {
public:
    /// V1.1 目标跟踪数据帧大小
    static constexpr size_t kTargetFrameSize = 30;

    /// 检查缓冲区开头是否为有效帧头 AA FF 03 00
    static bool HasTargetFrameHeader(const uint8_t* data, size_t size);

    /// 返回当前帧大小: 30 (标准帧), 0 (数据不够或帧头无效)
    static size_t GetTargetFrameSize(const uint8_t* data, size_t size);

    /// 解码一帧目标跟踪数据
    /// @return true 解码成功, false 校验失败
    static bool DecodeTargetFrame(const uint8_t* data, size_t size, Ld2450Snapshot& snapshot);
};

#endif // LD2450_PROTOCOL_H

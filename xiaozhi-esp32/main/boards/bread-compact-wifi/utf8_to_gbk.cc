#include "utf8_to_gbk.h"

#include <cstdint>
#include <cstring>

namespace {

// ========== 完整 Unicode → GBK 映射表（21791 条，85.1 KB）==========
#include "gbk_table.inc"

// 二分查找 GBK 映射
const FullGbkEntry* FindGbkEntry(uint16_t unicode) {
    int lo = 0;
    int hi = static_cast<int>(kFullGbkTableSize) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (kFullGbkTable[mid].unicode == unicode) {
            return &kFullGbkTable[mid];
        } else if (kFullGbkTable[mid].unicode < unicode) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return nullptr;
}

// 解码一个 UTF-8 多字节序列，返回 Unicode 码点
// 调用前已确认 byte0 >= 0x80
// 返回: 成功返回码点；失败返回 0
uint32_t DecodeUtf8(const char* input, size_t input_len, size_t* consumed) {
    *consumed = 1;
    uint8_t b0 = static_cast<uint8_t>(input[0]);

    if ((b0 & 0xE0) == 0xC0) {
        // 2 字节: 110xxxxx 10xxxxxx
        if (input_len < 2) return 0;
        uint8_t b1 = static_cast<uint8_t>(input[1]);
        if ((b1 & 0xC0) != 0x80) return 0;
        *consumed = 2;
        return ((b0 & 0x1F) << 6) | (b1 & 0x3F);
    }

    if ((b0 & 0xF0) == 0xE0) {
        // 3 字节: 1110xxxx 10xxxxxx 10xxxxxx
        if (input_len < 3) return 0;
        uint8_t b1 = static_cast<uint8_t>(input[1]);
        uint8_t b2 = static_cast<uint8_t>(input[2]);
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return 0;
        *consumed = 3;
        return ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
    }

    if ((b0 & 0xF8) == 0xF0) {
        // 4 字节: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if (input_len < 4) return 0;
        uint8_t b1 = static_cast<uint8_t>(input[1]);
        uint8_t b2 = static_cast<uint8_t>(input[2]);
        uint8_t b3 = static_cast<uint8_t>(input[3]);
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return 0;
        *consumed = 4;
        return ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
    }

    // 非法或超出范围
    *consumed = 1;
    return 0;
}

}  // namespace

size_t Utf8ToGbk(const char* input, char* output, size_t output_size) {
    if (input == nullptr || output == nullptr || output_size == 0) {
        return 0;
    }

    size_t in_len = std::strlen(input);
    size_t out = 0;

    for (size_t i = 0; i < in_len && out + 1 < output_size; ) {
        uint8_t ch = static_cast<uint8_t>(input[i]);

        if (ch < 0x80) {
            // ASCII 字符，直通
            output[out++] = static_cast<char>(ch);
            ++i;
        } else {
            // 多字节 UTF-8 序列，解码并查找 GBK 映射
            size_t consumed = 1;
            uint32_t unicode = DecodeUtf8(input + i, in_len - i, &consumed);

            const FullGbkEntry* entry = (unicode != 0)
                ? FindGbkEntry(static_cast<uint16_t>(unicode))
                : nullptr;

            if (entry != nullptr) {
                // 找到 GBK 映射，输出 2 字节
                if (out + 2 < output_size) {
                    output[out++] = static_cast<char>(entry->gbk_hi);
                    output[out++] = static_cast<char>(entry->gbk_lo);
                } else {
                    break;  // 缓冲区不足
                }
            } else {
                // 未找到映射，原样复制 UTF-8 字节（兼容 emoji 等 GBK 不支持的字符）
                for (size_t j = 0; j < consumed && i + j < in_len && out + 1 < output_size; ++j) {
                    output[out++] = input[i + j];
                }
            }
            i += consumed;
        }
    }

    output[out] = '\0';
    return out;
}

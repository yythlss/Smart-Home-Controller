#ifndef UTF8_TO_GBK_H
#define UTF8_TO_GBK_H

#include <cstddef>

/**
 * @brief 将 UTF-8 字符串转换为 GBK 编码
 *
 * 转换表来自同目录 `gbk_table.inc`，用于让 USART HMI/TJC 屏幕的 GBK 字库正确显示中文。
 * ASCII 字符（0x00-0x7F）原样透传；不在 GBK 表中的 UTF-8 多字节序列保持原样，
 * 方便保留 emoji 或 GBK 不支持的符号，但这类字符是否能显示取决于屏幕字库。
 *
 * @param input        输入 UTF-8 字符串（以 \0 结尾）
 * @param output       输出缓冲区
 * @param output_size  输出缓冲区大小（字节）
 * @return 写入的字节数（不含结尾 \0）
 */
size_t Utf8ToGbk(const char* input, char* output, size_t output_size);

#endif // UTF8_TO_GBK_H

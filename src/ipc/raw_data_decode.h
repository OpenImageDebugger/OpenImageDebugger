#ifndef RAW_DATA_DECODE_H_
#define RAW_DATA_DECODE_H_

#include <vector>

enum class BufferType {
    UnsignedByte  = 0,
    UnsignedShort = 2,
    Short         = 3,
    Int32         = 4,
    Float32       = 5,
    Float64       = 6
};

std::vector<uint8_t>
    make_float_buffer_from_double(const std::vector<uint8_t>& buff_double);

size_t typesize(BufferType type);

#endif // RAW_DATA_DECODE_H_

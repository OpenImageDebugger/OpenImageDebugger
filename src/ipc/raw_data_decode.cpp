#include "raw_data_decode.h"

std::vector<uint8_t>
    make_float_buffer_from_double(const std::vector<uint8_t>& buff_double)
{
    int element_count = buff_double.size() / sizeof(double);
    std::vector<uint8_t> buff_float(element_count);

    // Cast from double to float
    const double* src = reinterpret_cast<const double*>(buff_double.data());
    float* dst = reinterpret_cast<float*>(buff_float.data());
    for (int i = 0; i < element_count; ++i) {
        dst[i] = static_cast<float>(src[i]);
    }

    return buff_float;
}


size_t typesize(BufferType type)
{
    switch(type) {
    case BufferType::Int32:
        return sizeof(int32_t);
    case BufferType::Short:
    case BufferType::UnsignedShort:
        return sizeof(int16_t);
    case BufferType::Float32:
        return sizeof(float);
    case BufferType::Float64:
        return sizeof(double);
    case BufferType::UnsignedByte:
        return sizeof(uint8_t);
    }
}

#include <memory>
#include <QPixmap>

#include "buffer_exporter.hpp"

template<typename T> float get_multiplier() {
    return 255.f/static_cast<float>(std::numeric_limits<T>::max());
}

template<>
float get_multiplier<float>() {
    return 255.f;
}

template<typename T> T get_max_intensity() {
    return std::numeric_limits<T>::max();
}

template<>
float get_max_intensity<float>() {
    return 1.f;
}

template<typename T>
void export_bitmap(const char *fname, const Buffer *buffer, int channels)
{
    int width_i = static_cast<int>(buffer->buffer_width_f);
    int height_i = static_cast<int>(buffer->buffer_height_f);

    shared_ptr<uint8_t> processed_buffer(new uint8_t[4 * width_i * height_i]);
    uint8_t default_channel_vals[] = {0, 0, 0, 255};

    uint8_t* out_ptr = processed_buffer.get();
    const T* in_ptr = reinterpret_cast<T*>(buffer->buffer);

    const float *bc_comp = buffer->auto_buffer_contrast_brightness();
    float color_scale = get_multiplier<T>();

    const float maxIntensity = get_max_intensity<T>();

    for(int y = height_i - 1; y >= 0; --y) {
        for(int x = width_i - 1; x >= 0; --x) {
            int c;
            for(c = 0; c < channels; ++c)  {
                float in_val = static_cast<float>(in_ptr[y * channels * width_i + x * channels + c]);
                out_ptr[y * 4 * width_i + x * 4 + c] = static_cast<uint8_t>(
                            clamp((in_val * bc_comp[c] + bc_comp[4 + c]*maxIntensity)*color_scale, 0.f, 255.f));
            }
            if(channels == 1) {
                // Grayscale: Repeat first channel into G and B
                for(; c < 3; ++c) {
                  out_ptr[y * 4 * width_i + x * 4 + c] = out_ptr[y * 4 * width_i + x * 4];
                }
                out_ptr[y * 4 * width_i + x * 4 + c] = 255;
            } else {
                for(; c < 4; ++c) {
                  out_ptr[y * 4 * width_i + x * 4 + c] = default_channel_vals[c];
                }
            }
        }
    }

    const int bytes_per_line = width_i * 4;
    QImage output_image(processed_buffer.get(), width_i, height_i, bytes_per_line, QImage::Format_RGBA8888);
    output_image.save(fname, "png");
}

template<typename T>
const char* get_type_descriptor();

template<>
const char* get_type_descriptor<uint8_t>() {
    return "uint8";
}

template<>
const char* get_type_descriptor<uint16_t>() {
    return "uint16";
}

template<>
const char* get_type_descriptor<int16_t>() {
    return "int16";
}

template<>
const char* get_type_descriptor<int32_t>() {
    return "int32";
}

template<>
const char* get_type_descriptor<float>() {
    return "float";
}

template<>
const char* get_type_descriptor<uint32_t>() {
    return "uint32";
}

template<typename T>
void export_binary(const char *fname, const Buffer *buffer, int channels)
{
    int width_i = static_cast<int>(buffer->buffer_width_f);
    int height_i = static_cast<int>(buffer->buffer_height_f);

    const T* in_ptr = reinterpret_cast<T*>(buffer->buffer);

    FILE* fhandle = fopen(fname, "wb");

    if(fhandle != NULL) {
      fprintf(fhandle, "%s\n", get_type_descriptor<T>());
      fwrite(&height_i, sizeof(int), 1, fhandle);
      fwrite(&width_i, sizeof(int), 1, fhandle);
      fwrite(&channels, sizeof(int), 1, fhandle);
      fwrite(in_ptr, sizeof(T), height_i * width_i * channels, fhandle);
      fclose(fhandle);
    }
}

void BufferExporter::export_buffer(const Buffer *buffer, const std::string &path, BufferExporter::OutputType type)
{
    if(type == OutputType::Bitmap) {
        switch(buffer->type) {
        case Buffer::BufferType::UnsignedByte:
          export_bitmap<uint8_t>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::UnsignedShort:
          export_bitmap<uint16_t>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::Short:
          export_bitmap<int16_t>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::Int32:
          export_bitmap<int32_t>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::Float32:
          export_bitmap<float>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::UnsignedInt32:
          export_bitmap<uint32_t>(path.c_str(), buffer, buffer->channels);
            break;
        }
    } else {
        // Matlab/Octave matrix (load with the giw_load.m function)
        switch(buffer->type) {
        case Buffer::BufferType::UnsignedByte:
          export_binary<uint8_t>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::UnsignedShort:
          export_binary<uint16_t>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::Short:
          export_binary<int16_t>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::Int32:
          export_binary<int32_t>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::Float32:
          export_binary<float>(path.c_str(), buffer, buffer->channels);
            break;
        case Buffer::BufferType::UnsignedInt32:
          export_binary<uint32_t>(path.c_str(), buffer, buffer->channels);
            break;
        }
    }
}

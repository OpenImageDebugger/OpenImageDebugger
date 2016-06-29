#pragma once

#include "buffer.hpp"

class BufferExporter {
public:
    enum class OutputType {
        Bitmap, OctaveMatrix
    };

    static void export_buffer(const Buffer* buffer, const std::string& path, OutputType type);

private:
    /*
    static void export_ubyte_octave(const char* fname, const uint8_t* buffer, int channels);

    /*
     * Unsigned Short exporters
     * /
    static void export_ushort_bitmap(const char* fname, const uint16_t* buffer, int channels);
    static void export_ushort_octave(const char* fname, const uint16_t* buffer, int channels);

    /*
     * Short exporters
     * /
    static void export_short_bitmap(const char* fname, const int16_t* buffer, int channels);
    static void export_short_octave(const char* fname, const int16_t* buffer, int channels);

    /*
     * Int exporters
     * /
    static void export_int_bitmap(const char* fname, const int32_t* buffer, int channels);
    static void export_int_octave(const char* fname, const int32_t* buffer, int channels);

    /*
     * Float exporters
     * /
    static void export_float_bitmap(const char* fname, const float* buffer, int channels);
    static void export_float_octave(const char* fname, const float* buffer, int channels);

    /*
     * Unsigned int exporters
     * /
    static void export_uint_bitmap(const char* fname, const uint32_t* buffer, int channels);
    static void export_uint_octave(const char* fname, const uint32_t* buffer, int channels);
    */

};

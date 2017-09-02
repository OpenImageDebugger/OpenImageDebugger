#pragma once

#include "visualization/components/buffer.h"

class BufferExporter {
public:
    enum class OutputType {
        Bitmap, OctaveMatrix
    };

    static void export_buffer(const Buffer* buffer,
                              const std::string& path,
                              OutputType type);
};

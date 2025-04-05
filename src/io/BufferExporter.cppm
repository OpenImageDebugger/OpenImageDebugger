//
// Created by Bruno Rosa on 05/04/2025.
//

module;

#include <string>

#include "visualization/components/buffer.h"

export module BufferExporter;

export namespace oid::BufferExporter
{

enum class OutputType { Bitmap, OctaveMatrix };

void export_buffer(const Buffer* buffer,
                   const std::string& path,
                   const OutputType type);
} // namespace oid::BufferExporter
# The MIT License (MIT)

# Copyright (c) 2015-2026 OpenImageDebugger contributors
# (https://github.com/OpenImageDebugger/OpenImageDebugger)

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

# oid_embed_binary(SOURCE <abs path to .ttf/.svg>
#                  REL     <path under OID_GENERATED_DIR, e.g.
#                           host/ui/icons/x_svg.h>
#                  NAMESPACE <e.g. oid::host::icons>
#                  SYMBOL    <e.g. X_SVG>
#                  PROVENANCE <human-readable source description>)
#
# Reads SOURCE at configure time and writes a byte-array header to
# ${OID_GENERATED_DIR}/${REL} containing:
#   inline constexpr unsigned char ${SYMBOL}[]     = { ... };
#   inline constexpr std::size_t   ${SYMBOL}_SIZE   = sizeof(${SYMBOL});
# inside namespace ${NAMESPACE}. The array is byte-array-identical to the
# asset. copy_if_different avoids bumping the header mtime (and forcing a
# recompile) when the asset is unchanged.
function(oid_embed_binary)
  cmake_parse_arguments(A "" "SOURCE;REL;NAMESPACE;SYMBOL;PROVENANCE" "" ${ARGN})

  set(output "${OID_GENERATED_DIR}/${A_REL}")

  # Include guard from REL: uppercase, non-alnum -> '_', trailing '_'.
  string(TOUPPER "${A_REL}" guard)
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" guard "${guard}")
  set(guard "${guard}_")

  # Asset bytes as a comma-separated 0xNN list, wrapped 12 per line.
  file(READ "${A_SOURCE}" hex HEX)
  string(REGEX REPLACE "(..)" "0x\\1," bytes "${hex}")
  string(REGEX REPLACE
    "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)"
    "    \\1\n" bytes "${bytes}")

  # IMPORTANT: the string body below MUST be flush-left (column 0). CMake
  # preserves string contents verbatim, so any leading indentation here would
  # appear on every line of the generated header. Do not re-indent to match
  # the surrounding function body.
set(content
"// Generated from ${A_PROVENANCE}. Do not edit.
// clang-format off
#ifndef ${guard}
#define ${guard}
#include <cstddef>
namespace ${A_NAMESPACE} {
inline constexpr unsigned char ${A_SYMBOL}[] = {
${bytes}
};
inline constexpr std::size_t ${A_SYMBOL}_SIZE = sizeof(${A_SYMBOL});
} // namespace ${A_NAMESPACE}
#endif
// clang-format on
")

  # Write via copy_if_different so an unchanged asset does not bump the
  # header's mtime and trigger needless recompiles. COMMAND_ERROR_IS_FATAL
  # makes a failed copy (read-only tree, disk error) abort configure loudly
  # instead of surfacing later as a confusing missing/stale-header build error.
  file(WRITE "${output}.tmp" "${content}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                          "${output}.tmp" "${output}"
                  COMMAND_ERROR_IS_FATAL ANY)
  file(REMOVE "${output}.tmp")

  # Re-run configure automatically when the asset changes.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${A_SOURCE}")
endfunction()

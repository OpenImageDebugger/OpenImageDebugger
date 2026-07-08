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

# Regenerates the embedded font/SVG byte-array headers from their source
# assets at configure time. Included from the root CMakeLists.txt (after
# common.cmake) so OID_GENERATED_DIR is in scope for both src/ and tests/.
include(${CMAKE_CURRENT_LIST_DIR}/EmbedBinary.cmake)

set(OID_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(RES "${CMAKE_SOURCE_DIR}/src/resources")

# --- Fonts (namespace oid::host::fonts) ---
oid_embed_binary(SOURCE ${RES}/fonts/Roboto-Regular.ttf
                 REL host/text/fonts/roboto_regular_ttf.h
                 NAMESPACE oid::host::fonts SYMBOL ROBOTO_REGULAR
                 PROVENANCE "src/resources/fonts/Roboto-Regular.ttf (Apache-2.0)")

oid_embed_binary(SOURCE ${RES}/icons/fontello.ttf
                 REL host/text/fonts/fontello_ttf.h
                 NAMESPACE oid::host::fonts SYMBOL FONTELLO
                 PROVENANCE "src/resources/icons/fontello.ttf (license: src/resources/icons/LICENSE.txt)")

# --- Icons (namespace oid::host::icons) ---
oid_embed_binary(SOURCE ${RES}/icons/x.svg
                 REL host/ui/icons/x_svg.h
                 NAMESPACE oid::host::icons SYMBOL X_SVG
                 PROVENANCE "src/resources/icons/x.svg (license: src/resources/icons/LICENSE.txt)")

oid_embed_binary(SOURCE ${RES}/icons/y.svg
                 REL host/ui/icons/y_svg.h
                 NAMESPACE oid::host::icons SYMBOL Y_SVG
                 PROVENANCE "src/resources/icons/y.svg (license: src/resources/icons/LICENSE.txt)")

oid_embed_binary(SOURCE ${RES}/icons/label_red_channel.svg
                 REL host/ui/icons/label_red_channel_svg.h
                 NAMESPACE oid::host::icons SYMBOL LABEL_RED_CHANNEL_SVG
                 PROVENANCE "src/resources/icons/label_red_channel.svg (license: src/resources/icons/LICENSE.txt)")

oid_embed_binary(SOURCE ${RES}/icons/label_green_channel.svg
                 REL host/ui/icons/label_green_channel_svg.h
                 NAMESPACE oid::host::icons SYMBOL LABEL_GREEN_CHANNEL_SVG
                 PROVENANCE "src/resources/icons/label_green_channel.svg (license: src/resources/icons/LICENSE.txt)")

oid_embed_binary(SOURCE ${RES}/icons/label_blue_channel.svg
                 REL host/ui/icons/label_blue_channel_svg.h
                 NAMESPACE oid::host::icons SYMBOL LABEL_BLUE_CHANNEL_SVG
                 PROVENANCE "src/resources/icons/label_blue_channel.svg (license: src/resources/icons/LICENSE.txt)")

oid_embed_binary(SOURCE ${RES}/icons/label_alpha_channel.svg
                 REL host/ui/icons/label_alpha_channel_svg.h
                 NAMESPACE oid::host::icons SYMBOL LABEL_ALPHA_CHANNEL_SVG
                 PROVENANCE "src/resources/icons/label_alpha_channel.svg (license: src/resources/icons/LICENSE.txt)")

oid_embed_binary(SOURCE ${RES}/icons/lower_upper_bound.svg
                 REL host/ui/icons/lower_upper_bound_svg.h
                 NAMESPACE oid::host::icons SYMBOL LOWER_UPPER_BOUND_SVG
                 PROVENANCE "src/resources/icons/lower_upper_bound.svg (license: src/resources/icons/LICENSE.txt)")

oid_embed_binary(SOURCE ${RES}/icons/app_icon.png
                 REL host/ui/icons/app_icon_png.h
                 NAMESPACE oid::host::icons SYMBOL APP_ICON_PNG
                 PROVENANCE "src/resources/icons/app_icon.png (OpenImageDebugger application icon)")

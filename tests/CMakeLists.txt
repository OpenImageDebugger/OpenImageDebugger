# The MIT License (MIT)
#
# Copyright (c) 2015-2026 OpenImageDebugger contributors
# (https://github.com/OpenImageDebugger/OpenImageDebugger)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.28.3)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/googletest googletest SYSTEM)
foreach(_gtest_target gtest gtest_main gmock gmock_main)
    if(TARGET ${_gtest_target})
        target_compile_options(${_gtest_target} PRIVATE
            $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-w>
            $<$<CXX_COMPILER_ID:MSVC>:/w>
        )
    endif()
endforeach()

enable_testing()

# Test for linear algebra
add_executable(test_linear_algebra
    test_linear_algebra.cpp
)

target_include_directories(test_linear_algebra
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

target_include_directories(test_linear_algebra SYSTEM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/Eigen
)

target_link_libraries(test_linear_algebra
    PRIVATE
    GTest::gtest_main
    GTest::gtest
)

# Add math source file directly since it's not a separate library
target_sources(test_linear_algebra PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/math/linear_algebra.cpp
)

add_test(NAME LinearAlgebraTests COMMAND test_linear_algebra)

# Test for raw data decode
add_executable(test_raw_data_decode
    test_raw_data_decode.cpp
)

target_include_directories(test_raw_data_decode
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

target_link_libraries(test_raw_data_decode
    PRIVATE
    GTest::gtest_main
    GTest::gtest
)

# Add raw_data_decode source file directly since we need the implementation
target_sources(test_raw_data_decode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/raw_data_decode.cpp
)

add_test(NAME RawDataDecodeTests COMMAND test_raw_data_decode)

# Test for IPC message exchange
add_executable(test_message_exchange
    test_message_exchange.cpp
)

target_include_directories(test_message_exchange
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

target_include_directories(test_message_exchange SYSTEM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/asio/asio/include
)

target_compile_definitions(test_message_exchange PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
find_package(Threads REQUIRED)

target_link_libraries(test_message_exchange
    PRIVATE
    Threads::Threads
    GTest::gtest_main
    GTest::gtest
)

# Add message_exchange source file directly for testing
target_sources(test_message_exchange PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/message_exchange.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/asio_transport.cpp
)

add_test(NAME MessageExchangeTests COMMAND test_message_exchange)

# Test BufferAssembler
add_executable(test_buffer_assembler test_buffer_assembler.cpp)

target_include_directories(test_buffer_assembler
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

target_link_libraries(test_buffer_assembler
    PRIVATE
    GTest::gtest_main
    GTest::gtest
)

target_sources(test_buffer_assembler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/buffer_assembler.cpp
)

add_test(NAME BufferAssemblerTests COMMAND test_buffer_assembler)

# Test AsioTransport (standalone-Asio TCP client implementing ITransport;
# Qt-free -- Asio + gtest + Threads only).
add_executable(test_asio_transport test_asio_transport.cpp)

target_include_directories(test_asio_transport
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_include_directories(test_asio_transport SYSTEM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/asio/asio/include)

target_compile_definitions(test_asio_transport PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
find_package(Threads REQUIRED)

target_sources(test_asio_transport PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/asio_transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/message_exchange.cpp)

target_link_libraries(test_asio_transport PRIVATE Threads::Threads GTest::gtest_main GTest::gtest)

add_test(NAME AsioTransportTests COMMAND test_asio_transport)

# Test FrameLoop (pure logic, no GL/GLFW) -- only built alongside the
# opt-in ImGui frontend it belongs to.
if(OID_BUILD_IMGUI_FRONTEND)
    add_executable(frame_loop_test
        host/frame_loop_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/frame_loop.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/main_loop.cpp
    )

    target_include_directories(frame_loop_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(frame_loop_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME FrameLoopTests COMMAND frame_loop_test)

    # Test GlfwCanvas (size math + null text renderer via SizeProvider
    # injection; no live GL context needed).
    add_executable(glfw_canvas_test
        host/glfw_canvas_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/glfw_canvas.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/text/stb_glyph_atlas.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/visualization/glyph_atlas.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/visualization/gl_text_renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/visualization/shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/visualization/shaders/text_fs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/visualization/shaders/text_vs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/gl_dialect.cpp
    )

    # glfw_canvas.cpp pulls in visualization/render_canvas.h (via
    # gl_text_renderer.h), which selects GlfwCanvas over the Qt GLCanvas
    # only when OID_IMGUI_FRONTEND is defined, same as oidwindow and
    # buffer_export_core_test above.
    target_compile_definitions(glfw_canvas_test PRIVATE OID_IMGUI_FRONTEND)

    target_include_directories(glfw_canvas_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
        PRIVATE ${OID_GENERATED_DIR}
    )

    # Vendored Khronos GL/gl.h gives glfw_canvas.h a portable <GL/gl.h>
    # include across platforms (same header the Qt canvas uses); see
    # src/CMakeLists.txt's thirdparty/Khronos include for ${PROJECT_NAME}.
    # stb (SYSTEM) backs stb_glyph_atlas.cpp's font baking, same as
    # stb_glyph_atlas_test below. GlfwCanvas::render_buffer_icon() -- the one
    # piece of GlfwCanvas that touches Stage/Camera (and so needs Eigen) --
    # lives in glfw_canvas_icon.cpp instead of glfw_canvas.cpp precisely so
    # this test doesn't need to pull in Eigen/VIZ_SOURCES; see the comment
    # above GlfwCanvas::render_buffer_icon's declaration in glfw_canvas.h.
    target_include_directories(glfw_canvas_test SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/Khronos
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/stb
    )

    target_link_libraries(glfw_canvas_test
        PRIVATE
        oid_imgui
        glfw
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME GlfwCanvasTests COMMAND glfw_canvas_test)

    # Test StageView's pure size-bookkeeping helper (GL-free; the FBO/texture
    # management itself needs a live GL context and is only exercised by the
    # deferred manual smoke of oidwindow).
    add_executable(stage_view_test
        host/stage_view_test.cpp
    )

    target_include_directories(stage_view_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    # Vendored Khronos GL/gl.h, same as glfw_canvas_test above: stage_view.h
    # pulls in GL/gl.h for GLuint even though this test only exercises the
    # GL-free stage_view_needs_realloc() helper.
    target_include_directories(stage_view_test SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/Khronos
    )

    target_link_libraries(stage_view_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME StageViewTests COMMAND stage_view_test)

    # Test the GL/ImGui-free parse_int_field() helper out of text_input.cpp.
    # The input_text_line()/input_int_field() wrappers need a live ImGui
    # context (created by ImGuiLayer::initialize()) and are only exercised by
    # the deferred manual smoke of oidwindow's spike panel; this
    # target still compiles text_input.cpp, which includes <imgui.h>, so it
    # gets the same include dirs and oid_imgui/glfw linkage as
    # glfw_canvas_test above.
    add_executable(text_input_test
        host/ui/text_input_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ui/text_input.cpp
    )

    target_include_directories(text_input_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(text_input_test
        PRIVATE
        oid_imgui
        glfw
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME TextInputTests COMMAND text_input_test)

    # Test the header-only BufferModel/MockBufferModel/type_label() out of
    # host/ui/buffer_model.h. No live GL/ImGui context needed -- the mock
    # model is pure data, and its only dependency (ipc/raw_data_decode.h's
    # BufferType) is header-only too, so no extra source/link beyond gtest.
    add_executable(buffer_model_test
        host/ui/buffer_model_test.cpp
    )

    target_include_directories(buffer_model_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(buffer_model_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME BufferModelTests COMMAND buffer_model_test)

    # Test IpcBufferModel out of host/ui/ipc_buffer_model.cpp: the dynamic
    # (upsert/remove/revision) BufferModel fed by IpcClient. Pure logic --
    # no GL/Asio.
    add_executable(ipc_buffer_model_test
        host/ui/ipc_buffer_model_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ui/ipc_buffer_model.cpp
    )

    target_include_directories(ipc_buffer_model_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(ipc_buffer_model_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME IpcBufferModelTests COMMAND ipc_buffer_model_test)

    # Test filter_symbols() out of host/ui/symbol_filter.cpp. Pure STL --
    # no imgui/GL dependency.
    add_executable(symbol_filter_test
        host/ui/symbol_filter_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ui/symbol_filter.cpp
    )

    target_include_directories(symbol_filter_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(symbol_filter_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME SymbolFilterTests COMMAND symbol_filter_test)

    # Test should_fire_ctrl_shortcut() out of host/ui/shortcuts.h: a pure
    # header-only helper with no imgui/GL dependency.
    add_executable(shortcuts_test
        host/ui/shortcuts_test.cpp
    )

    target_include_directories(shortcuts_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(shortcuts_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME ShortcutsTests COMMAND shortcuts_test)

    # Test UiState (selection clamping, filtered_indices() over
    # filter_symbols(), parse_goto()) out of host/ui/ui_state.cpp. UiState
    # itself needs no live GL/ImGui context, but it depends on
    # text_input.cpp's parse_int_field(), and that TU includes <imgui.h>,
    # so this target gets the same include dirs and oid_imgui/glfw linkage
    # as text_input_test above.
    add_executable(ui_state_test
        host/ui/ui_state_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ui/ui_state.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ui/symbol_filter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ui/text_input.cpp
    )

    target_include_directories(ui_state_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(ui_state_test
        PRIVATE
        oid_imgui
        glfw
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME UiStateTests COMMAND ui_state_test)

    # Test make_buffer_record() and IpcClient out of host/ipc/: the Qt-free
    # wire-fields -> BufferRecord funnel and the client that drives it from
    # both its single-shot and chunked decode paths. Also compiles the
    # Qt-free codec/data sources they depend on (message_exchange,
    # buffer_assembler, raw_data_decode), plus ipc_buffer_model.cpp.
    add_executable(ipc_client_test
        host/ipc/ipc_client_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ipc/buffer_decode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ipc/ipc_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ui/ipc_buffer_model.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/message_exchange.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/buffer_assembler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/raw_data_decode.cpp
    )

    target_include_directories(ipc_client_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_include_directories(ipc_client_test SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/asio/asio/include
    )

    target_compile_definitions(ipc_client_test PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
    find_package(Threads REQUIRED)

    target_link_libraries(ipc_client_test
        PRIVATE
        Threads::Threads
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME IpcClientTests COMMAND ipc_client_test)

    # Test SettingsStore/AppSettings out of host/settings/settings_store.cpp:
    # the Qt-free JSON settings persistence (window geometry, UI prefs,
    # previous-buffers) ImGuiLayer loads/saves on startup/shutdown. Pure
    # logic -- no GL/Asio; the vendored nlohmann/json single_include is
    # SYSTEM for the same rationale as oidwindow's own include in
    # src/CMakeLists.txt.
    add_executable(settings_store_test
        host/settings/settings_store_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/settings/settings_store.cpp
    )

    target_include_directories(settings_store_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_include_directories(settings_store_test SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/json/single_include
    )

    target_link_libraries(settings_store_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME SettingsStoreTests COMMAND settings_store_test)

    # Test config_file_path()/config_file_path_from_env() out of
    # host/settings/config_path.cpp: the Qt-free platform config-directory
    # resolver SettingsStore (see above) is pointed at in production. Pure
    # logic -- <filesystem>/<cstdlib> only, no JSON.
    add_executable(config_path_test
        host/settings/config_path_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/settings/config_path.cpp
    )

    target_include_directories(config_path_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(config_path_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME ConfigPathTests COMMAND config_path_test)

    # Test SettingsSaver out of host/settings/settings_saver.cpp: the
    # dirty-tracking/debounced save wrapper ImGuiLayer drives once per frame
    # with the live AppSettings snapshot. Pure logic -- depends only on
    # AppSettings (header-only, JSON-free), so no json include needed.
    add_executable(settings_saver_test
        host/settings/settings_saver_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/settings/settings_saver.cpp
    )

    target_include_directories(settings_saver_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(settings_saver_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME SettingsSaverTests COMMAND settings_saver_test)

    # Test merge_previous_buffers() out of host/settings/session_buffers.cpp:
    # the pure session-buffer merge (loaded -> fresh expiry, user-deleted ->
    # dropped, not-yet-reloaded -> kept as-is) that IpcClient's auto-restore
    # hook and ImGuiLayer's shutdown persistence both build on. Pure logic --
    # depends only on PreviousBuffer (header-only, JSON-free), so no json
    # include needed.
    add_executable(session_buffers_test
        host/settings/session_buffers_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/settings/session_buffers.cpp
    )

    target_include_directories(session_buffers_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(session_buffers_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME SessionBuffersTests COMMAND session_buffers_test)

    # Test GlfwHostBackend::window_visible_on() out of
    # host/glfw_host_backend.cpp: the pure window-rect/monitor-work-area
    # overlap check main_imgui uses at startup to decide whether to restore
    # a saved window position (a fully off-screen saved position, e.g. from
    # a since-unplugged monitor, is ignored instead of placing the window
    # somewhere unreachable). Only exercises the static pure helper -- no
    # window/monitor is created -- but glfw_host_backend.cpp's other methods
    # still pull in GLFW, so this target links oid_imgui/glfw the same way
    # glfw_canvas_test above does.
    add_executable(glfw_geometry_test
        host/glfw_geometry_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/glfw_host_backend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/window_hints.cpp
    )

    target_include_directories(glfw_geometry_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(glfw_geometry_test
        PRIVATE
        oid_imgui
        glfw
        ${OPENGL_gl_LIBRARY}
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME GlfwGeometryTests COMMAND glfw_geometry_test)

    # Test buffer_export_core's Qt-free normalize/octave-export helpers out
    # of io/buffer_export_core.cpp: the shared contrast-normalized RGBA8
    # conversion and raw Octave-matrix writer both frontends' exporters
    # drive. Exercises only the pure `_raw` entry points (no GL-coupled
    # Buffer construction). The Buffer-taking adapters
    # (normalize_to_rgba8(const Buffer&), export_octave(const Buffer&, ...))
    # live in their own TU, io/buffer_export_adapters.cpp, which this test
    # deliberately doesn't compile/link (that would drag in
    # visualization/components/buffer.h's Component/RenderCanvas/GL types
    # and the full VIZ_SOURCES/GL graph), so this target only needs the same
    # OID_IMGUI_FRONTEND define (selects the GL-free GlfwCanvas over the Qt
    # GLCanvas) and Khronos/Eigen include dirs as oidwindow.
    add_executable(buffer_export_core_test
        io/buffer_export_core_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/io/buffer_export_core.cpp
    )

    target_compile_definitions(buffer_export_core_test PRIVATE OID_IMGUI_FRONTEND)

    target_include_directories(buffer_export_core_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_include_directories(buffer_export_core_test SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/Khronos
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/Eigen
    )

    target_link_libraries(buffer_export_core_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME BufferExportCoreTests COMMAND buffer_export_core_test)

    # Test the ImGui-side stb PNG exporter out of
    # host/io/imgui_buffer_exporter.cpp: export_rgba_png(), the stb_image_write
    # encode path export_buffer_imgui() uses for OutputType::Bitmap. Drives it
    # through the pure RgbaImage struct (no GL-coupled Buffer needed), then
    # decodes the written PNG back with stb_image (implementation defined in
    # this test TU only, via STB_IMAGE_IMPLEMENTATION) to check round-trip
    # bytes. Deliberately does not compile
    # host/io/imgui_buffer_exporter_glue.cpp (the Buffer-taking
    # export_buffer_imgui() wrapper) or io/buffer_export_adapters.cpp, for the
    # same reason buffer_export_core_test above skips buffer_export_adapters.cpp:
    # that would drag in visualization/components/buffer.h's Component/
    # RenderCanvas/GL types and the full VIZ_SOURCES/GL graph.
    add_executable(imgui_buffer_exporter_test
        host/io/imgui_buffer_exporter_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/io/imgui_buffer_exporter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/io/buffer_export_core.cpp
    )

    target_include_directories(imgui_buffer_exporter_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_include_directories(imgui_buffer_exporter_test SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/stb
    )

    target_link_libraries(imgui_buffer_exporter_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME ImguiBufferExporterTests COMMAND imgui_buffer_exporter_test)

    # Test finalize_strip_atlas() out of visualization/glyph_atlas.cpp: the
    # Qt-free strip-packing finisher both the legacy Qt frontend's QPainter
    # baker (see tag legacy-qt) and the ImGui-side stb baker call to turn a
    # rendered glyph strip into a pow2-packed GlyphAtlas. Pure logic -- no
    # GL/Qt/canvas types.
    add_executable(glyph_atlas_test
        visualization/glyph_atlas_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/visualization/glyph_atlas.cpp
    )

    target_include_directories(glyph_atlas_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(glyph_atlas_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME GlyphAtlasTests COMMAND glyph_atlas_test)

    # Test stb_glyph_atlas() out of host/text/stb_glyph_atlas.cpp: bakes
    # oid::GLYPH_TEXT from the embedded Roboto-Regular font (stb_truetype)
    # through the same finalize_strip_atlas() pipeline glyph_atlas_test above
    # exercises via the Qt baker. Pure CPU (no GL/Qt/canvas types); needs the
    # SYSTEM stb include (this TU defines STB_TRUETYPE_IMPLEMENTATION).
    add_executable(stb_glyph_atlas_test
        host/text/stb_glyph_atlas_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/text/stb_glyph_atlas.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/visualization/glyph_atlas.cpp
    )

    target_include_directories(stb_glyph_atlas_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
        PRIVATE ${OID_GENERATED_DIR}
    )

    target_include_directories(stb_glyph_atlas_test SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/stb
    )

    target_link_libraries(stb_glyph_atlas_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME StbGlyphAtlasTests COMMAND stb_glyph_atlas_test)

    # Test decode_png_rgba() out of host/app_icon_decode.cpp: decodes the
    # embedded application icon PNG (oid::host::icons::APP_ICON_PNG) into an
    # RGBA8 buffer. Pure CPU (no GL/GLFW); needs the SYSTEM stb include (this
    # TU defines STB_IMAGE_IMPLEMENTATION).
    add_executable(app_icon_decode_test
        host/app_icon_decode_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/app_icon_decode.cpp
    )

    target_include_directories(app_icon_decode_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
        PRIVATE ${OID_GENERATED_DIR}
    )

    target_include_directories(app_icon_decode_test SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/stb
    )

    target_link_libraries(app_icon_decode_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME AppIconDecodeTests COMMAND app_icon_decode_test)

    # Test rasterize_svg() out of host/ui/svg_raster.cpp: the nanosvg-backed
    # SVG-to-RGBA8 raster core the ImGui toolbar/label icons are baked with
    # once at startup, same role stb_glyph_atlas_test's font baking plays for
    # text. Pure CPU (no GL/Qt/canvas types); needs the SYSTEM nanosvg
    # include (this TU defines NANOSVG_IMPLEMENTATION/
    # NANOSVGRAST_IMPLEMENTATION).
    add_executable(svg_raster_test
        host/ui/svg_raster_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ui/svg_raster.cpp
    )

    target_include_directories(svg_raster_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
        PRIVATE ${OID_GENERATED_DIR}
    )

    target_include_directories(svg_raster_test SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/nanosvg/src
    )

    target_link_libraries(svg_raster_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME SvgRasterTests COMMAND svg_raster_test)

    # Test the pure helpers out of host/ui/export_dialog.cpp
    # (default_export_path, apply_format_extension, open_export_dialog).
    # Deliberately does NOT compile host/ui/export_dialog_draw.cpp (the
    # draw_export_dialog() ImGui modal, smoke-tested only) so this target
    # never needs to link ImGui/GL at all; export_dialog.h itself only pulls
    # in the header-only io/buffer_export_core.h for
    # BufferExporter::OutputType.
    add_executable(export_dialog_test
        host/ui/export_dialog_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/host/ui/export_dialog.cpp
    )

    target_include_directories(export_dialog_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(export_dialog_test
        PRIVATE
        GTest::gtest_main
        GTest::gtest
    )

    add_test(NAME ExportDialogTests COMMAND export_dialog_test)
endif()


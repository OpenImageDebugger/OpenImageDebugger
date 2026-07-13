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

cmake_minimum_required(VERSION 3.28.3)

project(oidwindow CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(OpenGL_GL_PREFERENCE LEGACY)
  find_package(OpenGL 2.1 REQUIRED)
endif()

# Shared visualization layer: renders through oid::RenderCanvas (an alias of
# oid::host::GlfwCanvas under OID_IMGUI_FRONTEND, which every surviving target
# defines) and has no UI-toolkit dependency of its own.
set(VIZ_SOURCES
    math/linear_algebra.cpp
    visualization/components/background.cpp
    visualization/components/buffer.cpp
    visualization/components/buffer_values.cpp
    visualization/components/camera.cpp
    visualization/components/component.cpp
    visualization/game_object.cpp
    visualization/gl_text_renderer.cpp
    visualization/glyph_atlas.cpp
    visualization/shader.cpp
    visualization/shaders/background_fs.cpp
    visualization/shaders/background_vs.cpp
    visualization/shaders/buffer_fs.cpp
    visualization/shaders/buffer_vs.cpp
    visualization/shaders/text_fs.cpp
    visualization/shaders/text_vs.cpp
    visualization/stage.cpp
)

# --- GLFW + Dear ImGui frontend ---
option(OID_BUILD_IMGUI_FRONTEND
       "Build the GLFW + Dear ImGui viewer frontend (OFF gives a bridge/IPC-only build)" ON)

if(OID_BUILD_IMGUI_FRONTEND)
  find_package(Threads REQUIRED)

  # GLFW: native-only. Under Emscripten, GLFW is emulated by the Emscripten
  # system library (linked via the "-s USE_GLFW=3" SHELL flag below); the
  # <GLFW/glfw3.h> header oid_imgui's backends include comes from the
  # emscripten sysroot, and there is no vendored glfw target to build or
  # link against.
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    # On Apple, GLFW's own CMakeLists enables the OBJC language itself (for
    # its Cocoa backend), but nativefiledialog-extended's nfd_cocoa.m relies
    # on OBJC already being enabled rather than enabling it itself. CMake's
    # per-directory-scope language bookkeeping does not reliably reconcile
    # one subdirectory explicitly enabling a language with a sibling
    # subdirectory implicitly depending on it via source file extension
    # alone (observed: "CMAKE_OBJC_COMPILE_OBJECT" missing during Generate).
    # Enabling OBJC here, before either subdirectory is added, avoids that.
    if(APPLE)
      enable_language(OBJC)
    endif()

    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    add_subdirectory(thirdparty/glfw EXCLUDE_FROM_ALL)

    # Vendored code is not held to this project's -Werror policy (see
    # tests/CMakeLists.txt for the same pattern applied to googletest).
    if(TARGET glfw)
      target_compile_options(glfw PRIVATE
        $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-w>
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-w>
        $<$<C_COMPILER_ID:MSVC>:/w>
        $<$<CXX_COMPILER_ID:MSVC>:/w>)
    endif()

    # nativefiledialog-extended: native-only, same as GLFW above. Skip its
    # own tests/install targets -- this project only needs the `nfd` library.
    set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(NFD_BUILD_SDL2_TESTS OFF CACHE BOOL "" FORCE)
    set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(thirdparty/nativefiledialog-extended)

    # Vendored code is not held to this project's -Werror policy (see
    # tests/CMakeLists.txt for the same pattern applied to googletest).
    if(TARGET nfd)
      target_compile_options(nfd PRIVATE
        $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-w>
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-w>
        $<$<C_COMPILER_ID:MSVC>:/w>
        $<$<CXX_COMPILER_ID:MSVC>:/w>)
    endif()
  endif()

  # Dear ImGui has no CMakeLists of its own; compile the core + backends here.
  set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui)
  add_library(oid_imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_include_directories(oid_imgui SYSTEM PUBLIC
    ${IMGUI_DIR} ${IMGUI_DIR}/backends)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_link_libraries(oid_imgui PUBLIC glfw)
  endif()
  target_compile_options(oid_imgui PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-w>
    $<$<CXX_COMPILER_ID:MSVC>:/w>)

  set(IMGUI_FRONTEND_SOURCES
    main.cpp
    host/cli_options.cpp
    host/app_icon.cpp
    host/app_icon_decode.cpp
    host/glfw_host_backend.cpp
    host/imgui_layer.cpp
    host/ui_fonts.cpp
    host/frame_loop.cpp
    host/glfw_canvas.cpp
    host/glfw_canvas_icon.cpp
    host/text/stb_glyph_atlas.cpp
    host/stage_view.cpp
    host/ui/text_input.cpp
    host/ui/symbol_filter.cpp
    host/ipc/buffer_decode.cpp
    host/ipc/ipc_client.cpp
    host/ui/ipc_buffer_model.cpp
    host/ui/ui_state.cpp
    host/ui/stage_manager.cpp
    host/ui/svg_icon_cache.cpp
    host/ui/thumbnail_cache.cpp
    host/ui/export_dialog.cpp
    host/ui/export_dialog_draw.cpp
    host/ui/panels/buffer_list_panel.cpp
    host/ui/panels/contrast_panel.cpp
    host/ui/panels/goto_panel.cpp
    host/ui/panels/menu_bar.cpp
    host/ui/panels/status_bar.cpp
    host/ui/panels/symbol_search_panel.cpp
    host/ui/panels/toolbar_panel.cpp
    host/settings/settings_store.cpp
    host/settings/config_path.cpp
    host/settings/settings_saver.cpp
    host/settings/session_buffers.cpp
    visualization/events.cpp
    io/buffer_export_core.cpp
    io/buffer_export_adapters.cpp
    host/io/imgui_buffer_exporter.cpp
    host/io/imgui_buffer_exporter_glue.cpp
    host/io/file_buffer_loader.cpp
    host/io/file_open_queue.cpp
    host/io/npy_decode.cpp
    host/ui/svg_raster.cpp
    ${VIZ_SOURCES}
  )

  # Platform-selected sources: the GL dialect probe and the IPC transport
  # construction differ between the native (GLFW window, asio transport)
  # and Emscripten (canvas-driven GL, postMessage transport) builds.
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    list(APPEND IMGUI_FRONTEND_SOURCES
      platform/gl_dialect.cpp
      platform/transport_factory.cpp
      platform/main_loop.cpp
      platform/window_hints.cpp
      platform/display_env.cpp
      platform/app_services.cpp
      platform/native_file_dialog.cpp
    )
  endif()

  add_executable(oidwindow ${IMGUI_FRONTEND_SOURCES})

  # Embeds the application icon into the Windows executable; contributes no
  # sources on other platforms.
  target_sources(oidwindow PRIVATE
    $<$<BOOL:${WIN32}>:${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app_icon.rc>)

  # On macOS the runtime icon is set on the Dock tile via AppKit (there is no
  # per-window title-bar icon); the Objective-C++ helper needs ARC and the
  # AppKit/Foundation frameworks.
  if(APPLE)
    target_sources(oidwindow PRIVATE host/app_icon_mac.mm)
    set_source_files_properties(host/app_icon_mac.mm
      PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(oidwindow PRIVATE "-framework AppKit"
                                            "-framework Foundation")
  endif()

  target_compile_definitions(oidwindow PRIVATE OID_IMGUI_FRONTEND)
  target_include_directories(oidwindow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${OID_GENERATED_DIR})

  # INSTALL_RPATH lets the installed binary locate the shared liboidipc at
  # runtime (it lives in ${CMAKE_INSTALL_PREFIX}/lib, but the executable
  # installs to OpenImageDebugger/).
  set_target_properties(oidwindow PROPERTIES
                        INSTALL_RPATH
                        "${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_SOURCE_DIR}/lib/ipc")

  # Vendored Khronos GL/gl.h gives the shared viz layer (and GlfwCanvas) a
  # portable <GL/gl.h> include across platforms. Eigen is the linear-algebra
  # header the viz layer's math/linear_algebra.h wraps.
  target_include_directories(oidwindow SYSTEM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Khronos
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Eigen)

  # nlohmann/json (header-only, single_include) is native-only settings
  # persistence for oidwindow; it must not leak into any non-native
  # translation unit, so it is scoped to this target alone.
  target_include_directories(oidwindow SYSTEM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/json/single_include)

  # stb single-header libraries (stb_image_write, stb_truetype) back the
  # Qt-free export/text-overlay utilities for oidwindow; scoped to this
  # target alone, same as the nlohmann/json include above.
  target_include_directories(oidwindow SYSTEM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb)

  # nanosvg (single-header) backs svg_raster.cpp's SVG-to-RGBA8 raster core
  # for the Qt-free toolbar/label icons; scoped to this target alone, same
  # as the stb include above.
  target_include_directories(oidwindow SYSTEM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/nanosvg/src)

  target_link_libraries(oidwindow PRIVATE
    oid_imgui
    oidipc)

  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    # The out-of-tree platform implementation (sources + link flags) is
    # injected by the caller: -DOID_PLATFORM_DIR=<dir> containing
    # platform-link.cmake. Included after add_executable so the fragment
    # can target_sources()/target_link_options() the existing target.
    if(NOT OID_PLATFORM_DIR)
      message(FATAL_ERROR
        "Emscripten builds require -DOID_PLATFORM_DIR=<dir> providing the "
        "platform sources and platform-link.cmake (the non-native port is "
        "maintained in a separate repository).")
    endif()
    include(${OID_PLATFORM_DIR}/platform-link.cmake)
  else()
    target_link_libraries(oidwindow PRIVATE
      glfw
      nfd
      ${OPENGL_gl_LIBRARY}
      Threads::Threads)
  endif()
  add_dependencies(oidwindow oidipc)

  install(TARGETS oidwindow RUNTIME DESTINATION OpenImageDebugger)
endif()

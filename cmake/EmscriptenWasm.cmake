if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  return()
endif()

if(NOT DEFINED ENV{EMSDK})
  message(FATAL_ERROR "EMSDK not set. Run: source emsdk_env.sh")
endif()

if(NOT Qt6_DIR)
  message(FATAL_ERROR "Set -DQt6_DIR=/path/to/qt6-wasm/lib/cmake/Qt6")
endif()

add_compile_definitions(OID_TRANSPORT_POSTMESSAGE)

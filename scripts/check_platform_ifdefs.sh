#!/usr/bin/env bash
set -euo pipefail

cpp_violations=$(grep -rn "__EMSCRIPTEN__" src --include=*.cpp \
  | grep -v "src/platform/" | grep -v "src/thirdparty/" || true)
h_violations=$(grep -rn "__EMSCRIPTEN__" src --include=*.h \
  | grep -v "src/platform/" | grep -v "src/thirdparty/" \
  | grep -v "src/ui/gl_canvas.h" || true)

status=0
if [[ -n "$cpp_violations" ]]; then
  echo "Platform ifdefs leaked outside src/platform/ (.cpp):" >&2
  echo "$cpp_violations" >&2
  status=1
fi
if [[ -n "$h_violations" ]]; then
  echo "Platform ifdefs leaked outside src/platform/ and src/ui/gl_canvas.h (.h):" >&2
  echo "$h_violations" >&2
  status=1
fi

if [[ $status -eq 0 ]]; then
  echo "OK: all __EMSCRIPTEN__ ifdefs are confined to src/platform/ (and src/ui/gl_canvas.h)"
fi
exit $status

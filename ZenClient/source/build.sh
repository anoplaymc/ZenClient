#!/usr/bin/env bash
# Builds ZenClient.dll (into ../) and ZenClient.exe (into the game folder) with MinGW-w64.
# On NixOS:  nix-shell -p pkgsCross.mingwW64.buildPackages.gcc --run ./build.sh
set -euo pipefail
cd "$(dirname "$0")"
CXX=${CXX:-x86_64-w64-mingw32-g++}
CC=${CC:-x86_64-w64-mingw32-gcc}
OUT=build
mkdir -p "$OUT/obj"
IMGUI=third_party/imgui
IMCFG="-DIMGUI_USER_CONFIG=\"zen_imconfig.h\" -Isrc"
MH=third_party/minhook
CXXFLAGS="-O2 -g -std=c++20 -DNDEBUG -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -I$IMGUI -I$IMGUI/backends -I$MH/include $IMCFG -Wall -Wno-unused-function -Wno-missing-field-initializers"

objs=()
compile() {  # src obj compiler flags
  local src=$1 obj=$2
  if [[ ! -f $obj || $src -nt $obj || ${FORCE:-0} == 1 ]]; then
    echo "  CC $src"
    "${@:3}" -c "$src" -o "$obj"
  fi
  objs+=("$obj")
}
for f in imgui imgui_draw imgui_tables imgui_widgets; do
  compile "$IMGUI/$f.cpp" "$OUT/obj/$f.o" $CXX -O2 -std=c++20 -I$IMGUI $IMCFG
done
compile "$IMGUI/backends/imgui_impl_win32.cpp" "$OUT/obj/imgui_impl_win32.o" $CXX -O2 -std=c++20 -I$IMGUI -DUNICODE $IMCFG
compile "$IMGUI/backends/imgui_impl_dx11.cpp" "$OUT/obj/imgui_impl_dx11.o" $CXX -O2 -std=c++20 -I$IMGUI $IMCFG
for f in buffer hook trampoline hde/hde64; do
  compile "$MH/src/$f.c" "$OUT/obj/mh_$(basename $f).o" $CC -O2 -I$MH/include
done
FORCE=1
for f in src/*.cpp; do
  compile "$f" "$OUT/obj/$(basename "${f%.cpp}").o" $CXX $CXXFLAGS
done

echo "  LD ZenClient.dll"
$CXX -shared -static -s -Wl,-Map=build/zen.map -o ../ZenClient.dll "${objs[@]}" \
  -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -limm32 -lgdi32 -lcomdlg32 -lshell32 -luser32 -lkernel32 -Wl,--subsystem,windows

$CXX -shared -static -o build/ZenClient_dbg.dll "${objs[@]}" \
  -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -limm32 -lgdi32 -lcomdlg32 -lshell32 -luser32 -lkernel32 -Wl,--subsystem,windows
echo "  LD ZenClient.exe"
IMOBJS=("$OUT/obj/imgui.o" "$OUT/obj/imgui_draw.o" "$OUT/obj/imgui_tables.o" "$OUT/obj/imgui_widgets.o" "$OUT/obj/imgui_impl_win32.o" "$OUT/obj/imgui_impl_dx11.o")
$CXX -O2 -std=c++20 -municode -mwindows -static -s -DUNICODE -D_UNICODE -DNOMINMAX -I$IMGUI -I$IMGUI/backends $IMCFG \
  launcher/launcher.cpp "${IMOBJS[@]}" -o ../../ZenClient.exe -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -limm32 -lgdi32 -lshell32

echo "OK"
ls -la ../ZenClient.dll ../../ZenClient.exe

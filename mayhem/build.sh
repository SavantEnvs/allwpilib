#!/usr/bin/env bash
#
# mayhem/build.sh — builds, inside the commit image (as `mayhem` in /mayhem):
#   1. wpiutil/datalog/wpinet/ntcore (upstream's own CMake) instrumented with $SANITIZER_FLAGS,
#      libFuzzer coverage (-fsanitize=fuzzer-no-link) and DWARF 3, into build-fuzz/
#   2. each mayhem/fuzz_*.cpp harness twice: libFuzzer binary + `-standalone` repro binary
#   3. upstream's Catch2 suites for those libraries with NORMAL flags into build-tests/ (run by test.sh)
# Everything is vendored upstream (Catch2, libuv, ada, llhttp, ...): no network needed.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

: "${SRC:=/mayhem}"
cd "$SRC"

# Start from clean build trees: CMake caches the compiler it was first configured with, and the
# cache-prefill image build and a later plain re-run resolve `clang` differently, which makes CMake
# discard its cache mid-configure and drop the -D option set (CSCORE/OpenCV turns back on).
rm -rf build-tests build-fuzz

CMAKE_COMMON=(-G Ninja -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX"
  -DBUILD_SHARED_LIBS=OFF -DWPILIB_WITH_CSCORE=OFF -DWPILIB_WITH_WPIMATH=OFF
  -DWPILIB_WITH_WPILIB=OFF -DWPILIB_WITH_BENCHMARK=OFF -DWPILIB_WITH_GUI=OFF
  -DWPILIB_WITH_SIMULATION_MODULES=OFF)

# 3) test suites, normal flags, independent tree (built first: fuzz flags must not leak into it)
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake -S . -B build-tests "${CMAKE_COMMON[@]}" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWPILIB_WITH_TESTS=ON -DWPILIB_WITH_EXECUTABLES=ON \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --build build-tests -j"$MAYHEM_JOBS" \
  --target datalog_test wpiutil_test wpinet_test ntcore_test

# 1) sanitized + coverage-instrumented libraries
FUZZ_FLAGS="-O1 $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake -S . -B build-fuzz "${CMAKE_COMMON[@]}" \
  -DCMAKE_BUILD_TYPE= -DWPILIB_NO_WERROR=ON -DWPILIB_WITH_TESTS=OFF -DWPILIB_WITH_EXECUTABLES=OFF \
  -DCMAKE_C_FLAGS="$FUZZ_FLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_FLAGS"
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --build build-fuzz -j"$MAYHEM_JOBS" \
  --target wpiutil datalog wpinet ntcore

# 2) harnesses
N=wpiutil/src/main/native
INC=(-I$N/include
  $(for d in argparse debugging double-conversion htrie json llvm mpack nanopb sigslot upb; do echo -I$N/thirdparty/$d/include; done)
  -Idatalog/src/main/native/include
  -Iwpinet/src/main/native/include
  $(for d in ada llhttp tcpsockets libuv; do echo -Iwpinet/src/main/native/thirdparty/$d/include; done)
  -Intcore/src/main/native/include -Intcore/src/main/native/cpp
  -Intcore/src/generated/main/native/include)
LIBS=(build-fuzz/lib/libntcore.a build-fuzz/lib/libdatalog.a build-fuzz/lib/libwpinet.a
  build-fuzz/lib/libwpiutil.a -lpthread -ldl -latomic)
CXXFLAGS_H=(-std=c++23 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS "${INC[@]}")

$CXX "${CXXFLAGS_H[@]}" -c mayhem/lsan_off.cc -o build-fuzz/lsan_off.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build-fuzz/standalone_main.o

for t in fuzz_datalog_reader fuzz_nt_wire fuzz_json fuzz_struct_schema fuzz_protobuf_db fuzz_http; do
  $CXX "${CXXFLAGS_H[@]}" -fsanitize=fuzzer-no-link -c "mayhem/$t.cpp" -o "build-fuzz/$t.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "build-fuzz/$t.o" build-fuzz/lsan_off.o \
    "${LIBS[@]}" -o "$SRC/$t"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "build-fuzz/$t.o" build-fuzz/standalone_main.o build-fuzz/lsan_off.o \
    "${LIBS[@]}" -o "$SRC/$t-standalone"
done

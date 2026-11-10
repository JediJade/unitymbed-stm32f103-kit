#!/bin/sh
# Build every template and check that each image can actually boot.
#
#   sh tools/build-all.sh            # from the kit root, in Git Bash or Linux
#
# Uses arm-none-eabi-gcc and make from PATH; on Windows, falls back to the
# toolchain UnityMbed installs under %LOCALAPPDATA%\UnityMbed\tools.
#
# "Builds" is not enough for an STM32 image: if the vector table is dropped
# by --gc-sections the build still succeeds and flashing still verifies, and
# the chip dies before main(). So for each image this also checks that word 0
# of the .bin (the initial stack pointer) is a RAM address and word 1 (the
# reset vector) is an odd, i.e. Thumb, flash address.

set -u

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1 || ! command -v make >/dev/null 2>&1; then
  if [ -n "${LOCALAPPDATA:-}" ]; then
    U=$(cygpath -u "$LOCALAPPDATA" 2>/dev/null || echo "$LOCALAPPDATA")/UnityMbed/tools
    if [ -d "$U" ]; then
      PATH="$U/arm-none-eabi/bin:$U/make/xpack-windows-build-tools-4.4.1-3/bin:$PATH"
      export PATH
      echo "using the UnityMbed toolchain in $U"
    fi
  fi
fi

for tool in arm-none-eabi-gcc arm-none-eabi-nm make od; do
  command -v "$tool" >/dev/null 2>&1 || { echo "missing: $tool"; exit 2; }
done
arm-none-eabi-gcc --version | head -1

root=$(cd "$(dirname "$0")/.." && pwd)
fail=0

for dir in "$root"/templates/*/; do
  name=$(basename "$dir")
  log="$dir/build/build-all.log"
  mkdir -p "$dir/build"

  if ! (cd "$dir" && make -B PROJECT="$name" >"$log" 2>&1); then
    echo "FAIL  $name: build failed, see templates/$name/build/build-all.log"
    tail -5 "$log"
    fail=1
    continue
  fi

  bin="$dir/build/$name.bin"
  sp=$(od -A n -t x4 -N 4 "$bin" | tr -d ' ')
  rv=$(od -A n -t x4 -j 4 -N 4 "$bin" | tr -d ' ')
  warnings=$(grep -c -i 'warning:' "$log")
  size=$(wc -c <"$bin" | tr -d ' ')

  case "$sp" in
    2000*) ;;
    *) echo "FAIL  $name: word 0 is 0x$sp, not a RAM address - vector table missing?"; fail=1; continue ;;
  esac
  case "$rv" in
    0800*[13579bdf]) ;;
    *) echo "FAIL  $name: reset vector 0x$rv is not an odd flash address"; fail=1; continue ;;
  esac

  printf "ok    %-14s %6s bytes  sp=0x%s reset=0x%s  warnings=%s\n" "$name" "$size" "$sp" "$rv" "$warnings"
done

exit $fail

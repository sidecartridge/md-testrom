#!/bin/bash
#
# Build the test image the RP serves in the cartridge window, and convert it
# into rp/src/include/target_firmware.h for the RP build.
#
# The image is TESTROM.BIN: 128 KB, ROM4 ($FA0000) in its first 64 KB and ROM3
# ($FB0000) in the second. There is no m68k code: the ST boots as if no
# cartridge were there, so the image must never start with a cartridge magic.
#
# Usage: target/atarist/build.sh   (run from target/atarist)
set -Eeo pipefail
trap 'echo "ERROR: ${BASH_SOURCE[0]}: failed at line ${LINENO}" >&2' ERR

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

IMAGE=TESTROM.BIN
WINDOW_BYTES=$((128 * 1024))
TARGET_FIRMWARE=target_firmware.h

size=$(wc -c < "$IMAGE" | tr -d ' ')
if [ "$size" -ne "$WINDOW_BYTES" ]; then
    echo "ERROR: $IMAGE is $size bytes; the window is $WINDOW_BYTES" >&2
    exit 1
fi

# TOS runs a cartridge whose first long is $ABCDEF42 (an application
# cartridge) or $FA52235F (a diagnostic cartridge, run before the OS): the
# test image must be neither, or the ST would try to run the test data.
magic=$(od -A n -t x1 -N 4 "$IMAGE" | tr -d ' \n')
case "$magic" in
    abcdef42|fa52235f)
        echo "ERROR: $IMAGE starts with a cartridge magic ($magic)" >&2
        exit 1
        ;;
esac

mkdir -p dist
cp "$IMAGE" dist/FIRMWARE.IMG

echo "Creating $TARGET_FIRMWARE"
python3 firmware.py --input=dist/FIRMWARE.IMG --output="$TARGET_FIRMWARE" \
    --array_name=target_firmware

mv "$TARGET_FIRMWARE" "../../rp/src/include/$TARGET_FIRMWARE"
echo "Wrote rp/src/include/$TARGET_FIRMWARE"

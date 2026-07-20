"""
KITEN firmware build patch script.

This script patches the ESP32 SDK's libnet80211.a library to weaken the
`ieee80211_raw_frame_sanity_check` symbol, allowing our override in
wifi_atks.cpp to take precedence. Without this patch, the ESP-IDF's
internal sanity check refuses to transmit raw 802.11 frames (deauth,
beacon), so WiFi attacks silently fail.

This is the same approach KITEN firmware uses (see KITEN's patch.py),
adapted for KITEN.

The patch is applied once per framework install — a `.patched` flag file
is created in the framework lib directory so subsequent builds skip the
patch step.
"""
from os import makedirs, remove, rename
from os.path import basename, dirname, exists, isfile, join

Import("env")  # type: ignore

# Find the Arduino-ESP32 framework package — the exact name varies by
# PlatformIO version, so we try both `framework-arduinoespressif32` and
# `framework-arduinoespressif32-libs`.
pio_platform = env.PioPlatform()
fw_pkg = None
for name in ("framework-arduinoespressif32", "framework-arduinoespressif32-libs"):
    try:
        p = pio_platform.get_package_dir(name)
        if p and isfile(join(p, "tools", "sdk", "esp32s3", "lib", "libnet80211.a")):
            fw_pkg = p
            break
    except Exception:
        continue

if fw_pkg is None:
    print("[KITEN patch] WARNING: Could not locate framework-arduinoespressif32 — skipping patch")
    print("[KITEN patch] WiFi attacks will not work without this patch.")
else:
    board_mcu = env.BoardConfig()
    mcu = board_mcu.get("build.mcu", "")
    lib_dir = join(fw_pkg, "tools", "sdk", mcu, "lib")
    patchflag_path = join(lib_dir, ".patched")

    if not isfile(patchflag_path):
        original_file = join(lib_dir, "libnet80211.a")
        patched_file = join(lib_dir, "libnet80211.a.patched")

        print(f"[KITEN patch] Weakening ieee80211_raw_frame_sanity_check in {original_file}")

        if mcu in ("esp32c5", "esp32c6"):
            env.Execute(
                "pio pkg exec -p toolchain-riscv32-esp -- riscv32-esp-elf-objcopy "
                "--weaken-symbol=ieee80211_raw_frame_sanity_check %s %s"
                % (original_file, patched_file)
            )
        elif mcu == "esp32p4":
            # ESP32-P4 doesn't have WiFi — skip
            pass
        else:
            # ESP32, ESP32-S2, ESP32-S3, ESP32-C3 etc.
            env.Execute(
                "pio pkg exec -p toolchain-xtensa-%s -- xtensa-%s-elf-objcopy "
                "--weaken-symbol=ieee80211_raw_frame_sanity_check %s %s"
                % (mcu, mcu, original_file, patched_file)
            )

        if isfile("%s.old" % (original_file)):
            remove("%s.old" % (original_file))

        if isfile(original_file):
            rename(original_file, "%s.old" % (original_file))
        else:
            print("[KITEN patch] Original libnet80211.a not found — cannot patch")

        if isfile(patched_file):
            rename(patched_file, original_file)
        else:
            print("[KITEN patch] Patched file not found — patch may have failed")


        def _touch(path):
            with open(path, "w") as fp:
                fp.write("")

        env.Execute(lambda *args, **kwargs: _touch(patchflag_path))
        print("[KITEN patch] Done — ieee80211_raw_frame_sanity_check is now weak")
    else:
        print("[KITEN patch] Already patched, skipping")

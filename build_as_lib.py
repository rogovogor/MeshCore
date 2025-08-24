from os.path import join, realpath, exists
import shutil, os

Import("env")

platform_stm32=False
platform_esp32=False
platform_nrf52=False
platform_rp2040=False

src_filter = []
src_filter.append("+<*>")
src_filter.append("-<helpers/ui/*>") # don't build with ui for now ...

for item in env.get("CPPDEFINES", []):
    if isinstance(item, str) and item == "STM32_PLATFORM":
        # add STM32 specific sources
        env.Append(CPPPATH=[realpath("src/helpers/stm32")])
        platform_stm32=True
        env.Append(BUILD_FLAGS=["-I src/helpers/stm32"])
    elif isinstance(item, str) and item == "ESP32":
        platform_esp32=True
        env.Append(CPPPATH=[realpath("src/helpers/esp32")])
        env.Append(BUILD_FLAGS=["-I src/helpers/esp32"])
    elif isinstance(item, str) and item == "WIO_E5_DEV_VARIANT":
        env.Append(BUILD_FLAGS=["-I variants/wio-e5-dev"])
        src_filter.append("+<../variants/wio-e5-dev>")
    elif isinstance(item, str) and item == "RAK_3X72_VARIANT":
        env.Append(BUILD_FLAGS=["-I variants/rak3x72"])
        src_filter.append("+<../variants/rak3x72>")
    elif isinstance(item, str) and item == "XIAO_S3_WIO_VARIANT":
        env.Append(BUILD_FLAGS=["-I variants/xiao_s3_wio"])
        src_filter.append("+<../variants/xiao_s3_wio>")
    elif isinstance(item, str) and item == "XIAO_C6_VARIANT":
        env.Append(BUILD_FLAGS=["-I variants/xiao_c6"])
        src_filter.append("+<../variants/xiao_c6>")
    elif isinstance(item, str) and item == "GENERIC_ESPNOW_VARIANT":
        env.Append(BUILD_FLAGS=["-I variants/generic_espnow"])
        src_filter.append("+<../variants/generic_espnow>")
        src_filter.append("+<helpers/esp32/ESPNOWRadio.cpp>")

if not platform_stm32:
    src_filter.append("-<helpers/stm32/*>")
if not platform_esp32:
    src_filter.append("-<helpers/esp32/*>")
if not platform_nrf52:
    src_filter.append("-<helpers/nrf52/*>")
if not platform_rp2040:
    src_filter.append("-<helpers/rp2040/*>")

env.Replace(SRC_FILTER=src_filter)

print (env.Dump())

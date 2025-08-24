from os.path import realpath

Import("env") # type: ignore
menv=env # type: ignore

src_filter = [
  '+<*.cpp>',
  '+<helpers/*.cpp>',
  '+<helpers/radiolib/*.cpp>',
  '+<helpers/ui/MomentaryButton.cpp>',
]

for item in menv.get("CPPDEFINES", []):
    if isinstance(item, str) and item == "STM32_PLATFORM":
        # add STM32 specific sources
        menv.Append(CPPPATH=[realpath("src/helpers/stm32")])
        menv.Append(BUILD_FLAGS=["-I src/helpers/stm32"])
        src_filter.append("+<helpers/stm32/*>")
    elif isinstance(item, str) and item == "ESP32":
        menv.Append(CPPPATH=[realpath("src/helpers/esp32")])
        menv.Append(BUILD_FLAGS=["-I src/helpers/esp32"])
        src_filter.append("+<helpers/esp32/*>")
    elif isinstance(item, tuple) and item[0] == "MC_VARIANT":
        variant_name = item[1]
        menv.Append(BUILD_FLAGS=[f"-I variants/{variant_name}"])
        src_filter.append(f"+<../variants/{variant_name}>")

menv.Replace(SRC_FILTER=src_filter)

#print (menv.Dump())

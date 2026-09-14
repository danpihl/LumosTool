# Lumos - Simple STM32 Build Tool

A straightforward command-line tool for building STM32 projects using the ARM GCC toolchain.

## Overview

This is a simplified version of Lumos focused on one core task: building STM32 firmware from source files.

## Features

- ✅ Simple `project.yaml` configuration
- ✅ Automatic compilation of C/C++ source files
- ✅ ARM GCC toolchain integration
- ✅ STM32F4 support (F4/G0/G4/H7 platforms available)
- ✅ Generates `.elf` and `.bin` firmware files
- ✅ Memory map generation

## Building Lumos

```bash
mkdir build && cd build
cmake ..
make lumos_simple
```

The executable will be at: `build/src/applications/lumos_simple/lumos_simple`

## Project Structure

A Lumos project needs:

```
my_project/
├── project.yaml        # Project configuration
├── main.cpp            # Your source files
├── source_file.cpp
├── source_file.h
└── build/              # Build output (created automatically)
```

### project.yaml Format

```yaml
sources:
  - main.cpp
  - source_file.cpp
board: LumosBrain
```

**Supported Boards:**
- `LumosBrain` - STM32F407VG (Cortex-M4, 168MHz, 1MB Flash, 192KB RAM)

## Usage

### Build a Project

```bash
cd my_project
lumos build
```

The tool will:
1. Parse `project.yaml`
2. Compile all source files
3. Compile STM32 startup and system files
4. Link everything together
5. Generate `firmware.elf` and `firmware.bin`

### Output Files

After a successful build:

```
build/
├── firmware.elf        # ELF executable with debug symbols
├── firmware.bin        # Raw binary for flashing
├── firmware.map        # Memory map file
├── main.o              # Object files
├── source_file.o
├── startup.o
└── system_stm32f4xx.o
```

## Example Build Output

```
=== Lumos Builder ===
Project directory: /path/to/my_project

Board: LumosBrain
Sources: 2 files

Platform: f4
MCU: STM32F407xx
CPU: cortex-m4

Compiling user sources...
  main.cpp -> main.o
  source_file.cpp -> source_file.o

Compiling system files...
  startup_stm32f407xx.s -> startup.o
  system_stm32f4xx.c -> system_stm32f4xx.o

Linking...

Creating binary...

Build complete!
Output files:
  /path/to/my_project/build/firmware.elf
  /path/to/my_project/build/firmware.bin
  Binary size: 161472 bytes
```

## Compiler Configuration

The tool automatically configures:

**Compiler Flags:**
- `-mcpu=cortex-m4` - Target Cortex-M4 core
- `-mthumb` - Use Thumb instruction set
- `-mfloat-abi=soft` - Software floating point
- `-O2` - Optimization level 2
- `-Wall` - Enable all warnings
- `-ffunction-sections` / `-fdata-sections` - Dead code elimination
- `-fno-exceptions` / `-fno-rtti` - No C++ exceptions/RTTI

**Defines:**
- `-DSTM32F407xx` - Target MCU
- `-DUSE_HAL_DRIVER` - Enable STM32 HAL

**Include Paths:**
- Platform configuration
- CMSIS Device headers
- CMSIS Core headers
- STM32 HAL Driver headers

**Linker Flags:**
- `-T<linker_script>` - Memory layout
- `-Wl,--gc-sections` - Remove unused sections
- `-specs=nano.specs` - Use newlib-nano (smaller C library)
- `-specs=nosys.specs` - No system calls (bare metal)

## Platform Files

The tool uses platform files from `src/toolchains/platform/`:

```
platform/
├── f4/     # STM32F4 series
│   └── lumos_config/
│       ├── STM32F407VG_FLASH.ld       # Linker script
│       ├── startup_stm32f407xx.s      # Startup code
│       ├── system_stm32f4xx.c         # System init
│       └── board_config.h             # Board config
├── g0/     # STM32G0 series
├── g4/     # STM32G4 series
└── h7/     # STM32H7 series
```

## Toolchain

Uses ARM GCC toolchain located at:
```
src/toolchains/macos/gcc-arm-none-eabi-10.3-2021.10/
```

Compiler binaries:
- `arm-none-eabi-gcc` - C compiler
- `arm-none-eabi-g++` - C++ compiler
- `arm-none-eabi-objcopy` - Create binary files
- `arm-none-eabi-size` - Check firmware size

## Flashing to Hardware

After building, flash with ST-Link:

```bash
st-flash write build/firmware.bin 0x8000000
```

Or with OpenOCD:

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/firmware.elf verify reset exit"
```

## Example Project

See `example_project/` for a complete working example:

```cpp
// main.cpp
#include <iostream>

int main(int argc, char *argv[])
{
    std::cout << "Hello, Lumos Tool!" << std::endl;
    return 0;
}
```

Build it:
```bash
cd example_project
../build/src/applications/lumos_simple/lumos_simple build
```

## Memory Layout (STM32F407VG)

```
Flash (ROM):    0x08000000 - 0x080FFFFF  (1024 KB)
RAM (SRAM):     0x20000000 - 0x2001FFFF  (128 KB)
CCM RAM:        0x10000000 - 0x1000FFFF  (64 KB)
```

## Checking Firmware Size

```bash
arm-none-eabi-size build/firmware.elf
```

Output:
```
   text    data     bss     dec     hex filename
 160712     748    7328  168788   29354 firmware.elf
```

- **text**: Code (Flash)
- **data**: Initialized data (Flash + RAM)
- **bss**: Uninitialized data (RAM only)

## Troubleshooting

### "project.yaml not found"
Make sure you're running `lumos build` from the project directory containing `project.yaml`.

### Compilation Errors
Check that your source files are valid C/C++ and compatible with the ARM embedded environment (no standard library functions that require OS support).

### Linker Errors
- **"section `.text' will not fit in region `FLASH`"** - Code is too large, enable more optimization or reduce code size
- **"region `RAM' overflowed"** - Too many variables, reduce heap/stack size

### Unknown Board
If your board isn't recognized, edit `src/applications/lumos_simple/project_config.cpp` and add your board configuration.

## Adding New Boards

Edit `src/applications/lumos_simple/project_config.cpp`:

```cpp
BoardConfig BoardConfig::GetConfig(const std::string& board_name) {
    BoardConfig config;
    config.name = board_name;

    if (board_name == "MyCustomBoard") {
        config.platform = "f4";              // or g0, g4, h7
        config.mcu = "STM32F407xx";         // MCU variant
        config.cpu = "cortex-m4";            // CPU core
        config.float_abi = "soft";           // or "hard"
        config.fpu = "fpv4-sp-d16";         // if using hard float
    }
    // ...
}
```

## Future Development

This is the simple, focused version. Future capabilities being designed include:
- Multiple application management
- Inter-application communication
- Distributed system support
- IDL-based interfaces
- Advanced configuration

See `ARCHITECTURE.md` for the full vision.

## License

See repository license file.

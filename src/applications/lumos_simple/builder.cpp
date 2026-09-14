#include "builder.h"
#include <iostream>
#include <filesystem>
#include <sstream>
#include <cstdlib>
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace fs = std::filesystem;

namespace Lumos {

Builder::Builder(const std::string& lumos_root)
    : lumos_root_(lumos_root)
{
}

// Helper to get the correct base path for resources
// Development: lumos_root/src/
// Release: lumos_root/
std::string Builder::GetResourceBasePath() const {
    // Check if we have development structure (with src/)
    fs::path dev_path = fs::path(lumos_root_) / "src" / "toolchains";
    if (fs::exists(dev_path)) {
        return lumos_root_ + "/src";
    }
    // Otherwise assume release structure (no src/)
    return lumos_root_;
}

std::string Builder::GetToolchainPath() const {
#if defined(__APPLE__)
    return GetResourceBasePath() + "/toolchains/macos/gcc-arm-none-eabi-10.3-2021.10/bin";
#elif defined(_WIN32)
    return GetResourceBasePath() + "/toolchains/windows/gcc-arm-none-eabi-10.3-2021.10/bin";
#else
    return GetResourceBasePath() + "/toolchains/linux/gcc-arm-none-eabi-10.3-2021.10/bin";
#endif
}

std::string Builder::GetPlatformPath(const std::string& platform) const {
    return GetResourceBasePath() + "/toolchains/platform/" + platform;
}

std::string Builder::GetBoardPath(const std::string& board_name) const {
    // Convert board name to lowercase with underscores (e.g., LumosBrain -> lumos_brain)
    std::string board_dir;
    for (size_t i = 0; i < board_name.length(); ++i) {
        char c = board_name[i];
        // Insert underscore before uppercase letters (except first character)
        if (i > 0 && std::isupper(c) && std::islower(board_name[i-1])) {
            board_dir += '_';
        }
        board_dir += std::tolower(c);
    }
    return GetResourceBasePath() + "/boards/" + board_dir;
}

std::vector<std::string> Builder::GetIncludePaths(const BoardConfig& board, const std::string& project_dir) const {
    std::string platform_path = GetPlatformPath(board.platform);

    std::vector<std::string> includes;

    // Add project include directory if it exists
    std::string project_include = project_dir + "/include";
    if (fs::exists(project_include)) {
        includes.push_back(project_include);
    }

    // Add board-specific directory for board headers
    std::string board_path = GetBoardPath(board.name);
    if (fs::exists(board_path)) {
        includes.push_back(board_path);
    }

    // Add wrapper directory for generic peripheral abstractions
    std::string wrapper_path = GetResourceBasePath() + "/wrapper";
    if (fs::exists(wrapper_path)) {
        includes.push_back(wrapper_path);
    }

    includes.push_back(platform_path + "/Drivers/CMSIS/Include");

    // Add platform-specific CMSIS device include
    if (board.platform == "f4") {
        includes.push_back(platform_path + "/Drivers/CMSIS/Device/ST/STM32F4xx/Include");
        includes.push_back(platform_path + "/Drivers/STM32F4xx_HAL_Driver/Inc");
    } else if (board.platform == "h7") {
        includes.push_back(platform_path + "/Drivers/CMSIS/Device/ST/STM32H7xx/Include");
        includes.push_back(platform_path + "/Drivers/STM32H7xx_HAL_Driver/Inc");
    } else if (board.platform == "h5") {
        includes.push_back(platform_path + "/Drivers/CMSIS/Device/ST/STM32H5xx/Include");
        includes.push_back(platform_path + "/Drivers/STM32H5xx_HAL_Driver/Inc");
    } else if (board.platform == "g0") {
        includes.push_back(platform_path + "/Drivers/CMSIS/Device/ST/STM32G0xx/Include");
        includes.push_back(platform_path + "/Drivers/STM32G0xx_HAL_Driver/Inc");
    } else if (board.platform == "g4") {
        includes.push_back(platform_path + "/Drivers/CMSIS/Device/ST/STM32G4xx/Include");
        includes.push_back(platform_path + "/Drivers/STM32G4xx_HAL_Driver/Inc");
    }

    // Add USB Middleware includes if needed
    includes.push_back(platform_path + "/Middlewares/ST/STM32_USB_Device_Library/Core/Inc");
    includes.push_back(platform_path + "/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc");

    return includes;
}

std::vector<std::string> Builder::GetDefines(const BoardConfig& board) const {
    return {
        board.mcu,
        "USE_HAL_DRIVER"
    };
}

std::vector<std::string> Builder::GetCompilerFlags(const BoardConfig& board) const {
    std::vector<std::string> flags = {
        "-mcpu=" + board.cpu,
        "-mthumb",
        "-mfloat-abi=" + board.float_abi,
        "-O0",
        "-Wall",
        "-ffunction-sections",
        "-fdata-sections",
        "-fno-exceptions",  // No C++ exceptions on embedded
        "-fno-rtti"         // No RTTI on embedded
    };

    // Add FPU if using hard float
    if (board.float_abi == "hard" && !board.fpu.empty()) {
        flags.push_back("-mfpu=" + board.fpu);
    }

    return flags;
}

std::string Builder::GetLinkerScript(const BoardConfig& board) const {
    std::string board_path = GetBoardPath(board.name);

    // Check for board-specific linker script
    std::vector<std::string> board_linker_candidates;

    if (board.platform == "h7") {
        board_linker_candidates.push_back(board_path + "/STM32H723VGTX_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32H723VG_FLASH.ld");
    } else if (board.platform == "h5") {
        board_linker_candidates.push_back(board_path + "/STM32H503RB_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32H523xx_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32H533xx_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32H563xx_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32H573xx_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32H5xx_FLASH.ld");
    } else if (board.platform == "f4") {
        board_linker_candidates.push_back(board_path + "/STM32F407VG_FLASH.ld");
    } else if (board.platform == "g0") {
        board_linker_candidates.push_back(board_path + "/STM32G0B1CBUX_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32G0B1xx_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32G0xx_FLASH.ld");
    } else if (board.platform == "g4") {
        board_linker_candidates.push_back(board_path + "/STM32G431CBU_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32G431xx_FLASH.ld");
        board_linker_candidates.push_back(board_path + "/STM32G4xx_FLASH.ld");
    }

    // Check board-specific linker scripts
    for (const auto& candidate : board_linker_candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }

    // No fallback - linker script must exist in board directory
    std::cerr << "Error: No linker script found in board directory: " << board_path << std::endl;
    return "";
}

std::string Builder::GetStartupFile(const BoardConfig& board) const {
    std::string board_path = GetBoardPath(board.name);

    // Check for board-specific startup file
    std::vector<std::string> board_startup_candidates;

    if (board.platform == "h7") {
        board_startup_candidates.push_back(board_path + "/startup_stm32h723vgtx.s");
        board_startup_candidates.push_back(board_path + "/startup_stm32h723xx.s");
    } else if (board.platform == "h5") {
        board_startup_candidates.push_back(board_path + "/startup_stm32h503rb.s");
        board_startup_candidates.push_back(board_path + "/startup_stm32h523xx.s");
        board_startup_candidates.push_back(board_path + "/startup_stm32h533xx.s");
        board_startup_candidates.push_back(board_path + "/startup_stm32h563xx.s");
        board_startup_candidates.push_back(board_path + "/startup_stm32h573xx.s");
        board_startup_candidates.push_back(board_path + "/startup_stm32h5xx.s");
    } else if (board.platform == "f4") {
        board_startup_candidates.push_back(board_path + "/startup_stm32f407xx.s");
    } else if (board.platform == "g0") {
        board_startup_candidates.push_back(board_path + "/startup_stm32g0b1cbux.s");
        board_startup_candidates.push_back(board_path + "/startup_stm32g0b1xx.s");
        board_startup_candidates.push_back(board_path + "/startup_stm32g0xx.s");
    } else if (board.platform == "g4") {
        board_startup_candidates.push_back(board_path + "/startup_stm32g431xx.s");
        board_startup_candidates.push_back(board_path + "/startup_stm32g4xx.s");
    }

    // Check board-specific startup files
    for (const auto& candidate : board_startup_candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }

    // No fallback - startup file must exist in board directory
    std::cerr << "Error: No startup file found in board directory: " << board_path << std::endl;
    return "";
}

std::string Builder::GetSystemFile(const BoardConfig& board) const {
    std::string board_path = GetBoardPath(board.name);

    // Check for board-specific system file
    std::vector<std::string> board_system_candidates;

    if (board.platform == "h7") {
        board_system_candidates.push_back(board_path + "/system_stm32h7xx.c");
    } else if (board.platform == "h5") {
        board_system_candidates.push_back(board_path + "/system_stm32h5xx.c");
    } else if (board.platform == "f4") {
        board_system_candidates.push_back(board_path + "/system_stm32f4xx.c");
    } else if (board.platform == "g0") {
        board_system_candidates.push_back(board_path + "/system_stm32g0xx.c");
    } else if (board.platform == "g4") {
        board_system_candidates.push_back(board_path + "/system_stm32g4xx.c");
    }

    // Check board-specific system files
    for (const auto& candidate : board_system_candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }

    // No fallback - system file must exist in board directory
    std::cerr << "Error: No system file found in board directory: " << board_path << std::endl;
    return "";
}

std::vector<std::string> Builder::GetBoardSupportFiles(const BoardConfig& board) const {
    std::vector<std::string> board_files;
    std::string board_path = GetBoardPath(board.name);

    // Check if board directory exists
    if (!fs::exists(board_path)) {
        std::cout << "Note: No board-specific support files found at " << board_path << std::endl;
        return board_files;
    }

    // Scan for all .c and .cpp files in the board directory
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(board_path, ec)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            std::string extension = entry.path().extension().string();

            // Include .c and .cpp files (these include main.c, msp, it, syscalls, etc.)
            if (extension == ".c" || extension == ".cpp") {
                board_files.push_back(entry.path().string());
            }
        }
    }

    if (ec) {
        std::cerr << "Error scanning board directory: " << ec.message() << std::endl;
    }

    // Also scan wrapper directory for generic peripheral implementations
    std::string wrapper_path = GetResourceBasePath() + "/wrapper";
    if (fs::exists(wrapper_path)) {
        std::error_code wrapper_ec;
        for (const auto& entry : fs::directory_iterator(wrapper_path, wrapper_ec)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == ".c" || extension == ".cpp") {
                    board_files.push_back(entry.path().string());
                }
            }
        }
        if (wrapper_ec) {
            std::cerr << "Error scanning wrapper directory: " << wrapper_ec.message() << std::endl;
        }
    }

    return board_files;
}

std::vector<std::string> Builder::GetRequiredHALFiles(const BoardConfig& board, const std::vector<std::string>& hal_modules) const {
    std::string platform_path = GetPlatformPath(board.platform);
    std::string hal_driver_path;
    std::string prefix;

    // Set platform-specific paths and prefixes
    if (board.platform == "f4") {
        hal_driver_path = platform_path + "/Drivers/STM32F4xx_HAL_Driver/Src";
        prefix = "stm32f4xx_hal";
    } else if (board.platform == "h7") {
        hal_driver_path = platform_path + "/Drivers/STM32H7xx_HAL_Driver/Src";
        prefix = "stm32h7xx_hal";
    } else if (board.platform == "h5") {
        hal_driver_path = platform_path + "/Drivers/STM32H5xx_HAL_Driver/Src";
        prefix = "stm32h5xx_hal";
    } else if (board.platform == "g0") {
        hal_driver_path = platform_path + "/Drivers/STM32G0xx_HAL_Driver/Src";
        prefix = "stm32g0xx_hal";
    } else if (board.platform == "g4") {
        hal_driver_path = platform_path + "/Drivers/STM32G4xx_HAL_Driver/Src";
        prefix = "stm32g4xx_hal";
    } else {
        // Default to F4
        hal_driver_path = platform_path + "/Drivers/STM32F4xx_HAL_Driver/Src";
        prefix = "stm32f4xx_hal";
    }

    // Core HAL files always needed
    std::vector<std::string> hal_files = {
        hal_driver_path + "/" + prefix + ".c",
        hal_driver_path + "/" + prefix + "_cortex.c",
        hal_driver_path + "/" + prefix + "_rcc.c",
        hal_driver_path + "/" + prefix + "_rcc_ex.c",
        hal_driver_path + "/" + prefix + "_gpio.c",
        hal_driver_path + "/" + prefix + "_pwr.c",
        hal_driver_path + "/" + prefix + "_pwr_ex.c",
        hal_driver_path + "/" + prefix + "_dma.c"
    };

    // Add requested HAL modules
    for (const auto& module : hal_modules) {
        std::string module_file = hal_driver_path + "/" + prefix + "_" + module + ".c";
        hal_files.push_back(module_file);

        // Check if there's an _ex version
        std::string module_ex_file = hal_driver_path + "/" + prefix + "_" + module + "_ex.c";
        if (fs::exists(module_ex_file)) {
            hal_files.push_back(module_ex_file);
        }

        // Special case: PCD module needs LL USB driver
        if (module == "pcd") {
            std::string ll_usb_file = hal_driver_path + "/" + prefix.substr(0, prefix.find("_hal")) + "_ll_usb.c";
            if (fs::exists(ll_usb_file)) {
                hal_files.push_back(ll_usb_file);
            }
        }
    }

    return hal_files;
}

std::vector<std::string> Builder::GetUSBMiddlewareFiles(const BoardConfig& board) const {
    std::string platform_path = GetPlatformPath(board.platform);
    std::string usb_core_path = platform_path + "/Middlewares/ST/STM32_USB_Device_Library/Core/Src";
    std::string usb_cdc_path = platform_path + "/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src";

    std::vector<std::string> usb_files = {
        // USB Device Core files
        usb_core_path + "/usbd_core.c",
        usb_core_path + "/usbd_ctlreq.c",
        usb_core_path + "/usbd_ioreq.c",
        // USB CDC Class file
        usb_cdc_path + "/usbd_cdc.c"
    };

    return usb_files;
}

std::vector<std::string> Builder::GetLinkerFlags(const BoardConfig& board, const std::string& project_dir) const {
    return {
        "-mcpu=" + board.cpu,
        "-mthumb",
        "-mfloat-abi=" + board.float_abi,
        "-T" + GetLinkerScript(board),
        "-Wl,--gc-sections",
        "-Wl,-Map=" + project_dir + "/build/firmware.map",
        "-specs=nano.specs",
        "-specs=nosys.specs",
        "-lc",
        "-lm",
        "-lnosys"
    };
}

bool Builder::RunCommand(const std::string& command) const {
    std::cout << "Running: " << command << std::endl;
    int result = system(command.c_str());
    return result == 0;
}

bool Builder::CompileFile(const std::string& source_file,
                         const std::string& output_file,
                         const BoardConfig& board,
                         const std::string& project_dir) const {
    std::string toolchain = GetToolchainPath();
    std::string compiler;

    // Helper lambda for ends_with (C++17 compatible)
    auto ends_with = [](const std::string& str, const std::string& suffix) {
        if (suffix.size() > str.size()) return false;
        return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    // Choose compiler based on file extension
    if (ends_with(source_file, ".c")) {
        compiler = toolchain + "/arm-none-eabi-gcc";
    } else if (ends_with(source_file, ".cpp") || ends_with(source_file, ".cc")) {
        compiler = toolchain + "/arm-none-eabi-g++";
    } else if (ends_with(source_file, ".s") || ends_with(source_file, ".S")) {
        compiler = toolchain + "/arm-none-eabi-gcc";
    } else {
        std::cerr << "Unknown file type: " << source_file << std::endl;
        return false;
    }

    std::ostringstream cmd;
    cmd << compiler << " -c " << source_file << " -o " << output_file;

    // Add compiler flags (skip for assembly)
    if (!ends_with(source_file, ".s") && !ends_with(source_file, ".S")) {
        for (const auto& flag : GetCompilerFlags(board)) {
            cmd << " " << flag;
        }

        // Add defines
        for (const auto& define : GetDefines(board)) {
            cmd << " -D" << define;
        }

        // Add include paths
        for (const auto& include : GetIncludePaths(board, project_dir)) {
            cmd << " -I" << include;
        }

        // Automatically include lumos.h for user convenience
        std::string board_path = GetBoardPath(board.name);
        std::string lumos_header = board_path + "/lumos.h";
        if (fs::exists(lumos_header)) {
            cmd << " -include " << lumos_header;
        }
    } else {
        // Assembly files just need basic flags
        cmd << " -mcpu=" << board.cpu << " -mthumb";
    }

    return RunCommand(cmd.str());
}

bool Builder::LinkFiles(const std::vector<std::string>& object_files,
                       const std::string& output_elf,
                       const BoardConfig& board,
                       const std::string& project_dir) const {
    std::string toolchain = GetToolchainPath();
    std::string linker = toolchain + "/arm-none-eabi-g++";

    std::ostringstream cmd;
    cmd << linker;

    // Add all object files
    for (const auto& obj : object_files) {
        cmd << " " << obj;
    }

    cmd << " -o " << output_elf;

    // Add linker flags
    for (const auto& flag : GetLinkerFlags(board, project_dir)) {
        cmd << " " << flag;
    }

    return RunCommand(cmd.str());
}

bool Builder::CreateBinary(const std::string& elf_file, const std::string& bin_file) const {
    std::string toolchain = GetToolchainPath();
    std::string objcopy = toolchain + "/arm-none-eabi-objcopy";

    std::ostringstream cmd;
    cmd << objcopy << " -O binary " << elf_file << " " << bin_file;

    return RunCommand(cmd.str());
}

std::string Builder::PromptLanguage() const {
    std::cout << "\nNo main.c or main.cpp found in project directory." << std::endl;
    std::cout << "Select programming language:" << std::endl;
    std::cout << "  1. C++" << std::endl;
    std::cout << "  2. C" << std::endl;
    std::cout << "Enter choice [1-2]: ";

    std::string input;
    std::getline(std::cin, input);

    if (input == "2") {
        return "C";
    }
    // Default to C++
    return "C++";
}

bool Builder::GenerateMainFile(const std::string& language, const std::string& project_dir) const {
    std::string filename = (language == "C") ? "main.c" : "main.cpp";
    fs::path main_path = fs::path(project_dir) / filename;

    std::ofstream file(main_path);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to create " << filename << std::endl;
        return false;
    }

    const std::string loop_declaration = (language == "C") ? "void loop(void);" : "void loop();";
    const std::string setup_declaration = (language == "C") ? "void setup(void);" : "void setup();";

    file << "/**\n";
    file << " * Main application file\n";
    file << " * This is where the setup() and loop() functions are defined\n";
    file << " */\n\n";
    file << "/**\n";
    file << " * Setup function - called once at startup\n";
    file << " */\n";
    file << setup_declaration << "\n";
    file << "{\n";
    file << "    // Initialize your application here\n";
    file << "    // - Configure GPIO pins\n";
    file << "    // - Initialize UART, SPI, I2C, etc.\n";
    file << "    // - Set up timers\n";
    file << "}\n\n";
    file << "/**\n";
    file << " * Loop function - called repeatedly\n";
    file << " */\n";
    file << loop_declaration << "\n";
    file << "{\n";
    file << "    // Your main application logic here\n";
    file << "    // This function runs continuously\n";
    file << "}\n";

    file.close();

    std::cout << "Created " << filename << std::endl;
    return true;
}

bool Builder::CheckAndCreateMainFile(const std::string& project_dir, ProjectConfig& project) {
    fs::path main_c = fs::path(project_dir) / "main.c";
    fs::path main_cpp = fs::path(project_dir) / "main.cpp";

    bool main_c_exists = fs::exists(main_c);
    bool main_cpp_exists = fs::exists(main_cpp);
    std::string main_file;
    std::string language;

    // Check if we need to create a main file
    if (!main_c_exists && !main_cpp_exists) {
        // No main file found, prompt user for language
        language = PromptLanguage();

        // Generate the main file
        if (!GenerateMainFile(language, project_dir)) {
            return false;
        }

        main_file = (language == "C") ? "main.c" : "main.cpp";

        // If auto-discovery was used, reload the sources
        YAML::Node config = YAML::LoadFile(project_dir + "/project.yaml");
        if (!config["sources"]) {
            // Re-scan for sources since we just created a new file
            std::cout << "Re-scanning for source files..." << std::endl;
            project.sources.clear();

            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(project_dir, ec)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();
                    if (extension == ".c" || extension == ".cpp") {
                        project.sources.push_back(entry.path().filename().string());
                    }
                }
            }
        } else {
            // Add the new main file to sources list
            project.sources.push_back(main_file);
        }
    } else {
        // Main file exists, check if it's in the sources list
        main_file = main_c_exists ? "main.c" : "main.cpp";

        // Check if main file is in sources list
        bool found = false;
        for (const auto& src : project.sources) {
            if (src == main_file) {
                found = true;
                break;
            }
        }

        // If not found and we have an explicit sources list, add it
        YAML::Node config = YAML::LoadFile(project_dir + "/project.yaml");
        if (!found && config["sources"]) {
            std::cout << "Adding " << main_file << " to sources list" << std::endl;
            project.sources.push_back(main_file);
        }
    }

    return true;
}

bool Builder::Build(const std::string& project_dir) {
    std::cout << "=== Lumos Builder ===" << std::endl;
    std::cout << "Project directory: " << project_dir << std::endl;
    std::cout << std::endl;

    // Load project configuration
    ProjectConfig project;
    std::string yaml_path = project_dir + "/project.yaml";

    if (!project.Load(yaml_path, project_dir)) {
        std::cerr << "Error: Failed to load project.yaml" << std::endl;
        return false;
    }

    // Check if main file exists, create if needed
    if (!CheckAndCreateMainFile(project_dir, project)) {
        std::cerr << "Error: Failed to create main file" << std::endl;
        return false;
    }

    std::cout << "Board: " << project.board << std::endl;
    std::cout << "Sources: " << project.sources.size() << " files" << std::endl;

    // Auto-detect HAL modules if not specified
    if (project.hal_modules.empty()) {
        std::cout << "Auto-detecting HAL modules from source files..." << std::endl;
        HALModuleDetector detector;
        project.hal_modules = detector.DetectModules(project.sources, project_dir);

        if (!project.hal_modules.empty()) {
            std::cout << "Detected modules: ";
            for (size_t i = 0; i < project.hal_modules.size(); ++i) {
                std::cout << project.hal_modules[i];
                if (i < project.hal_modules.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
        } else {
            std::cout << "No HAL modules detected (using core modules only)" << std::endl;
        }
    } else {
        std::cout << "Using manually specified HAL modules: ";
        for (size_t i = 0; i < project.hal_modules.size(); ++i) {
            std::cout << project.hal_modules[i];
            if (i < project.hal_modules.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // Get board configuration
    BoardConfig board = BoardConfig::GetConfig(project.board);
    std::cout << "Platform: " << board.platform << std::endl;
    std::cout << "MCU: " << board.mcu << std::endl;
    std::cout << "CPU: " << board.cpu << std::endl;
    std::cout << std::endl;

    // Create build directory
    std::string build_dir = project_dir + "/build";
    fs::create_directories(build_dir);

    std::vector<std::string> object_files;

    // Compile user source files
    std::cout << "Compiling user sources..." << std::endl;
    for (const auto& source : project.sources) {
        std::string source_path = project_dir + "/" + source;
        std::string obj_name = fs::path(source).stem().string() + ".o";
        std::string obj_path = build_dir + "/" + obj_name;

        std::cout << "  " << source << " -> " << obj_name << std::endl;

        if (!CompileFile(source_path, obj_path, board, project_dir)) {
            std::cerr << "Error: Compilation failed for " << source << std::endl;
            return false;
        }

        object_files.push_back(obj_path);
    }
    std::cout << std::endl;

    // Compile board support files
    std::vector<std::string> board_files = GetBoardSupportFiles(board);
    if (!board_files.empty()) {
        std::cout << "Compiling board support files..." << std::endl;
        for (const auto& board_file : board_files) {
            std::string filename = fs::path(board_file).filename().string();
            std::string stem = fs::path(board_file).stem().string();

            // Rename board's main to board_main to avoid conflict with user code
            std::string obj_name;
            if (stem == "main") {
                obj_name = "board_main.o";
            } else {
                obj_name = stem + ".o";
            }
            std::string obj_path = build_dir + "/" + obj_name;

            std::cout << "  " << filename << " -> " << obj_name << std::endl;

            if (!CompileFile(board_file, obj_path, board, project_dir)) {
                std::cerr << "Error: Compilation failed for " << filename << std::endl;
                return false;
            }

            object_files.push_back(obj_path);
        }
        std::cout << std::endl;
    }

    // Compile HAL driver files
    std::cout << "Compiling HAL drivers..." << std::endl;
    std::vector<std::string> hal_files = GetRequiredHALFiles(board, project.hal_modules);
    for (const auto& hal_file : hal_files) {
        // Only compile if file exists
        if (!fs::exists(hal_file)) {
            std::cout << "  Skipping " << fs::path(hal_file).filename().string() << " (not found)" << std::endl;
            continue;
        }

        std::string hal_filename = fs::path(hal_file).filename().string();
        std::string obj_name = fs::path(hal_file).stem().string() + ".o";
        std::string obj_path = build_dir + "/" + obj_name;

        std::cout << "  " << hal_filename << " -> " << obj_name << std::endl;

        if (!CompileFile(hal_file, obj_path, board, project_dir)) {
            std::cerr << "Error: Failed to compile HAL file " << hal_filename << std::endl;
            return false;
        }

        object_files.push_back(obj_path);
    }
    std::cout << std::endl;

    // Compile USB middleware files if needed
    bool uses_usb = false;
    for (const auto& module : project.hal_modules) {
        if (module == "pcd" || module == "pcd_ex") {
            uses_usb = true;
            break;
        }
    }

    if (uses_usb) {
        std::cout << "Compiling USB middleware..." << std::endl;
        std::vector<std::string> usb_files = GetUSBMiddlewareFiles(board);
        for (const auto& usb_file : usb_files) {
            if (!fs::exists(usb_file)) {
                std::cout << "  Skipping " << fs::path(usb_file).filename().string() << " (not found)" << std::endl;
                continue;
            }

            std::string usb_filename = fs::path(usb_file).filename().string();
            std::string obj_name = fs::path(usb_file).stem().string() + ".o";
            std::string obj_path = build_dir + "/" + obj_name;

            std::cout << "  " << usb_filename << " -> " << obj_name << std::endl;

            if (!CompileFile(usb_file, obj_path, board, project_dir)) {
                std::cerr << "Error: Failed to compile USB file " << usb_filename << std::endl;
                return false;
            }

            object_files.push_back(obj_path);
        }
        std::cout << std::endl;
    }

    // Compile system files (startup file)
    std::cout << "Compiling system files..." << std::endl;

    // Startup file (board-specific or platform default)
    std::string startup_file = GetStartupFile(board);
    std::string startup_filename = fs::path(startup_file).filename().string();
    std::string startup_obj = build_dir + "/startup.o";
    std::cout << "  " << startup_filename << " -> startup.o" << std::endl;
    if (!CompileFile(startup_file, startup_obj, board, project_dir)) {
        std::cerr << "Error: Failed to compile startup file" << std::endl;
        return false;
    }
    object_files.push_back(startup_obj);
    std::cout << std::endl;

    // Note: system_stm32h7xx.c is now compiled as part of board support files

    // Link
    std::cout << "Linking..." << std::endl;
    std::string elf_file = build_dir + "/firmware.elf";
    if (!LinkFiles(object_files, elf_file, board, project_dir)) {
        std::cerr << "Error: Linking failed" << std::endl;
        return false;
    }
    std::cout << std::endl;

    // Create binary
    std::cout << "Creating binary..." << std::endl;
    std::string bin_file = build_dir + "/firmware.bin";
    if (!CreateBinary(elf_file, bin_file)) {
        std::cerr << "Error: Failed to create binary" << std::endl;
        return false;
    }
    std::cout << std::endl;

    // Print file sizes
    std::cout << "Build complete!" << std::endl;
    std::cout << "Output files:" << std::endl;
    std::cout << "  " << elf_file << std::endl;
    std::cout << "  " << bin_file << std::endl;

    // Get binary size
    if (fs::exists(bin_file)) {
        auto bin_size = fs::file_size(bin_file);
        std::cout << "  Binary size: " << bin_size << " bytes" << std::endl;
    }

    return true;
}

} // namespace Lumos

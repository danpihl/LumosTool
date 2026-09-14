#!/usr/bin/env python3
"""
Create macOS Release Package for Lumos

This script:
1. Builds yaml-cpp library for both x86_64 and arm64
2. Creates universal yaml-cpp library using lipo
3. Builds Lumos for both x86_64 (Intel) and arm64 (Apple Silicon)
4. Creates universal binaries using lipo
5. Bundles everything into a distributable package
6. Creates a .tar.gz archive with checksums
"""

import os
import sys
import shutil
import subprocess
import platform
from pathlib import Path

# Configuration
VERSION = "1.0.0"
PROJECT_ROOT = Path(__file__).parent.parent.absolute()
BUILD_DIR = PROJECT_ROOT / "build"
RELEASE_DIR = PROJECT_ROOT / "release"
PACKAGE_NAME = f"lumos-macos-{VERSION}"
PACKAGE_DIR = RELEASE_DIR / PACKAGE_NAME
YAML_CPP_DIR = PROJECT_ROOT / "third_party" / "yaml-cpp"

# Executables to build
EXECUTABLES = [
    "src/applications/lumos_simple/lumos"
]

# Resources to bundle (relative to PROJECT_ROOT)
# Format: "source_path": "destination_path_in_share"
RESOURCES = {
    "src/boards": "boards",  # Flatten: no src/ prefix in release
    "src/toolchains/macos/gcc-arm-none-eabi-10.3-2021.10": "toolchains/macos/gcc-arm-none-eabi-10.3-2021.10",
    "src/toolchains/platform": "toolchains/platform"
}


def print_step(message):
    """Print a formatted step message"""
    print(f"\n{'='*60}")
    print(f"  {message}")
    print(f"{'='*60}\n")


def run_command(cmd, cwd=None, env=None):
    """Run a shell command and handle errors"""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"Error running command: {' '.join(cmd)}")
        print(f"stdout: {result.stdout}")
        print(f"stderr: {result.stderr}")
        sys.exit(1)

    return result


def check_requirements():
    """Check if required tools are installed"""
    print_step("Checking Requirements")

    # Check if we're on macOS
    if platform.system() != "Darwin":
        print("Error: This script must be run on macOS")
        sys.exit(1)

    # Check for required tools
    required_tools = ["cmake", "lipo", "file"]
    for tool in required_tools:
        result = subprocess.run(["which", tool], capture_output=True)
        if result.returncode != 0:
            print(f"Error: {tool} not found. Please install it first.")
            sys.exit(1)
        print(f"✓ Found {tool}")

    print("✓ All requirements met")


def clean_build_dirs():
    """Clean previous build directories"""
    print_step("Cleaning Build Directories")

    dirs_to_clean = [
        BUILD_DIR / "x86_64",
        BUILD_DIR / "arm64",
        PACKAGE_DIR,
        YAML_CPP_DIR / "build_x86_64",
        YAML_CPP_DIR / "build_arm64",
        YAML_CPP_DIR / "build"
    ]

    for dir_path in dirs_to_clean:
        if dir_path.exists():
            print(f"Removing {dir_path}")
            shutil.rmtree(dir_path)

    # Create fresh directories
    RELEASE_DIR.mkdir(exist_ok=True)
    print("✓ Build directories cleaned")


def build_yaml_cpp_for_architecture(arch):
    """Build yaml-cpp library for a specific architecture"""
    print(f"Building yaml-cpp for {arch}...")

    build_dir = YAML_CPP_DIR / f"build_{arch}"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Configure yaml-cpp
    cmake_flags = [
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_OSX_ARCHITECTURES={arch}",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15",
        "-DYAML_CPP_BUILD_TESTS=OFF",
        "-DYAML_CPP_BUILD_TOOLS=OFF",
        "-DYAML_BUILD_SHARED_LIBS=OFF"
    ]

    run_command(
        ["cmake", "-S", str(YAML_CPP_DIR), "-B", str(build_dir)] + cmake_flags,
        cwd=YAML_CPP_DIR
    )

    # Build yaml-cpp
    run_command(
        ["cmake", "--build", str(build_dir), "--config", "Release", "-j"],
        cwd=YAML_CPP_DIR
    )

    # Return path to the built library
    lib_path = build_dir / "libyaml-cpp.a"
    if not lib_path.exists():
        print(f"Error: yaml-cpp library not found at {lib_path}")
        sys.exit(1)

    print(f"✓ yaml-cpp for {arch} complete: {lib_path}")
    return lib_path


def create_universal_yaml_cpp():
    """Build yaml-cpp for both architectures and create universal library"""
    print_step("Building Universal yaml-cpp Library")

    # Build for both architectures
    x86_lib = build_yaml_cpp_for_architecture("x86_64")
    arm_lib = build_yaml_cpp_for_architecture("arm64")

    # Create output directory
    universal_build_dir = YAML_CPP_DIR / "build"
    universal_build_dir.mkdir(parents=True, exist_ok=True)
    universal_lib = universal_build_dir / "libyaml-cpp.a"

    # Create universal library
    print(f"\nCreating universal yaml-cpp library...")
    run_command(
        ["lipo", "-create", str(x86_lib), str(arm_lib), "-output", str(universal_lib)]
    )

    # Verify it's universal
    result = run_command(["lipo", "-info", str(universal_lib)])
    print(f"  {result.stdout.strip()}")

    print(f"✓ Universal yaml-cpp library created: {universal_lib}")
    return universal_lib


def build_for_architecture(arch):
    """Build for a specific architecture"""
    print_step(f"Building for {arch}")

    build_dir = BUILD_DIR / arch
    build_dir.mkdir(parents=True, exist_ok=True)

    # Set architecture-specific flags
    cmake_flags = [
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_OSX_ARCHITECTURES={arch}",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15",
        "-DLUMOS_OFFICIAL_RELEASE=ON",  # Mark as official release build
        "-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON",  # Skip Qt6 GUI for release builds
    ]

    # Configure
    print(f"Configuring for {arch}...")
    run_command(
        ["cmake", "-S", str(PROJECT_ROOT), "-B", str(build_dir)] + cmake_flags,
        cwd=PROJECT_ROOT
    )

    # Build
    print(f"Building for {arch}...")
    run_command(
        ["cmake", "--build", str(build_dir), "--config", "Release", "-j"],
        cwd=PROJECT_ROOT
    )

    print(f"✓ Build for {arch} complete")
    return build_dir


def create_universal_binary(x86_build_dir, arm_build_dir, output_dir, executable_path):
    """Create a universal binary from x86_64 and arm64 versions"""
    x86_exe = x86_build_dir / executable_path
    arm_exe = arm_build_dir / executable_path
    output_exe = output_dir / Path(executable_path).name

    # Ensure output directory exists
    output_exe.parent.mkdir(parents=True, exist_ok=True)

    # Check if both binaries exist
    if not x86_exe.exists():
        print(f"Error: x86_64 binary not found: {x86_exe}")
        sys.exit(1)
    if not arm_exe.exists():
        print(f"Error: arm64 binary not found: {arm_exe}")
        sys.exit(1)

    # Create universal binary
    print(f"Creating universal binary: {output_exe.name}")
    run_command(
        ["lipo", "-create", str(x86_exe), str(arm_exe), "-output", str(output_exe)]
    )

    # Make executable
    output_exe.chmod(0o755)

    # Verify it's universal
    result = run_command(["file", str(output_exe)])
    print(f"  {result.stdout.strip()}")

    return output_exe


def create_package_structure():
    """Create the package directory structure"""
    print_step("Creating Package Structure")

    # Create directory structure
    bin_dir = PACKAGE_DIR / "bin"
    share_dir = PACKAGE_DIR / "share"

    bin_dir.mkdir(parents=True, exist_ok=True)
    share_dir.mkdir(parents=True, exist_ok=True)

    print(f"✓ Created package structure at {PACKAGE_DIR}")
    return bin_dir, share_dir


def copy_resources(share_dir):
    """Copy resource directories to package"""
    print_step("Copying Resources")

    for src_path, dst_path in RESOURCES.items():
        src = PROJECT_ROOT / src_path
        dst = share_dir / dst_path

        if src.exists():
            print(f"Copying {src_path}...")
            # Create parent directories if needed
            dst.parent.mkdir(parents=True, exist_ok=True)
            if dst.exists():
                shutil.rmtree(dst)

            # Copy the directory
            shutil.copytree(src, dst, symlinks=True)

            # Get size of copied directory
            total_size = sum(f.stat().st_size for f in dst.rglob('*') if f.is_file())
            size_mb = total_size / (1024 * 1024)
            print(f"  ✓ {src_path} ({size_mb:.1f} MB)")
        else:
            print(f"  ! Warning: {src_path} not found, skipping")

    print("✓ Resources copied")


def create_install_script():
    """Create an install script for users"""
    print_step("Creating Install Script")

    install_script = PACKAGE_DIR / "install.sh"

    script_content = """#!/bin/bash
# Lumos Installation Script for macOS

set -e

# Colors for output
RED='\\033[0;31m'
GREEN='\\033[0;32m'
YELLOW='\\033[1;33m'
NC='\\033[0m' # No Color

INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

echo "Lumos Installation Script"
echo "========================="
echo ""
echo "This will install Lumos to: $INSTALL_PREFIX"
echo ""

# Check if running with appropriate permissions
if [ ! -w "$INSTALL_PREFIX" ]; then
    echo "${YELLOW}Warning: You may need to run this with sudo${NC}"
    echo "Try: sudo ./install.sh"
    echo ""
fi

# Create directories
echo "Creating directories..."
mkdir -p "$INSTALL_PREFIX/bin"
mkdir -p "$INSTALL_PREFIX/share/lumos"

# Install binaries
echo "Installing binaries..."
cp -v bin/* "$INSTALL_PREFIX/bin/"
chmod +x "$INSTALL_PREFIX/bin/lumos"
chmod +x "$INSTALL_PREFIX/bin/simple_serial"

# Install resources
echo "Installing resources..."
cp -rv share/* "$INSTALL_PREFIX/share/lumos/"

echo ""
echo "${GREEN}✓ Installation complete!${NC}"
echo ""
echo "Lumos has been installed to: $INSTALL_PREFIX/bin/lumos"
echo ""
echo "Installed components:"
echo "  - lumos CLI tool (universal binary)"
echo "  - ARM GCC toolchain (gcc-arm-none-eabi 10.3)"
echo "  - STM32 HAL drivers and board definitions"
echo ""
echo "Make sure $INSTALL_PREFIX/bin is in your PATH."
echo "You may need to run: export PATH=\"$INSTALL_PREFIX/bin:\\$PATH\""
echo ""
echo "To get started:"
echo "  mkdir my_project && cd my_project"
echo "  lumos init"
echo "  lumos build"
echo "  lumos flash"
echo ""
"""

    with open(install_script, 'w') as f:
        f.write(script_content)

    install_script.chmod(0o755)
    print(f"✓ Created install script: {install_script.name}")


def create_readme():
    """Create a README for the release package"""
    print_step("Creating README")

    readme = PACKAGE_DIR / "README.txt"

    readme_content = f"""Lumos v{VERSION} - macOS Release
{'='*60}

INSTALLATION
------------

Option 1: Automatic Installation (Recommended)
  Run the install script:
    ./install.sh

  This will install Lumos to /usr/local
  You may need sudo: sudo ./install.sh

Option 2: Manual Installation
  Copy the binaries to your PATH:
    sudo cp bin/lumos /usr/local/bin/
    sudo cp bin/simple_serial /usr/local/bin/
    sudo cp -r share/lumos /usr/local/share/

Option 3: Run from this directory
  Add the bin directory to your PATH:
    export PATH="$(pwd)/bin:$PATH"

  Set LUMOS_ROOT environment variable:
    export LUMOS_ROOT="$(pwd)/share/lumos"


GETTING STARTED
---------------

1. Create a new project:
     mkdir my_project && cd my_project
     lumos init

2. Build your project:
     lumos build

3. Flash to your STM32:
     lumos flash

4. Monitor serial output:
     lumos monitor


COMMANDS
--------

  lumos init              - Initialize a new project
  lumos build             - Build the current project
  lumos flash [port]      - Flash firmware to STM32
  lumos monitor [port]    - Monitor serial output
  lumos reset <port>      - Reset a serial port
  lumos ports             - List available serial ports
  lumos --help            - Show help
  lumos --version         - Show version


SYSTEM REQUIREMENTS
-------------------

- macOS 10.15 (Catalina) or later
- Universal binary (Intel and Apple Silicon)
- ARM GCC toolchain is bundled (gcc-arm-none-eabi 10.3)
- STM32 HAL drivers and CMSIS libraries included


SUPPORT
-------

Documentation: https://github.com/yourusername/lumos
Issues: https://github.com/yourusername/lumos/issues


LICENSE
-------

[Your License Here]

"""

    with open(readme, 'w') as f:
        f.write(readme_content)

    print(f"✓ Created README: {readme.name}")


def create_archive():
    """Create a .tar.gz archive of the package"""
    print_step("Creating Archive")

    archive_name = f"{PACKAGE_NAME}.tar.gz"
    archive_path = RELEASE_DIR / archive_name

    # Remove old archive if exists
    if archive_path.exists():
        archive_path.unlink()

    # Create archive
    print(f"Creating {archive_name}...")
    run_command(
        ["tar", "-czf", archive_name, PACKAGE_NAME],
        cwd=RELEASE_DIR
    )

    # Get file size
    size_mb = archive_path.stat().st_size / (1024 * 1024)

    print(f"✓ Created archive: {archive_path}")
    print(f"  Size: {size_mb:.2f} MB")

    return archive_path


def calculate_checksums(archive_path):
    """Calculate SHA256 checksum of the archive"""
    print_step("Calculating Checksums")

    result = run_command(["shasum", "-a", "256", archive_path.name], cwd=archive_path.parent)
    checksum = result.stdout.strip()

    # Save to file
    checksum_file = archive_path.parent / f"{archive_path.name}.sha256"
    with open(checksum_file, 'w') as f:
        f.write(checksum + '\n')

    print(f"SHA256: {checksum}")
    print(f"✓ Saved to: {checksum_file.name}")

    return checksum


def main():
    """Main build process"""
    print(f"""
    ╔════════════════════════════════════════════════════════╗
    ║        Lumos macOS Release Builder v{VERSION}           ║
    ╚════════════════════════════════════════════════════════╝
    """)

    try:
        # Step 1: Check requirements
        check_requirements()

        # Step 2: Clean old builds
        clean_build_dirs()

        # Step 3: Build universal yaml-cpp library
        create_universal_yaml_cpp()

        # Step 4: Build for x86_64
        x86_build_dir = build_for_architecture("x86_64")

        # Step 5: Build for arm64
        arm_build_dir = build_for_architecture("arm64")

        # Step 6: Create package structure
        bin_dir, share_dir = create_package_structure()

        # Step 7: Create universal binaries
        print_step("Creating Universal Binaries")
        for exe_path in EXECUTABLES:
            create_universal_binary(x86_build_dir, arm_build_dir, bin_dir, exe_path)

        # Step 8: Copy resources
        copy_resources(share_dir)

        # Step 9: Create install script
        create_install_script()

        # Step 10: Create README
        create_readme()

        # Step 11: Create archive
        archive_path = create_archive()

        # Step 12: Calculate checksums
        checksum = calculate_checksums(archive_path)

        # Success message
        print_step("Build Complete! 🎉")

        print("✓ Created universal binaries (x86_64 + arm64)")
        print("  Works natively on both Intel and Apple Silicon Macs")

        print(f"\nPackage created: {archive_path}")
        print(f"Package directory: {PACKAGE_DIR}")
        print(f"\nTo test the installation:")
        print(f"  cd {PACKAGE_DIR}")
        print(f"  ./install.sh")
        print(f"\nTo distribute:")
        print(f"  Upload {archive_path.name} to GitHub releases")
        print(f"  Include the SHA256 checksum: {checksum.split()[0]}")

    except KeyboardInterrupt:
        print("\n\nBuild interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()

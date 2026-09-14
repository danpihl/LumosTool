#!/usr/bin/env python3
"""
Create Linux musl Release Package for Lumos

Builds a fully static lumos binary using Alpine Linux + musl libc.
The resulting binary has zero shared-library dependencies and runs on any
Linux distro (glibc or musl) with a kernel >= 3.2, including Alpine,
Void Linux, and minimal/embedded environments.

This script:
1. Checks requirements (Docker must be installed and running)
2. Builds the Docker image (docker/Dockerfile.linux-musl) using Alpine + musl
3. Extracts the compiled lumos binary from the image
4. Bundles everything into a distributable package
5. Creates a .tar.gz archive with checksums
"""

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

# Configuration
VERSION = "1.0.0"
ARM_TOOLCHAIN_VERSION = "10.3-2021.10"
ARM_TOOLCHAIN_NAME = f"gcc-arm-none-eabi-{ARM_TOOLCHAIN_VERSION}"
PROJECT_ROOT = Path(__file__).parent.parent.absolute()
RELEASE_DIR = PROJECT_ROOT / "release"
PACKAGE_NAME = f"lumos-linux-musl-{VERSION}"
PACKAGE_DIR = RELEASE_DIR / PACKAGE_NAME
DOCKER_IMAGE = "lumos-linux-musl-builder"
DOCKER_FILE = PROJECT_ROOT / "docker" / "Dockerfile.linux-musl"

# Resources to bundle (relative to PROJECT_ROOT)
# Format: "source_path": "destination_path_in_share"
RESOURCES = {
    "src/boards": "boards",
    "src/toolchains/platform": "toolchains/platform",
    f"src/toolchains/linux/{ARM_TOOLCHAIN_NAME}": f"toolchains/linux/{ARM_TOOLCHAIN_NAME}",
}


def print_step(message):
    print(f"\n{'='*60}")
    print(f"  {message}")
    print(f"{'='*60}\n")


def run_command(cmd, cwd=None, env=None, capture=True):
    print(f"Running: {' '.join(str(c) for c in cmd)}")
    result = subprocess.run(
        cmd, cwd=cwd, env=env,
        capture_output=capture, text=True
    )
    if result.returncode != 0:
        print(f"Error running command: {' '.join(str(c) for c in cmd)}")
        if capture:
            print(f"stdout: {result.stdout}")
            print(f"stderr: {result.stderr}")
        sys.exit(1)
    return result


def check_requirements():
    print_step("Checking Requirements")

    result = subprocess.run(["which", "docker"], capture_output=True)
    if result.returncode != 0:
        print("Error: docker not found. Please install Docker Desktop or Docker Engine.")
        sys.exit(1)
    print("✓ Found docker")

    result = subprocess.run(["docker", "info"], capture_output=True, text=True)
    if result.returncode != 0:
        print("Error: Docker daemon is not running. Please start Docker.")
        sys.exit(1)
    print("✓ Docker daemon is running")

    if not DOCKER_FILE.exists():
        print(f"Error: {DOCKER_FILE} not found.")
        sys.exit(1)
    print(f"✓ Found {DOCKER_FILE.relative_to(PROJECT_ROOT)}")

    yaml_cmake = PROJECT_ROOT / "third_party" / "yaml-cpp" / "CMakeLists.txt"
    if not yaml_cmake.exists():
        print("Error: yaml-cpp submodule is not initialised.")
        print("Run: git submodule update --init third_party/yaml-cpp")
        sys.exit(1)
    print("✓ yaml-cpp submodule present")

    print("✓ All requirements met")


def clean_dirs():
    print_step("Cleaning Previous Release")

    if PACKAGE_DIR.exists():
        print(f"Removing {PACKAGE_DIR}")
        subprocess.run(["rm", "-rf", str(PACKAGE_DIR)], check=True)

    RELEASE_DIR.mkdir(exist_ok=True)
    print("✓ Done")


def build_docker_image():
    print_step("Building Docker Image (Alpine + musl)")

    run_command(
        [
            "docker", "build",
            "-f", str(DOCKER_FILE),
            "-t", DOCKER_IMAGE,
            str(PROJECT_ROOT),
        ],
        capture=False,
    )
    print(f"\n✓ Image '{DOCKER_IMAGE}' built successfully")


def extract_binary(bin_dir):
    print_step("Extracting Binary from Docker Image")

    container_name = "lumos-musl-extract-tmp"
    subprocess.run(["docker", "rm", "-f", container_name], capture_output=True)

    try:
        run_command(["docker", "create", "--name", container_name, DOCKER_IMAGE])

        lumos_bin = bin_dir / "lumos"
        run_command(
            ["docker", "cp",
             f"{container_name}:/src/build/src/applications/lumos_simple/lumos",
             str(lumos_bin)]
        )
        lumos_bin.chmod(0o755)

        result = run_command(["file", str(lumos_bin)])
        print(f"  {result.stdout.strip()}")

        if "ELF" not in result.stdout:
            print("Error: extracted file does not look like a Linux ELF binary.")
            sys.exit(1)

        if "statically linked" not in result.stdout:
            print("Warning: binary does not appear to be statically linked.")

        print(f"✓ Binary extracted: {lumos_bin}")
        return lumos_bin

    finally:
        subprocess.run(["docker", "rm", "-f", container_name], capture_output=True)




def create_package_structure():
    print_step("Creating Package Structure")

    bin_dir = PACKAGE_DIR / "bin"
    share_dir = PACKAGE_DIR / "share"
    bin_dir.mkdir(parents=True, exist_ok=True)
    share_dir.mkdir(parents=True, exist_ok=True)

    print(f"✓ Created package structure at {PACKAGE_DIR}")
    return bin_dir, share_dir


def copy_resources(share_dir):
    print_step("Copying Resources")

    for src_path, dst_path in RESOURCES.items():
        src = PROJECT_ROOT / src_path
        dst = share_dir / dst_path

        if src.exists():
            print(f"Copying {src_path}...")
            dst.parent.mkdir(parents=True, exist_ok=True)
            if dst.exists():
                subprocess.run(["rm", "-rf", str(dst)], check=True)
            shutil.copytree(src, dst, symlinks=True)

            total_size = sum(f.stat().st_size for f in dst.rglob("*") if f.is_file())
            print(f"  ✓ {src_path} ({total_size / (1024 * 1024):.1f} MB)")
        else:
            print(f"  ! Warning: {src_path} not found, skipping")

    print("✓ Resources copied")


def create_install_script():
    print_step("Creating Install Script")

    install_script = PACKAGE_DIR / "install.sh"

    script_content = """\
#!/bin/bash
# Lumos Installation Script for Linux (musl/static build)

set -e

INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

echo "Lumos Installation Script (static/musl build)"
echo "==============================================="
echo ""
echo "This will install Lumos to: $INSTALL_PREFIX"
echo ""

if [ ! -w "$INSTALL_PREFIX" ]; then
    echo "Warning: You may need to run this with sudo"
    echo "Try: sudo ./install.sh"
    echo ""
fi

echo "Creating directories..."
mkdir -p "$INSTALL_PREFIX/bin"
mkdir -p "$INSTALL_PREFIX/share/lumos"

echo "Installing binary..."
cp -v bin/lumos "$INSTALL_PREFIX/bin/"
chmod +x "$INSTALL_PREFIX/bin/lumos"

echo "Installing resources..."
cp -rv share/* "$INSTALL_PREFIX/share/lumos/"

echo ""
echo "✓ Installation complete!"
echo ""
echo "Lumos has been installed to: $INSTALL_PREFIX/bin/lumos"
echo ""
echo "NOTE: This release does not bundle an ARM toolchain."
echo "Install one with:"
echo "  sudo apt install gcc-arm-none-eabi          # Debian/Ubuntu"
echo "  sudo dnf install arm-none-eabi-gcc-cs       # Fedora"
echo "  sudo pacman -S arm-none-eabi-gcc            # Arch"
echo "  apk add gcc-arm-none-eabi                   # Alpine"
echo ""
echo "Make sure $INSTALL_PREFIX/bin is in your PATH."
echo ""
echo "To get started:"
echo "  mkdir my_project && cd my_project"
echo "  lumos init"
echo "  lumos build"
echo "  lumos flash"
echo ""
"""

    install_script.write_text(script_content)
    install_script.chmod(0o755)
    print(f"✓ Created install script: {install_script.name}")


def create_readme():
    print_step("Creating README")

    readme = PACKAGE_DIR / "README.txt"

    readme_content = f"""\
Lumos v{VERSION} - Linux Release (static/musl build)
{'='*60}

This is the fully static build of Lumos, compiled against musl libc.
It has zero shared-library dependencies and runs on any Linux distro
with a kernel >= 3.2, including Alpine Linux, Void Linux, Arch, Debian,
Ubuntu, Fedora, RHEL, and minimal/embedded environments.

For mainstream distros (Ubuntu/Debian/Fedora/RHEL/Arch) you may prefer
the glibc build (lumos-linux-{VERSION}.tar.gz) which is linked against
a very old glibc (2.17) for broad compatibility.

INSTALLATION
------------

Option 1: Automatic Installation (Recommended)
  Run the install script:
    ./install.sh

  This will install Lumos to /usr/local
  You may need sudo: sudo ./install.sh

Option 2: Manual Installation
  sudo cp bin/lumos /usr/local/bin/
  sudo cp -r share/lumos /usr/local/share/

Option 3: Run from this directory
  export PATH="$(pwd)/bin:$PATH"
  export LUMOS_ROOT="$(pwd)/share/lumos"


REQUIREMENTS
------------

- Linux x86_64, kernel >= 3.2
- No runtime library dependencies (fully static binary)
- ARM GCC cross-compiler (NOT bundled — install separately):
    sudo apt install gcc-arm-none-eabi          # Debian/Ubuntu
    sudo dnf install arm-none-eabi-gcc-cs       # Fedora
    sudo pacman -S arm-none-eabi-gcc            # Arch
    apk add gcc-arm-none-eabi                   # Alpine


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

  lumos init              - Initialise a new project
  lumos build             - Build the current project
  lumos flash [port]      - Flash firmware to STM32
  lumos monitor [port]    - Monitor serial output
  lumos reset <port>      - Reset a serial port
  lumos ports             - List available serial ports
  lumos --help            - Show help
  lumos --version         - Show version


SUPPORT
-------

Documentation: https://github.com/LumosRobotics/LumosTool
Issues:        https://github.com/LumosRobotics/LumosTool/issues
"""

    readme.write_text(readme_content)
    print(f"✓ Created README: {readme.name}")


def create_archive():
    print_step("Creating Archive")

    archive_name = f"{PACKAGE_NAME}.tar.gz"
    archive_path = RELEASE_DIR / archive_name

    if archive_path.exists():
        archive_path.unlink()

    print(f"Creating {archive_name}...")
    run_command(
        ["tar", "-czf", archive_name, PACKAGE_NAME],
        cwd=RELEASE_DIR
    )

    size_mb = archive_path.stat().st_size / (1024 * 1024)
    print(f"✓ Created archive: {archive_path}")
    print(f"  Size: {size_mb:.2f} MB")

    return archive_path


def calculate_checksums(archive_path):
    print_step("Calculating Checksums")

    if platform.system() == "Darwin":
        result = run_command(
            ["shasum", "-a", "256", archive_path.name],
            cwd=archive_path.parent
        )
    else:
        result = run_command(
            ["sha256sum", archive_path.name],
            cwd=archive_path.parent
        )

    checksum_line = result.stdout.strip()
    checksum_file = archive_path.parent / f"{archive_path.name}.sha256"
    checksum_file.write_text(checksum_line + "\n")

    print(f"SHA256: {checksum_line}")
    print(f"✓ Saved to: {checksum_file.name}")

    return checksum_line


def main():
    print(f"""
    ╔════════════════════════════════════════════════════════╗
    ║     Lumos Linux musl Release Builder v{VERSION}          ║
    ╚════════════════════════════════════════════════════════╝
    """)

    try:
        check_requirements()
        clean_dirs()
        build_docker_image()

        bin_dir, share_dir = create_package_structure()

        extract_binary(bin_dir)
        copy_resources(share_dir)
        create_install_script()
        create_readme()

        archive_path = create_archive()
        checksum = calculate_checksums(archive_path)

        print_step("Build Complete!")
        print(f"Package:   {archive_path}")
        print(f"Directory: {PACKAGE_DIR}")
        print(f"SHA256:    {checksum.split()[0]}")
        print(f"\nTo test the installation:")
        print(f"  cd {PACKAGE_DIR} && ./install.sh")
        print(f"\nTo distribute:")
        print(f"  Upload {archive_path.name} to GitHub releases")

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

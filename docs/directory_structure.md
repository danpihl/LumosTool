# Directory Structure

Lumos uses different directory structures for development and release builds to optimize organization.

## Development Structure

When developing or building from source:

```
LumosTool/
├── src/
│   ├── toolchains/
│   │   ├── gcc-arm-none-eabi-10.3-2021.10/
│   │   └── platform/
│   └── boards/
│       └── lumos_brain/
├── build/
│   └── src/applications/lumos_simple/lumos
└── examples/
```

**LUMOS_ROOT**: Points to repository root (`/path/to/LumosTool`)
- Toolchains: `{LUMOS_ROOT}/src/toolchains/`
- Boards: `{LUMOS_ROOT}/src/boards/`

## Release Structure

When installed via Homebrew or manual installation:

```
/usr/local/
├── bin/
│   └── lumos
└── share/
    └── lumos/
        ├── toolchains/
        │   ├── gcc-arm-none-eabi-10.3-2021.10/
        │   └── platform/
        └── boards/
            └── lumos_brain/
```

**LUMOS_ROOT**: Points to `/usr/local/share/lumos`
- Toolchains: `{LUMOS_ROOT}/toolchains/` (no `src/` prefix)
- Boards: `{LUMOS_ROOT}/boards/` (no `src/` prefix)

## Package Structure

The release package contains:

```
lumos-macos-1.0.0/
├── bin/
│   └── lumos                 # Universal binary (x86_64 + arm64)
├── share/
│   └── lumos/
│       ├── toolchains/       # Flattened (no src/)
│       └── boards/           # Flattened (no src/)
├── install.sh
└── README.txt
```

## Auto-Detection Logic

The builder automatically detects which structure to use:

```cpp
std::string GetResourceBasePath() const {
    // Check if we have development structure (with src/)
    fs::path dev_path = fs::path(lumos_root_) / "src" / "toolchains";
    if (fs::exists(dev_path)) {
        return lumos_root_ + "/src";
    }
    // Otherwise assume release structure (no src/)
    return lumos_root_;
}
```

### Path Resolution

```cpp
// Development (macOS):
GetToolchainPath() → "/path/to/LumosTool/src/toolchains/macos/gcc-arm-none-eabi-10.3-2021.10/bin"
GetBoardPath("LumosBrain") → "/path/to/LumosTool/src/boards/lumos_brain"

// Release (macOS):
GetToolchainPath() → "/usr/local/share/lumos/toolchains/macos/gcc-arm-none-eabi-10.3-2021.10/bin"
GetBoardPath("LumosBrain") → "/usr/local/share/lumos/boards/lumos_brain"
```

## Validation

Both structures are validated in `IsValidLumosRoot()`:

```cpp
bool IsValidLumosRoot(const fs::path& path) {
    // Development structure: src/toolchains, src/boards
    bool dev_structure = fs::exists(path / "src" / "toolchains") &&
                         fs::exists(path / "src" / "boards");

    // Release structure: toolchains, boards (no src/)
    bool release_structure = fs::exists(path / "toolchains") &&
                             fs::exists(path / "boards");

    return dev_structure || release_structure;
}
```

## Benefits

**Development:**
- Clear organization with everything under `src/`
- Familiar source code layout
- Easy to navigate

**Release:**
- Flatter structure (no unnecessary `src/` wrapper)
- Follows Unix conventions (`/usr/local/share/app/`)
- Smaller installation footprint
- Cleaner user experience

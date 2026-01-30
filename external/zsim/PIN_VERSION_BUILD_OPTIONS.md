# PIN Version Build Options

This document describes how to control PIN version selection when building zsim.

## Overview

The build system supports three modes for PIN version selection:
1. **Automatic Detection** (default) - Detects PIN version from filesystem structure
2. **Manual Override** - User specifies PIN version explicitly
3. **Error Reporting** - Clear error messages when PIN is not found or mismatched

## Usage

### Mode 1: Automatic Detection (Default)

Simply build without any options - the build system will auto-detect your PIN version:

```bash
export PINPATH=/path/to/pin-3.28-98749-g6643ecee5-gcc-linux
cd pimid/external/zsim
scons -j$(nproc)
```

**Output:**
```
Auto-detected PIN 3.x
--------------------------------------------------------------------------------
PIN Configuration:
  PINPATH: /path/to/pin-3.28-98749-g6643ecee5-gcc-linux
  Version: 3.x (auto-detected)
  Headers: /path/to/pin-3.28-98749-g6643ecee5-gcc-linux/source/include/pin
  XED Library: xed-intel64
--------------------------------------------------------------------------------
```

### Mode 2: Manual PIN 2.x Specification

Force the build system to use PIN 2.x paths:

```bash
export PINPATH=/path/to/pin-2.14-71313-gcc.4.4.7-linux
cd pimid/external/zsim
scons --pin-version=2 -j$(nproc)
```

**Output:**
```
User specified PIN 2.x
--------------------------------------------------------------------------------
PIN Configuration:
  PINPATH: /path/to/pin-2.14-71313-gcc.4.4.7-linux
  Version: 2.x (user-specified)
  Headers: /path/to/pin-2.14-71313-gcc.4.4.7-linux/source/include
  XED Library: xed2-intel64
--------------------------------------------------------------------------------
```

### Mode 3: Manual PIN 3.x Specification

Force the build system to use PIN 3.x paths:

```bash
export PINPATH=/path/to/pin-3.28-98749-g6643ecee5-gcc-linux
cd pimid/external/zsim
scons --pin-version=3 -j$(nproc)
```

**Output:**
```
User specified PIN 3.x
--------------------------------------------------------------------------------
PIN Configuration:
  PINPATH: /path/to/pin-3.28-98749-g6643ecee5-gcc-linux
  Version: 3.x (user-specified)
  Headers: /path/to/pin-3.28-98749-g6643ecee5-gcc-linux/source/include/pin
  XED Library: xed-intel64
--------------------------------------------------------------------------------
```

## Error Handling

### Error 1: PIN Not Found

If `PINPATH` is not set:

```bash
scons -j$(nproc)
```

**Error Output:**
```
================================================================================
ERROR: PIN not found!
================================================================================
You need to define the $PINPATH environment variable with Pin's path

Example:
  export PINPATH=/path/to/pin-3.28-98749-g6643ecee5-gcc-linux
  scons -j$(nproc)

Download PIN from:
  https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html
================================================================================
```

### Error 2: PIN Directory Does Not Exist

If `PINPATH` points to a non-existent directory:

```bash
export PINPATH=/nonexistent/path
scons -j$(nproc)
```

**Error Output:**
```
================================================================================
ERROR: PIN directory does not exist!
================================================================================
PINPATH is set to: /nonexistent/path
But this directory does not exist.

Please verify your PINPATH and try again.
================================================================================
```

### Error 3: PIN Version Auto-Detection Failed

If PIN is installed but headers are not in expected locations:

```bash
export PINPATH=/path/to/corrupted/pin
scons -j$(nproc)
```

**Error Output:**
```
================================================================================
ERROR: Could not detect PIN version!
================================================================================
Could not find pin.H in /path/to/corrupted/pin
Checked:
  PIN 2.x location: /path/to/corrupted/pin/source/include/pin.H
  PIN 3.x location: /path/to/corrupted/pin/source/include/pin/pin.H

Please verify:
  1. PINPATH is correct: /path/to/corrupted/pin
  2. PIN is properly installed
  3. Or manually specify version: scons --pin-version=2 or --pin-version=3
================================================================================
```

### Error 4: Version Mismatch (PIN 2.x Specified but PIN 3.x Installed)

```bash
export PINPATH=/path/to/pin-3.28-98749-g6643ecee5-gcc-linux
scons --pin-version=2 -j$(nproc)
```

**Error Output:**
```
User specified PIN 2.x
================================================================================
ERROR: PIN 2.x headers not found!
================================================================================
You specified --pin-version=2, but PIN 2.x structure not detected.
Checked: /path/to/pin-3.28-98749-g6643ecee5-gcc-linux/source/include/pin.H

Either:
  1. Remove --pin-version=2 to use auto-detection
  2. Verify you have PIN 2.x installed at: /path/to/pin-3.28-98749-g6643ecee5-gcc-linux
================================================================================
```

### Error 5: Version Mismatch (PIN 3.x Specified but PIN 2.x Installed)

```bash
export PINPATH=/path/to/pin-2.14-71313-gcc.4.4.7-linux
scons --pin-version=3 -j$(nproc)
```

**Error Output:**
```
User specified PIN 3.x
================================================================================
ERROR: PIN 3.x headers not found!
================================================================================
You specified --pin-version=3, but PIN 3.x structure not detected.
Checked: /path/to/pin-2.14-71313-gcc.4.4.7-linux/source/include/pin/pin.H

Either:
  1. Remove --pin-version=3 to use auto-detection
  2. Verify you have PIN 3.x installed at: /path/to/pin-2.14-71313-gcc.4.4.7-linux
================================================================================
```

### Error 6: Invalid PIN Version Specified

```bash
scons --pin-version=4 -j$(nproc)
```

**Error Output:**
```
================================================================================
ERROR: Invalid PIN version specified!
================================================================================
You specified: --pin-version=4
Valid options are: 2 or 3

Example:
  scons --pin-version=3
  scons --pin-version=2
================================================================================
```

### Error 7: XED Library Not Found

If PIN installation is incomplete or corrupted:

**Error Output:**
```
================================================================================
ERROR: XED library not found!
================================================================================
Could not find XED headers in /path/to/pin
Checked:
  PIN 2.x location: /path/to/pin/extras/xed2-intel64/include
  PIN 3.x location: /path/to/pin/extras/xed-intel64/include

Please verify PIN installation is complete.
================================================================================
```

## Command-Line Help

To see all available build options:

```bash
scons --help
```

Look for the `--pin-version` option in the output:

```
Local Options:
  --pin-version=VERSION     Manually specify PIN version (2 or 3). If not
                            specified, version is auto-detected.
```

## When to Use Manual Version Specification

### Use Auto-Detection When:
- ✅ You have a standard PIN installation
- ✅ You're building for the first time
- ✅ You only have one PIN version installed
- ✅ You trust the auto-detection

### Use Manual Specification When:
- ⚙️ You have multiple PIN versions installed
- ⚙️ Auto-detection picks the wrong version
- ⚙️ You want to be explicit about which version to use
- ⚙️ You're debugging build issues
- ⚙️ You're writing build scripts that should be deterministic

## Examples

### Example 1: First-time Ubuntu 24.04 Setup (Auto-detect)

```bash
# Download PIN 3.28
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz
tar xzf pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz
export PINPATH=$PWD/pin-3.28-98749-g6643ecee5-gcc-linux

# Build with auto-detection
cd pimid/external/zsim
scons -j$(nproc)
```

### Example 2: Explicit PIN 3.x Build

```bash
export PINPATH=/opt/pin-3.28
cd pimid/external/zsim
scons --pin-version=3 -j$(nproc)
```

### Example 3: Ubuntu 18.04 Legacy Build (PIN 2.x)

```bash
export PINPATH=/opt/pin-2.14-71313-gcc.4.4.7-linux
cd pimid/external/zsim
scons --pin-version=2 -j$(nproc)
```

### Example 4: Testing Both Versions (CI/CD)

```bash
# Test PIN 2.x build
export PINPATH=/opt/pin-2.14
scons --pin-version=2 -j$(nproc)
scons -c  # clean

# Test PIN 3.x build
export PINPATH=/opt/pin-3.28
scons --pin-version=3 -j$(nproc)
```

## Build Script Integration

For automated builds, use explicit version specification:

```bash
#!/bin/bash
set -e

PIN_VERSION=${PIN_VERSION:-3}  # Default to PIN 3.x
PINPATH=${PINPATH:-/opt/pin-3.28}

echo "Building zsim with PIN ${PIN_VERSION}.x"
cd pimid/external/zsim
scons --pin-version=${PIN_VERSION} -j$(nproc)
```

## Troubleshooting

### Problem: Auto-detection picks wrong version

**Solution:** Use manual specification:
```bash
scons --pin-version=3 -j$(nproc)
```

### Problem: Build fails with "pin.H not found"

**Solution:** Verify PINPATH and PIN installation:
```bash
ls -la $PINPATH/source/include/pin.H      # PIN 2.x
ls -la $PINPATH/source/include/pin/pin.H  # PIN 3.x
```

### Problem: Need to switch between PIN versions

**Solution:** Change PINPATH and rebuild:
```bash
scons -c  # Clean previous build
export PINPATH=/path/to/different/pin
scons --pin-version=3 -j$(nproc)
```

## Detection Logic

The build system uses the following detection logic:

```
1. Check if user specified --pin-version
   YES → Use user-specified version (validate it matches filesystem)
   NO → Auto-detect

2. Auto-detect:
   a. Check $PINPATH/source/include/pin.H exists
      YES → PIN 2.x detected
      NO → Check $PINPATH/source/include/pin/pin.H
         YES → PIN 3.x detected
         NO → ERROR (neither 2.x nor 3.x found)

3. Validate XED library exists:
   - PIN 2.x: Check extras/xed2-intel64/include
   - PIN 3.x: Check extras/xed-intel64/include

4. Report configuration to user
```

## Summary

| Mode | Command | When to Use |
|------|---------|-------------|
| **Auto-detect** | `scons` | Standard builds, single PIN version |
| **Force PIN 2.x** | `scons --pin-version=2` | Multiple versions, explicit control |
| **Force PIN 3.x** | `scons --pin-version=3` | Multiple versions, explicit control |
| **Clean build** | `scons -c` | Switch PIN versions, troubleshooting |

---

**Version:** 1.0
**Last Updated:** 2025-11-23
**Compatibility:** PIN 2.x and PIN 3.x

# Sawbuck-ng &nbsp; <img src="./sawbuck/icon.png" width="38" height="38">

Sawbuck-ng is a maintained fork of [Sawbuck](https://code.google.com/archive/p/sawbuck/), a Chromium log viewer.  

Dependencies have been updated/restored, build support for Visual Studio 2022 added, with further enhancements listed below.  

## Synopsis

Sawbuck is a log viewer and controller for Chromium logging on Windows, and for other applications or plugins that use
the logging facility in Chromium's [`//base` library](https://chromium.googlesource.com/chromium/src/base/).

Logging in Chromium is integrated with [Event Tracing for Windows (ETW)](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/event-tracing-for-windows),
which allows ETW controllers like Sawbuck to control log verbosity at runtime. The logging integration also captures the call stack at the logging site,
which can then be resolved and displayed in Sawbuck using a [packaged `dbghelp.dll`](./third_party/debugging_tools/README.thirdparty).  

## Enhancements
 - Bug fixes across the board.
 - Updated dependencies to newer Chromium M49 revisions.
 - Building with current toolchains restored.
 - Legacy Windows support restored, down to Windows XP.

## Supported Windows versions

**XP, Vista, 7, 8, 8.1, 10 and 11, 32-bit and 64-bit — all from one binary.**

XP support is what `build.bat` produces by default rather than a separate
build, and comes from [winwrap](https://github.com/Alex313031/winwrap) plus a
lowered subsystem version &ndash; see [BUILDING.md](./BUILDING.md#windows-xp)
for how it works, what it costs and how to build without it. Nothing is given
up on a modern Windows: each of winwrap's functions asks the running OS for the
real one first and falls back only where there is none, so Windows 7 and later
run the system's own code throughout.

XP last worked in pre-0.6 builds from early 2010, before the project's first
tagged release, so no published Sawbuck binary has run on it since. This one
does, and without pinning an old toolset: it is built against the current
Windows SDK with Visual Studio 2022.

## Download
Download a pre-built .msi installer from [Releases](https://github.com/Alex313031/sawbuck-ng/releases).
The XP builds come as a .zip and are suffixed with "_xp".

## Usage
For how to use Sawbuck or integrate your own app with Sawbuck, see [USAGE.md](./sawbuck/USAGE.md).

## Building
Instructions for building from source, can be found in [BUILDING.md](./BUILDING.md).

## License
Licensed under the [Apache 2.0 License](./LICENSE.md), with third party libraries under their own respective licenses.

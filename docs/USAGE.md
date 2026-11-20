## Usage

Sawbuck watches a live ETW session and shows Chromium's log messages as they
are emitted, or reads an `.etl` file you already have.

## Capturing Chromium Logs

**Sawbuck has to run as administrator to capture.** It starts two ETW sessions:
its own, `Sawbuck Log Session`, and the NT Kernel Logger, which a non-elevated
process cannot start. The kernel session is not optional — it supplies the
image-load and process events that let module names in call stacks be resolved.

1. **Settings → Configure Log Providers…** chooses which providers to listen to
   and at what verbosity: *None*, *Fatal*, *Error*, *Warning*, *Information* or
   *Verbose*. Providers start at *Information*.

   If the list is empty, nothing has registered itself yet. Run
   **Settings → Register Default Providers** once — it writes the default
   registrations under
   `HKEY_LOCAL_MACHINE\Software\Alex313031\Sawbuck\Providers`, which is
   otherwise the installer's job. This is what makes a portable, non-MSI build
   usable.

2. **Settings → Symbol Path…** sets where symbols are resolved from, if you
   want call stacks resolved to function names rather than addresses.

3. **Log → Capture** (**Ctrl+E**) starts and stops the session. The status bar
   reads *Capturing* while it is live and *Ready* otherwise.

Rows appear as they arrive; the sessions flush about once a second, so expect
up to that much lag behind the program being watched. **Log → Filter…**
(**Ctrl+L**) narrows what is displayed without disturbing the capture.

If another ETW tool already holds either session, Sawbuck says so and offers to
stop it before continuing.

If a session cannot be started, Sawbuck says which one failed and reminds you
about the elevation requirement, which is the usual cause.

### Reading an existing log

**File → Import Log…** (**Ctrl+I**) opens one or more `.etl` files and accepts
a multiple selection. It is disabled while a capture is running, since the two
cannot be mixed.

### Saving what you have

**Log → Export Log** writes every row currently in the view to a `.txt` file,
one row per line with the columns tab separated and a heading row first. If a
filter is set, what you get is what is on screen — the export follows the view,
not the underlying capture.

The file is UTF-8 with a byte order mark, so Notepad and anything else that
reads Unicode will show non-ASCII log text correctly. Tab separation means it
pastes into a spreadsheet as columns.

To copy a few rows instead of all of them, select them and use
**Edit → Copy** (**Ctrl+C**), or right-click a single row and choose
**Copy Row**.

### From the command line

```bat
:: Open and start capturing straight away.
Sawbuck.exe --start-capture

:: Open and import one or more .etl files instead.
Sawbuck.exe --import foo.etl bar.etl
```

## Working with the list

Both lists — the log and the stack trace below it — remember their column
order and widths between runs. Drag a heading to reorder, drag a divider to
resize.

**View → Auto Size Columns** (**F12**) fits every column to its contents. It
needs rows to measure, so it is disabled on an empty log. The same command is
on the right-click menu of the column headings.

Right-clicking a **row** gives a different menu: **Copy Row**, and the commands
that set the time column's zero point, which is useful for reading timings
relative to a particular message rather than to the clock.

**Edit → Find…** (**Ctrl+F**) searches the message text, and **Find Next**
(**F3**) repeats it. **Edit → Clear All** (**Ctrl+N**) empties the view.

## Settings

Everything Sawbuck remembers lives under
`HKEY_CURRENT_USER\Software\Alex313031\Sawbuck`: the window position and size,
the three lists' column layouts, the symbol path, the filters, and the
per-provider log levels.

**Settings → Reset All Settings** clears all of it after asking first. Provider
registrations are not touched — those are under `HKEY_LOCAL_MACHINE` and belong
to the installer. The window and columns keep their current layout for the rest
of the session and start from the defaults next time, because Sawbuck writes
its state back as it closes.

### Symbols over HTTPS on older Windows

Sawbuck makes up a symbol path on a first run, pointing at Microsoft's and
Chromium's public symbol servers. Both require TLS 1.2 now, so the path uses
`https` on Windows 8.1 and later, where a stock install negotiates TLS 1.2 for
a caller that did not ask for it by name — which symsrv, the component that
fetches symbols, does not.

Below 8.1 it checks the registry and uses `https` only if TLS 1.2 is actually
enabled for WinHTTP, falling back to `http`, which those servers still answer.

The line is at 8.1 rather than 8 because Windows 8's WinHTTP needs KB3140245
just as Windows 7's does, even though 8 enables TLS 1.2 in schannel by default.
The assumption is a bare install with its service pack and nothing else, which
is what a VM kept for debugging on legacy Windows usually is.

Every Windows from XP SP3 up *can* do TLS 1.2 with the right update.
`assets/enable_tls12.reg` applies the registry half and names the update each
version needs.

Note that the fabricated path is saved the first time it is worked out, so
enabling TLS 1.2 afterwards does not change it on its own. Set the path by hand
in **Settings → Symbol Path…**, or reset the settings, to pick up `https`. A
symbol path in the `_NT_SYMBOL_PATH` environment variable overrides Sawbuck's
choice entirely.

## Integrating your code with Sawbuck
How to get this for my own code?
If you're using the Chromium base library, then integrating your logging with ETW is as simple as allocating a new [GUID](https://learn.microsoft.com/en-us/windows/win32/api/guiddef/ns-guiddef-guid),
then initializing the ETW log provider like so:

```C++
  #include "base/logging.h"
  #include "base/logging_win.h"
  #include <initguid.h>

  // {????????-????-????-????-????????}
  DEFINE_GUID(kMyLogProvider,
      0x????????,
      0x????,
      0x????,
      0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??);

  void InitLogging() {
    logging::LogEventProvider::Initialize(kMyLogProvider);
  }
```

You will then need to write your provider GUID and name to the registry for Sawbuck to know about it. Create a new key at  
`HKEY_LOCAL_MACHINE\Software\Alex313031\Sawbuck\Providers`  
with your provider GUID as the name of the key, with your provider textual name as the default string value of the key.  
Note: the registry key for upstream Sawbuck was at `Software\Google\Sawbuck` instead.  
Feel free to send a patch to the Sawbuck installer with your changes to the installer's registration to get your provider added to Sawbuck.
If you're not using the base library, you'll need to duplicate what `//base/logging_win.cc` does.

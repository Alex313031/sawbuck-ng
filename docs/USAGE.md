## Usage

Sawbuck watches a live ETW session and shows Chromium's log messages as they
are emitted, or reads an `.etl` file you already have.

## Capturing Chromium Logs

**Sawbuck has to run as administrator to capture.** It starts two ETW sessions:
its own, `Sawbuck Log Session`, and the NT Kernel Logger, which a non-elevated
process cannot start. The kernel session is not optional — it supplies the
image-load and process events that let module names in call stacks be resolved.

1. **Log → Configure Providers…** chooses which providers to listen to and at
   what verbosity: *None*, *Fatal*, *Error*, *Warning*, *Information* or
   *Verbose*. Providers start at *Information*.

   If the list is empty, nothing has registered itself yet. Run
   **Log → Register Default Providers** once — it writes the default
   registrations under
   `HKEY_LOCAL_MACHINE\Software\Alex313031\Sawbuck\Providers`, which is
   otherwise the installer's job. This is what makes a portable, non-MSI build
   usable.

2. **Log → Symbol Path…** sets where symbols are resolved from, if you want
   call stacks resolved to function names rather than addresses.

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

**File → Import Log…** opens one or more `.etl` files and accepts a multiple
selection. It is disabled while a capture is running, since the two cannot be
mixed.

### From the command line

```bat
:: Open and start capturing straight away.
Sawbuck.exe --start-capture

:: Open and import one or more .etl files instead.
Sawbuck.exe --import foo.etl bar.etl
```

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

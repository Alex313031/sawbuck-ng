## TODO

 - clang-format, braces everywhere. Needs a .clang-format first -- winwrap has
   one, plus clang_format.sh, so copy and adjust. Guard the files whose include
   order is load-bearing with `// clang-format off` / `// clang-format on`, the
   way winwrap's src/framework.h does: sawbuck/viewer/framework.h (version
   macros before sdkddkver.h, ATL before WTL), log_list_view.cc and
   provider_dialog.cc (wmistr.h before evntrace.h), viewer_module.cc,
   viewer_window.cc and viewer_unittest_main.cc (initguid.h), and
   kernel_log_consumer.cc (initguid.h before kernel_log_types.h, which must be
   last).
 - convert const to constexpr where appropriate and NULL to nullptr where
   appropriate. 190 NULLs in sawbuck/ and no nullptr yet. Worth doing after
   clang-format so the same lines are not rewritten twice, and watch the
   Win32/ATL boundary: not every NULL there is a pointer.
 - Add more default logging providers like Electron
 - Add help link in Help menu
 - Compiler optimizations
 - "Component build" with base.dll, winwrap.dll and ICU's icuuc.dll and
   icui18n.dll. Note there is no icu54.dll target in M49 -- that was this
   entry's earlier guess.
 - Fully contained build with ICU data integrated? This is the opposite of the
   component build above rather than part of it: build/common.gypi:722 sets
   icu_use_data_file_flag 1, so the data is the external icudtl.dat that
   installer.gyp:117 ships. Setting it to 0 links the data into the binary and
   drops that installer entry.
 - Ensure all DEPS are ACTUALLY at 49.0.2623.112 revisions, whether with
   gclient or sync_deps.py, use patch if necessary.

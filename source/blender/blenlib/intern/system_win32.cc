/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <Windows.h>
#include <commctrl.h>
#include <sstream>

#include <dbghelp.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#include "MEM_guardedalloc.h"

#include "uri_convert.hh"
#include "utfconv.hh"

#include "BLI_string.h"

#include "BLI_system.h" /* Own include. */

static const char *bli_windows_get_exception_description(const DWORD exceptioncode)
{
  switch (exceptioncode) {
    case EXCEPTION_ACCESS_VIOLATION:
      return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:
      return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
      return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
      return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
      return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:
      return "EXCEPTION_FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:
      return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:
      return "EXCEPTION_FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:
      return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:
      return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:
      return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
      return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:
      return "EXCEPTION_INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
      return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:
      return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:
      return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:
      return "EXCEPTION_STACK_OVERFLOW";
      /* This one does not have a known define, but the MSVC runtime raises this for uncaught C++
       * exceptions. See https://devblogs.microsoft.com/oldnewthing/20100730-00/?p=13273 for
       * details. */
    case 0xe06d7363:
      return "Microsoft C++ Exception";
    default:
      return "UNKNOWN EXCEPTION";
  }
}

static void bli_windows_get_module_name(LPVOID address, PCHAR buffer, size_t size)
{
  HMODULE mod;
  buffer[0] = 0;
  if (GetModuleHandleEx(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, static_cast<LPCSTR>(address), &mod))
  {
    if (GetModuleFileName(mod, buffer, size)) {
      PathStripPath(buffer);
    }
  }
}

static void bli_windows_get_module_version(const char *file, char *buffer, size_t buffersize)
{
  buffer[0] = 0;
  DWORD verHandle = 0;
  UINT size = 0;
  LPBYTE lpBuffer = nullptr;
  DWORD verSize = GetFileVersionInfoSize(file, &verHandle);
  if (verSize != 0) {
    LPSTR verData = (LPSTR)MEM_callocN(verSize, "crash module version");

    if (GetFileVersionInfo(file, verHandle, verSize, verData)) {
      if (VerQueryValue(verData, "\\", (VOID FAR * FAR *)&lpBuffer, &size)) {
        if (size) {
          VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
          /* Magic value from
           * https://docs.microsoft.com/en-us/windows/win32/api/verrsrc/ns-verrsrc-vs_fixedfileinfo
           */
          if (verInfo->dwSignature == 0xfeef04bd) {
            BLI_snprintf(buffer,
                         buffersize,
                         "%d.%d.%d.%d",
                         (verInfo->dwFileVersionMS >> 16) & 0xffff,
                         (verInfo->dwFileVersionMS >> 0) & 0xffff,
                         (verInfo->dwFileVersionLS >> 16) & 0xffff,
                         (verInfo->dwFileVersionLS >> 0) & 0xffff);
          }
        }
      }
    }
    MEM_freeN(verData);
  }
}

static void bli_windows_system_backtrace_exception_record(FILE *fp, PEXCEPTION_RECORD record)
{
  char module[MAX_PATH];
  fprintf(fp, "Exception Record:\n\n");
  fprintf(fp,
          "ExceptionCode         : %s (0x%.8x)\n",
          bli_windows_get_exception_description(record->ExceptionCode),
          record->ExceptionCode);
  fprintf(fp, "Exception Address     : 0x%p\n", record->ExceptionAddress);
  bli_windows_get_module_name(record->ExceptionAddress, module, sizeof(module));
  fprintf(fp, "Exception Module      : %s\n", module);
  fprintf(fp, "Exception Flags       : 0x%.8x\n", record->ExceptionFlags);
  fprintf(fp, "Exception Parameters  : 0x%x\n", record->NumberParameters);

  /* Special handling for access violations to make them a little easier to read. */
  if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters == 2) {
    const char *action;
    switch (record->ExceptionInformation[0]) {
      case 0:
        action = "read";
        break;
      case 1:
        action = "write";
        break;
      case 8:
        action = "execute";
        break;
      default:
        action = "unknown";
        break;
    }
    fprintf(fp,
            "\tParameters[0] (action)  : 0x%p (%s)\n",
            (LPVOID *)record->ExceptionInformation[0],
            action);
    fprintf(fp, "\tParameters[1] (address) : 0x%p\n", (LPVOID *)record->ExceptionInformation[1]);
  }
  else {
    for (DWORD idx = 0; idx < record->NumberParameters; idx++) {
      fprintf(fp, "\tParameters[%d] : 0x%p\n", idx, (LPVOID *)record->ExceptionInformation[idx]);
    }
  }
  if (record->ExceptionRecord) {
    fprintf(fp, "Nested ");
    bli_windows_system_backtrace_exception_record(fp, record->ExceptionRecord);
  }
  fprintf(fp, "\n\n");
}

static bool BLI_windows_system_backtrace_run_trace(FILE *fp, HANDLE hThread, PCONTEXT context)
{
  const int max_symbol_length = 100;

  bool result = true;

  PSYMBOL_INFO symbolinfo = static_cast<PSYMBOL_INFO>(
      MEM_callocN(sizeof(SYMBOL_INFO) + max_symbol_length * sizeof(char), "crash Symbol table"));
  symbolinfo->MaxNameLen = max_symbol_length - 1;
  symbolinfo->SizeOfStruct = sizeof(SYMBOL_INFO);

  STACKFRAME frame = {0};
  DWORD machineType = 0;
#if defined(_M_AMD64)
  frame.AddrPC.Offset = context->Rip;
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrFrame.Offset = context->Rsp;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrStack.Offset = context->Rsp;
  frame.AddrStack.Mode = AddrModeFlat;
  machineType = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_ARM64)
  frame.AddrPC.Offset = context->Pc;
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrFrame.Offset = context->Fp;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrStack.Offset = context->Sp;
  frame.AddrStack.Mode = AddrModeFlat;
  machineType = IMAGE_FILE_MACHINE_ARM64;
#endif

  while (true) {
    if (StackWalk64(machineType,
                    GetCurrentProcess(),
                    hThread,
                    &frame,
                    context,
                    nullptr,
                    SymFunctionTableAccess64,
                    SymGetModuleBase64,
                    0))
    {
      if (frame.AddrPC.Offset) {
        char module[MAX_PATH];

        bli_windows_get_module_name((LPVOID)frame.AddrPC.Offset, module, sizeof(module));

        if (SymFromAddr(GetCurrentProcess(), (DWORD64)(frame.AddrPC.Offset), 0, symbolinfo)) {
          fprintf(fp, "%-20s:0x%p  %s", module, (LPVOID)symbolinfo->Address, symbolinfo->Name);
          IMAGEHLP_LINE lineinfo;
          lineinfo.SizeOfStruct = sizeof(lineinfo);
          DWORD displacement = 0;
          if (SymGetLineFromAddr(
                  GetCurrentProcess(), (DWORD64)(frame.AddrPC.Offset), &displacement, &lineinfo))
          {
            fprintf(fp, " %s:%d", lineinfo.FileName, lineinfo.LineNumber);
          }
          fprintf(fp, "\n");
        }
        else {
          fprintf(fp,
                  "%-20s:0x%p  %s\n",
                  module,
                  (LPVOID)frame.AddrPC.Offset,
                  "Symbols not available");
          result = false;
          break;
        }
      }
      else {
        break;
      }
    }
    else {
      break;
    }
  }
  MEM_freeN(symbolinfo);
  fprintf(fp, "\n\n");
  return result;
}

static bool bli_windows_system_backtrace_stack_thread(FILE *fp, HANDLE hThread)
{
  CONTEXT context = {0};
  context.ContextFlags = CONTEXT_ALL;
  /* GetThreadContext requires the thread to be in a suspended state, which is problematic for the
   * currently running thread, RtlCaptureContext is used as an alternative to sidestep this */
  if (hThread != GetCurrentThread()) {
    SuspendThread(hThread);
    bool success = GetThreadContext(hThread, &context);
    ResumeThread(hThread);
    if (!success) {
      fprintf(fp, "Cannot get thread context : 0x0%.8x\n", GetLastError());
      return false;
    }
  }
  else {
    RtlCaptureContext(&context);
  }
  return BLI_windows_system_backtrace_run_trace(fp, hThread, &context);
}

static void bli_windows_system_backtrace_modules(FILE *fp)
{
  fprintf(fp, "Loaded Modules :\n");
  HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
  if (hModuleSnap == INVALID_HANDLE_VALUE) {
    return;
  }

  MODULEENTRY32 me32;
  me32.dwSize = sizeof(MODULEENTRY32);

  if (!Module32First(hModuleSnap, &me32)) {
    CloseHandle(hModuleSnap); /* Must clean up the snapshot object! */
    fprintf(fp, " Error getting module list.\n");
    return;
  }

  do {
    if (me32.th32ProcessID == GetCurrentProcessId()) {
      char version[MAX_PATH];
      bli_windows_get_module_version(me32.szExePath, version, sizeof(version));

      IMAGEHLP_MODULE64 m64;
      m64.SizeOfStruct = sizeof(m64);
      if (SymGetModuleInfo64(GetCurrentProcess(), (DWORD64)me32.modBaseAddr, &m64)) {
        fprintf(fp,
                "0x%p %-20s %s %s %s\n",
                me32.modBaseAddr,
                version,
                me32.szModule,
                m64.LoadedPdbName,
                m64.PdbUnmatched ? "[unmatched]" : "");
      }
      else {
        fprintf(fp, "0x%p %-20s %s\n", me32.modBaseAddr, version, me32.szModule);
      }
    }
  } while (Module32Next(hModuleSnap, &me32));
}

static void bli_windows_system_backtrace_threads(FILE *fp)
{
  fprintf(fp, "Threads:\n");
  HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
  THREADENTRY32 te32;

  hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (hThreadSnap == INVALID_HANDLE_VALUE) {
    fprintf(fp, "Unable to retrieve threads list.\n");
    return;
  }

  te32.dwSize = sizeof(THREADENTRY32);

  if (!Thread32First(hThreadSnap, &te32)) {
    CloseHandle(hThreadSnap);
    return;
  }
  do {
    if (te32.th32OwnerProcessID == GetCurrentProcessId()) {
      if (GetCurrentThreadId() != te32.th32ThreadID) {
        fprintf(fp, "Thread : %.8x\n", te32.th32ThreadID);
        HANDLE ht = OpenThread(THREAD_ALL_ACCESS, FALSE, te32.th32ThreadID);
        bli_windows_system_backtrace_stack_thread(fp, ht);
        CloseHandle(ht);
      }
    }
  } while (Thread32Next(hThreadSnap, &te32));
  CloseHandle(hThreadSnap);
}

static bool bli_windows_system_backtrace_stack(FILE *fp, const EXCEPTION_POINTERS *exception_info)
{
  fprintf(fp, "Stack trace:\n");
  /* If we are handling an exception use the context record from that. */
  if (exception_info && exception_info->ExceptionRecord->ExceptionAddress) {
    /* The back trace code will write to the context record, to protect the original record from
     * modifications give the backtrace a copy to work on. */
    CONTEXT TempContext = *exception_info->ContextRecord;
    return BLI_windows_system_backtrace_run_trace(fp, GetCurrentThread(), &TempContext);
  }
  else {
    /* If there is no current exception or the address is not set, walk the current stack. */
    return bli_windows_system_backtrace_stack_thread(fp, GetCurrentThread());
  }
}

static bool bli_private_symbols_loaded()
{
  IMAGEHLP_MODULE64 m64;
  m64.SizeOfStruct = sizeof(m64);
  if (SymGetModuleInfo64(GetCurrentProcess(), (DWORD64)GetModuleHandle(nullptr), &m64)) {
    return m64.GlobalSymbols;
  }
  return false;
}

static void bli_load_symbols()
{
  /* If this is a developer station and the private pdb is already loaded leave it be. */
  if (bli_private_symbols_loaded()) {
    return;
  }

  char pdb_file[MAX_PATH] = {0};

  /* get the currently executing image */
  if (GetModuleFileNameA(nullptr, pdb_file, sizeof(pdb_file))) {
    /* remove the filename */
    PathRemoveFileSpecA(pdb_file);
    /* append blender.pdb */
    PathAppendA(pdb_file, "blender.pdb");
    if (PathFileExistsA(pdb_file)) {
      HMODULE mod = GetModuleHandle(nullptr);
      if (mod) {
        WIN32_FILE_ATTRIBUTE_DATA file_data;
        if (GetFileAttributesExA(pdb_file, GetFileExInfoStandard, &file_data)) {
          /* SymInitialize will try to load symbols on its own, so we first must unload whatever it
           * did trying to help */
          SymUnloadModule64(GetCurrentProcess(), (DWORD64)mod);

          DWORD64 module_base = SymLoadModule(GetCurrentProcess(),
                                              nullptr,
                                              pdb_file,
                                              nullptr,
                                              (DWORD64)mod,
                                              (DWORD)file_data.nFileSizeLow);
          if (module_base == 0) {
            fprintf(stderr,
                    "Error loading symbols %s\n\terror:0x%.8x\n\tsize = %d\n\tbase=0x%p\n",
                    pdb_file,
                    GetLastError(),
                    file_data.nFileSizeLow,
                    (LPVOID)mod);
          }
        }
      }
    }
  }
}

void BLI_system_backtrace_with_os_info(FILE *fp, const void *os_info)
{
  const EXCEPTION_POINTERS *exception_info = static_cast<const EXCEPTION_POINTERS *>(os_info);
  SymInitialize(GetCurrentProcess(), nullptr, TRUE);
  bli_load_symbols();
  if (exception_info) {
    bli_windows_system_backtrace_exception_record(fp, exception_info->ExceptionRecord);
  }
  if (bli_windows_system_backtrace_stack(fp, exception_info)) {
    /* When the blender symbols are missing the stack traces will be unreliable
     * so only run if the previous step completed successfully. */
    bli_windows_system_backtrace_threads(fp);
  }
  bli_windows_system_backtrace_modules(fp);
}

void BLI_windows_exception_print_message(const void *os_info)
{
  if (!os_info) {
    return;
  }

  const EXCEPTION_POINTERS *exception = static_cast<const EXCEPTION_POINTERS *>(os_info);
  const char *exception_name = bli_windows_get_exception_description(
      exception->ExceptionRecord->ExceptionCode);
  LPVOID address = exception->ExceptionRecord->ExceptionAddress;
  CHAR modulename[MAX_PATH];
  bli_windows_get_module_name(address, modulename, sizeof(modulename));
  DWORD threadId = GetCurrentThreadId();

  char message[512];
  BLI_snprintf(message,
               512,
               "Error   : %s\n"
               "Address : 0x%p\n"
               "Module  : %s\n"
               "Thread  : %.8x\n",
               exception_name,
               address,
               modulename,
               threadId);

  fprintf(stderr, message);
  fflush(stderr);
}

/* -------------------------------------------------------------------- */
/** \name bli_show_message_box
 * \{ */

static std::string get_os_info()
{
  OSVERSIONINFOEX osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (!GetVersionEx((OSVERSIONINFO *)&osvi)) {
    return "Unknown System";
  }

  std::string version = std::to_string(osvi.dwMajorVersion) + "-" +
                        std::to_string(osvi.dwMajorVersion) + "." +
                        std::to_string(osvi.dwMinorVersion) + "." +
                        std::to_string(osvi.dwBuildNumber) + "-SP" +
                        std::to_string(osvi.wServicePackMajor);

  SYSTEM_INFO si;
  GetSystemInfo(&si);
  std::string architecture;
  switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
      architecture = "64 Bits";
      break;
    case PROCESSOR_ARCHITECTURE_INTEL:
      architecture = "32 Bits";
      break;
    case PROCESSOR_ARCHITECTURE_ARM:
      architecture = "ARM Architecture";
      break;
    case PROCESSOR_ARCHITECTURE_ARM64:
      architecture = "ARM64 Architecture";
      break;
    case PROCESSOR_ARCHITECTURE_ARM32_ON_WIN64:
      architecture = "ARM32 on Windows 64-bit";
      break;
    case PROCESSOR_ARCHITECTURE_IA32_ON_ARM64:
      architecture = "IA32 on ARM64";
      break;
    default:
      architecture = "Unknown Architecture";
  }

  return "Windows-" + version + " " + architecture;
}

/**
 * Retrieve the path to "blender-launcher.exe" if it exists; otherwise, return the current
 * executable path.
 */
static bool bli_executable_path_get(LPWSTR path, DWORD size)
{
  wchar_t executable_path[MAX_PATH];
  DWORD nSize = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
  if (nSize == 0 || nSize == MAX_PATH) {
    return false;
  }

  if (size <= nSize) {
    return false;
  }

  /* Copy the path to the output buffer. */
  if (wcscpy_s(path, size, executable_path) != 0) {
    return false;
  }

  /* Replace the filename "blender.exe" with "blender-launcher.exe". */
  if (!PathRemoveFileSpecW(executable_path)) {
    /* Failed to remove the file spec. Use the original path. */
    return true;
  }
  if (!PathAppendW(executable_path, L"blender-launcher.exe")) {
    /* Failed to append the new filename. Use the original path. */
    return true;
  }

  /* Check if "blender-launcher.exe" exists at this path. */
  DWORD attributes = GetFileAttributesW(executable_path);
  if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
    /* "blender-launcher.exe" does not exist. Use the original executable path. */
    return true;
  }

  if (wcslen(executable_path) + 1 > size) {
    /* The output buffer is not large enough for the new path. Use the original path. */
    return true;
  }

  /* The file exists. Copy the path to the output buffer. */
  if (wcscpy_s(path, size, executable_path) != 0) {
    /* Error: It's not supposed to happen. Return false since the buffer has been modified. */
    return false;
  }

  return true;
}

/* Wrapper function for url_encode. */
static std::wstring url_encode_wstring(const std::string &str)
{
  size_t len = str.length();

  /* Maximum encoded length is 3 times the original length +1 for null terminator. */
  size_t encoded_len_max = len * 3 + 1;

  char *encoded_str = new char[encoded_len_max];
  url_encode(str.c_str(), encoded_str, encoded_len_max);

  /* Convert the encoded char *to a std::wstring (assuming the encoded string is ASCII). */
  std::wstring result(encoded_str, encoded_str + strlen(encoded_str));

  delete[] encoded_str;

  return result;
}

void BLI_windows_exception_show_dialog(const char *filepath_crashlog,
                                       const char *filepath_relaunch,
                                       const char *gpu_name,
                                       const char *build_version)
{
  /* Redundant: #InitCommonControls is already called during GHOST System initialization. */
  // InitCommonControls();

  /* Convert file paths to UTF16 to handle non-ASCII characters. */
  wchar_t *filepath_crashlog_utf16 = alloc_utf16_from_8(filepath_crashlog, 0);
  wchar_t *filepath_relaunch_utf16 = filepath_relaunch[0] ?
                                         alloc_utf16_from_8(filepath_relaunch, 0) :
                                         nullptr;

  std::wstring full_message_16 =
      L"A problem has caused the program to stop functioning correctly. If you know the steps to "
      L"reproduce this issue, please submit a bug report.\n"
      "\n"
      L"The crash log can be found at:\n" +
      std::wstring(filepath_crashlog_utf16);

  TASKDIALOGCONFIG config = {0};
  const TASKDIALOG_BUTTON buttons[] = {{IDRETRY, L"Restart"},
#if 0
    /* This lead to a large influx of low quality reports on the tracker,
     * and has been disabled for that reason, we can re-enable this when
     * a better workflow has been established. */
    {IDOK, L"Report a Bug"},
#endif
                                       {IDHELP, L"View Crash Log"},
                                       {IDCLOSE, L"Close"}};

  config.cbSize = sizeof(config);
  config.hwndParent = GetActiveWindow();
  config.hInstance = 0;
  config.dwCommonButtons = 0;
  config.pszMainIcon = TD_ERROR_ICON;
  config.pszWindowTitle = L"Blender";
  config.pszMainInstruction = L"Blender has stopped working";
  config.pszContent = full_message_16.c_str();
  config.pButtons = buttons;
  config.cButtons = ARRAY_SIZE(buttons);

  /* Data passed to the callback function for handling button events. */
  const struct Data {
    const wchar_t *filepath_crashlog_utf16;
    const wchar_t *filepath_relaunch_utf16;
    const char *gpu_name;
    const char *build_version;
  } data = {filepath_crashlog_utf16, filepath_relaunch_utf16, gpu_name, build_version};
  config.lpCallbackData = reinterpret_cast<LONG_PTR>(&data);

  /* Callback for handling button events. */
  config.pfCallback = [](HWND /*hwnd*/,
                         UINT uNotification,
                         WPARAM wParam,
                         LPARAM /*lParam*/,
                         LONG_PTR dwRefData) -> HRESULT {
    const Data *data_ptr = reinterpret_cast<const Data *>(dwRefData);
    if (uNotification != TDN_BUTTON_CLICKED) {
      return S_OK;
    }
    int pnButton = static_cast<int>(wParam);
    switch (pnButton) {
      case IDCLOSE:
        return S_OK;
      case IDRETRY: {
        /* Relaunch the application. */
        wchar_t executable_path[MAX_PATH];
        if (bli_executable_path_get(executable_path, ARRAYSIZE(executable_path))) {
          std::wstring parameters;
          if (data_ptr->filepath_relaunch_utf16) {
            /* Properly quote the argument to handle spaces and special characters. */
            parameters = L"\"" + std::wstring(data_ptr->filepath_relaunch_utf16) + L"\"";
          }
          else {
            /* Proceeding without parameters. */
            parameters = L"";
          }
          ShellExecuteW(
              nullptr, L"open", executable_path, parameters.c_str(), nullptr, SW_SHOWNORMAL);
        }
        return S_OK;
      }
      case IDHELP:
        /* Open the crash log. */
        ShellExecuteW(
            nullptr, L"open", data_ptr->filepath_crashlog_utf16, nullptr, nullptr, SW_SHOWNORMAL);
        return S_FALSE;
      case IDOK: {
        /* Open the bug report form with pre-filled data. */
        /* clang-format off */
        std::wstring link =
            L"https://redirect.blender.org/"
            L"?type=bug_report"
            L"&project=blender"
            L"&os=" + url_encode_wstring(get_os_info()) +
            L"&gpu=" + url_encode_wstring(data_ptr->gpu_name) +
            L"&broken_version=" + url_encode_wstring(data_ptr->build_version);
        /* clang-format on */
        ShellExecuteW(nullptr, L"open", link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return S_FALSE;
      }
      default:
        return S_FALSE;
    }
  };

  TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
  free((void *)filepath_crashlog_utf16);
  free((void *)filepath_relaunch_utf16);
}

/** \} */

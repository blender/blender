/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */
#include <Windows.h>
#include <stdio.h>

#include <dbghelp.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#include "BLI_string.h"

#include "MEM_guardedalloc.h"

static EXCEPTION_POINTERS *current_exception = NULL;

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
    default:
      return "UNKNOWN EXCEPTION";
  }
}

static void bli_windows_get_module_name(LPVOID address, PCHAR buffer, size_t size)
{
  HMODULE mod;
  buffer[0] = 0;
  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, address, &mod)) {
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
  LPBYTE lpBuffer = NULL;
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
          "ExceptionCode         : %s\n",
          bli_windows_get_exception_description(record->ExceptionCode));
  fprintf(fp, "Exception Address     : 0x%p\n", record->ExceptionAddress);
  bli_windows_get_module_name(record->ExceptionAddress, module, sizeof(module));
  fprintf(fp, "Exception Module      : %s\n", module);
  fprintf(fp, "Exception Flags       : 0x%.8x\n", record->ExceptionFlags);
  fprintf(fp, "Exception Parameters  : 0x%x\n", record->NumberParameters);
  for (DWORD idx = 0; idx < record->NumberParameters; idx++) {
    fprintf(fp, "\tParameters[%d] : 0x%p\n", idx, (LPVOID *)record->ExceptionInformation[idx]);
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

  PSYMBOL_INFO symbolinfo = MEM_callocN(sizeof(SYMBOL_INFO) + max_symbol_length * sizeof(char),
                                        "crash Symbol table");
  symbolinfo->MaxNameLen = max_symbol_length - 1;
  symbolinfo->SizeOfStruct = sizeof(SYMBOL_INFO);

  STACKFRAME frame = {0};
  frame.AddrPC.Offset = context->Rip;
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrFrame.Offset = context->Rsp;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrStack.Offset = context->Rsp;
  frame.AddrStack.Mode = AddrModeFlat;

  while (true) {
    if (StackWalk64(IMAGE_FILE_MACHINE_AMD64,
                    GetCurrentProcess(),
                    hThread,
                    &frame,
                    context,
                    NULL,
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
  if (hModuleSnap == INVALID_HANDLE_VALUE)
    return;

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

static bool BLI_windows_system_backtrace_stack(FILE *fp)
{
  fprintf(fp, "Stack trace:\n");
  /* If we are handling an exception use the context record from that. */
  if (current_exception && current_exception->ExceptionRecord->ExceptionAddress) {
    /* The back trace code will write to the context record, to protect the original record from
     * modifications give the backtrace a copy to work on. */
    CONTEXT TempContext = *current_exception->ContextRecord;
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
  if (SymGetModuleInfo64(GetCurrentProcess(), (DWORD64)GetModuleHandle(NULL), &m64)) {
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
  if (GetModuleFileNameA(NULL, pdb_file, sizeof(pdb_file))) {
    /* remove the filename */
    PathRemoveFileSpecA(pdb_file);
    /* append blender.pdb */
    PathAppendA(pdb_file, "blender.pdb");
    if (PathFileExistsA(pdb_file)) {
      HMODULE mod = GetModuleHandle(NULL);
      if (mod) {
        WIN32_FILE_ATTRIBUTE_DATA file_data;
        if (GetFileAttributesExA(pdb_file, GetFileExInfoStandard, &file_data)) {
          /* SymInitialize will try to load symbols on its own, so we first must unload whatever it
           * did trying to help */
          SymUnloadModule64(GetCurrentProcess(), (DWORD64)mod);

          DWORD64 module_base = SymLoadModule(GetCurrentProcess(),
                                              NULL,
                                              pdb_file,
                                              NULL,
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

/**
 * Write a backtrace into a file for systems which support it.
 */
void BLI_system_backtrace(FILE *fp)
{
  SymInitialize(GetCurrentProcess(), NULL, TRUE);
  bli_load_symbols();
  if (current_exception) {
    bli_windows_system_backtrace_exception_record(fp, current_exception->ExceptionRecord);
  }
  if (BLI_windows_system_backtrace_stack(fp)) {
    /* When the blender symbols are missing the stack traces will be unreliable
     * so only run if the previous step completed successfully. */
    bli_windows_system_backtrace_threads(fp);
  }
  bli_windows_system_backtrace_modules(fp);
}

void BLI_windows_handle_exception(EXCEPTION_POINTERS *exception)
{
  current_exception = exception;
  if (current_exception) {
    fprintf(stderr,
            "Error   : %s\n",
            bli_windows_get_exception_description(exception->ExceptionRecord->ExceptionCode));
    fflush(stderr);

    LPVOID address = exception->ExceptionRecord->ExceptionAddress;
    fprintf(stderr, "Address : 0x%p\n", address);

    CHAR modulename[MAX_PATH];
    bli_windows_get_module_name(address, modulename, sizeof(modulename));
    fprintf(stderr, "Module  : %s\n", modulename);
    fprintf(stderr, "Thread  : %.8x\n", GetCurrentThreadId());
  }
  fflush(stderr);
}

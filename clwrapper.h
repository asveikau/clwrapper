/*
 * Copyright (c) 2017 Andrew Sveikauskas
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef clwrapper_h
#define clwrapper_h

#include <windows.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _OUTPUT_STRING
{
   PWCHAR Buffer;
   SIZE_T AllocatedBytes;
   SIZE_T Length;
} OUTPUT_STRING, *POUTPUT_STRING;

typedef struct _STRING_LIST
{
   struct _STRING_LIST *Next;
   WCHAR String[0];
} STRING_LIST, *PSTRING_LIST;

typedef struct _CLWRAPPER_VERSION_SPEC
{
   BOOL Specified;
   WORD DesiredMajor, DesiredMinor;
} CLWRAPPER_VERSION_SPEC, *PCLWRAPPER_VERSION_SPEC;

typedef struct _CLWRAPPER_ARGS_BASE
{
   CLWRAPPER_VERSION_SPEC CompilerVersion;
   CLWRAPPER_VERSION_SPEC SdkVersion;
   PCWSTR DesiredArchitecture;
   BOOL StaticCrt;
   PSTRING_LIST LibraryPaths;
   PSTRING_LIST Libraries;
} CLWRAPPER_ARGS_BASE, *PCLWRAPPER_ARGS_BASE;

typedef struct _CC_ARGS
{
   CLWRAPPER_ARGS_BASE Base;
   enum
   {
      CC_EXECUTABLE,
      CC_SHARED_LIBRARY,
      CC_OBJECT_FILE
   } OutputType;
   PCWSTR OutputName;
   WCHAR Optimization;
   BOOL Wall;
   BOOL Werror;
   BOOL DisableRtti;
   PSTRING_LIST Macros;
   PSTRING_LIST IncludePaths;
   PSTRING_LIST LinkerOptions;
   PSTRING_LIST Inputs;
} CC_ARGS, *PCC_ARGS;

typedef struct _VS_VERSION
{
   WORD Major, Minor;
   PWSTR InstallDir;
   PSTRING_LIST Configurations;
   PSTRING_LIST ClPaths;
   struct _VS_VERSION *Next;
} VS_VERSION, *PVS_VERSION;

typedef struct _ARCHITECTURE
{
   PCWSTR ClFile;
   PCWSTR ConfigurationName;
   PCWSTR ClArchName;
   PCWSTR SdkArchName;
} ARCHITECTURE, *PARCHITECTURE;

const ARCHITECTURE *
FindArchByConfiguration(PCWSTR ConfigurationName);

HRESULT
BaseParseArg(
   PCLWRAPPER_ARGS_BASE Context,
   PWSTR *Arg,
   INT *NumConsumedOut
);

VOID
BaseArgsFree(
   PCLWRAPPER_ARGS_BASE Context
);

HRESULT
CcParseArg(
   PCC_ARGS Context,
   PWSTR *Arg,
   INT *NumConsumedOut
);

VOID
CcArgsFree(
   PCC_ARGS Context
);

HRESULT
StringListAlloc(
   DWORD NChars,
   PSTRING_LIST Next,
   PSTRING_LIST *Output
);

HRESULT
StringListAllocString(
   PCWSTR String,
   PSTRING_LIST Next,
   PSTRING_LIST *Output
);

VOID
StringListReverse(
   PSTRING_LIST *Head
);

VOID
FreeStringList(
   PSTRING_LIST List
);

HRESULT
AllocateString(
   DWORD NumChars,
   POUTPUT_STRING String,
   PWSTR *Output
);

HRESULT
AppendString(
   PCWSTR Src,
   POUTPUT_STRING Dst
);

VOID
FreeString(
   POUTPUT_STRING Str
);

HRESULT
GetInstalledVsVersions(
   PVS_VERSION *Out
);

HRESULT
GetInstalledSdks(
   BOOL WinCE,
   PVS_VERSION *Out
);

HRESULT
FindToolset(
   PCLWRAPPER_ARGS_BASE Args,
   PVS_VERSION *AvailableCompilers,
   PVS_VERSION *AvailableSdks
);

VOID
FreeVsVersions(
   PVS_VERSION Version
);

HRESULT
HeapPrintf(
   PWSTR *Output,
   PCWSTR Fmt,
   ...
);

HRESULT
GetStringValue(
   HKEY Key,
   PCWSTR ValueName,
   PWSTR *Out
);

HRESULT
EnumKey(
   PVOID Context,
   HKEY Parent,
   PCWSTR KeyName,
   HRESULT (*Fn)(PVOID Context, HKEY Key, PCWSTR ChildKey)
);

HRESULT
AddToPath(
   PCWSTR Path
);

HRESULT
LaunchProcess(
   PCWSTR CommandLine,
   PDWORD ExitCode
);

#if defined(__cplusplus)
}
#endif
#endif

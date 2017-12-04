/*
 * Copyright (c) 2017 Andrew Sveikauskas
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "clwrapper.h"
#include <stdlib.h>
#include <stdio.h>
#include <intsafe.h>

#if _M_IX86
#define WOW64NODE
#else
#define WOW64NODE L"Wow6432Node\\"
#endif

static HRESULT
SortByVersion(
   PVS_VERSION *Versions
);

static const ARCHITECTURE
Arches[] =
{
   {L"..\\..\\VC\\ce\\bin\\x86_arm\\cl.exe", L"ce",    L"arm",   L"arm"},
   {L"..\\..\\VC\\bin\\x86_arm\\cl.exe",     L"woa",   L"arm",   L"arm"},
   {L"..\\..\\VC\\bin\\x86_amd64\\cl.exe",   L"amd64", L"amd64", L"x64"},
   {L"..\\..\\VC\\bin\\cl.exe",              L"32",    NULL,     NULL},
   {NULL, NULL, NULL, NULL}
};

const ARCHITECTURE *
FindArchByConfiguration(
   PCWSTR Config
)
{
   const ARCHITECTURE *r = NULL;

   if (Config)
   {
      r = Arches;

      while (r->ConfigurationName)
      {
         if (!wcscmp(r->ConfigurationName, Config))
            break;
         ++r;
      }
   }

   return r;
}

static HRESULT
ProbeVsVersion(
   PVOID Context,
   HKEY Parent,
   PCWSTR KeyName
)
{
   PVS_VERSION *Out = Context;
   HRESULT hr = E_OUTOFMEMORY;
   PVS_VERSION Current = malloc(sizeof(*Current));
   HKEY Key = NULL;

   if (Current)
   {
      memset(Current, 0, sizeof(*Current));

      hr = S_OK;
   }

   if (SUCCEEDED(hr))
   {
      DWORD Result = 0;

      Result = RegOpenKeyEx(
         Parent,
         KeyName,
         0,
         KEY_QUERY_VALUE,
         &Key
      );

      if (Result)
         hr = HRESULT_FROM_WIN32(Result);
   }

   // Works for VS2008, VS2010...
   //
   if (SUCCEEDED(hr))
   {
      hr = GetStringValue(Key, L"InstallDir", &Current->InstallDir);
      if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
      {
         hr = S_OK;
      }
   }

   // Works for VS2012...
   //
   if (SUCCEEDED(hr) && !Current->InstallDir)
   {
      HKEY ChildKey = NULL;
      PWSTR ProductDir = NULL;
      DWORD Result = 0;

      Result = RegOpenKeyEx(
         Key,
         L"Setup\\VS",
         0,
         KEY_QUERY_VALUE,
         &ChildKey
      );
      if (Result)
         hr = HRESULT_FROM_WIN32(Result);

      if (SUCCEEDED(hr))
         hr = GetStringValue(ChildKey, L"ProductDir", &ProductDir);

      if (SUCCEEDED(hr))
      {
         hr = HeapPrintf(
            &Current->InstallDir,
            L"%s\\Common7\\IDE",
            ProductDir
         );
      }

      if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
      {
         hr = S_OK;
      }

      if (ChildKey)
         RegCloseKey(ChildKey);
      free(ProductDir);
   }

   if (SUCCEEDED(hr) &&
       Current->InstallDir)
   {
      const ARCHITECTURE *Dt = Arches;

      while (Dt->ClFile)
      {
         PWSTR File = NULL;
         hr = HeapPrintf(&File, L"%s\\%s", Current->InstallDir, Dt->ClFile);
         if (FAILED(hr))
            break;

         if (GetFileAttributes(File) != INVALID_FILE_ATTRIBUTES)
         {
            hr = StringListAllocString(
               Dt->ConfigurationName,
               Current->Configurations,
               &Current->Configurations
            );
            if (SUCCEEDED(hr))
            {
               hr = StringListAllocString(
                  File,
                  Current->ClPaths,
                  &Current->ClPaths
               );
            }
         }

         free(File);
         ++Dt;
      }
   }

   if (SUCCEEDED(hr) &&
       Current->InstallDir &&
       (swscanf(KeyName, L"%hd.%hd", &Current->Major, &Current->Minor) == 2))
   {
      Current->Next = *Out;
      *Out = Current;
      Current = NULL;
   }

   if (Key)
      RegCloseKey(Key);
   FreeVsVersions(Current);
   return hr;
}

HRESULT
GetInstalledVsVersions(
   PVS_VERSION *Out
)
{
   HRESULT hr = S_OK;

   hr = EnumKey(
      Out,
      HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\" WOW64NODE L"Microsoft\\VisualStudio",
      ProbeVsVersion
   );
   if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
   {
      hr = S_OK;
   }

   if (SUCCEEDED(hr))
   {
      hr = SortByVersion(Out);
   }

   if (FAILED(hr))
   {
      FreeVsVersions(*Out);
      *Out = NULL;
   }

   return hr; 
}

static HRESULT
ProbeSdkVersion(
   PVOID Context,
   HKEY Parent,
   PCWSTR KeyName
)
{
   HRESULT hr = S_OK;
   PVS_VERSION *Out = Context;
   HKEY Key = NULL;
   PVS_VERSION Current = NULL;

   if (SUCCEEDED(hr))
   {
      DWORD Result = 0;

      Result = RegOpenKeyEx(
         Parent,
         KeyName,
         0,
         KEY_QUERY_VALUE,
         &Key
      );

      if (Result)
         hr = HRESULT_FROM_WIN32(Result);
   }

   if (SUCCEEDED(hr))
   {
      Current = malloc(sizeof(*Current));
      if (!Current)
         hr = E_OUTOFMEMORY;
      else
         memset(Current, 0, sizeof(*Current));
   }

   if (SUCCEEDED(hr))
   {
      hr = GetStringValue(Key, L"InstallationFolder", &Current->InstallDir);
      if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
      {
         hr = S_OK;
      }
   }

   if (SUCCEEDED(hr) &&
       Current->InstallDir &&
       (swscanf(KeyName, L"v%hd.%hd", &Current->Major, &Current->Minor) == 2))
   {
      Current->Next = *Out;
      *Out = Current;
      Current = NULL;
   }

   if (Key)
      RegCloseKey(Key);
   FreeVsVersions(Current);
   return hr;
}

HRESULT
GetInstalledSdks(
   BOOL WinCE,
   PVS_VERSION *Out
)
{
   HRESULT hr = S_OK;

   if (WinCE)
   {
      // TODO
      return hr;
   }

   hr = EnumKey(
      Out,
      HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\" WOW64NODE L"Microsoft\\Microsoft SDKs\\Windows",
      ProbeSdkVersion 
   );
   if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
   {
      hr = S_OK;
   }

   if (SUCCEEDED(hr))
   {
      hr = SortByVersion(Out);
   }

   if (FAILED(hr))
   {
      FreeVsVersions(*Out);
      *Out = NULL;
   }

   return hr;
}

static DWORD
ParseVersion(const VS_VERSION **VsVersion)
{
   DWORD r = (*VsVersion)->Major;
   r <<= 16;
   r |= (*VsVersion)->Minor;
   return r;
}

static INT
VersionCmp(
   const void *a, const void *b
)
{
   //
   // Sort higher versions first.
   //
   DWORD aa = ParseVersion(a), bb = ParseVersion(b);
   if (aa > bb)
      return -1;
   else if (aa < bb)
      return 1;
   else
      return 0;
}

static HRESULT
SortByVersion(
   PVS_VERSION *Versions
)
{
   HRESULT hr = S_OK;

   if (*Versions && (*Versions)->Next)
   {
      PVS_VERSION Node;
      PVS_VERSION *Nodes;
      DWORD Count;
      DWORD CountBytes;

      for (Node = *Versions, Count = 0; Node; Node = Node->Next)
      {
         ++Count;
      }

      hr = DWordMult(Count, sizeof(*Nodes), &CountBytes);
      if (SUCCEEDED(hr))
      {
         Nodes = malloc(CountBytes);
         if (!Nodes)
            hr = E_OUTOFMEMORY;
      }

      if (SUCCEEDED(hr))
      {
         DWORD Idx;

         for (Node = *Versions, Idx = 0; Node; Node = Node->Next)
         {
            Nodes[Idx++] = Node;
         }

         qsort(Nodes, Count, sizeof(*Nodes), VersionCmp);

         for (Idx = 0; Idx < Count; ++Idx)
         {
            Nodes[Idx]->Next = ((Idx + 1) < Count) ? Nodes[Idx+1] : NULL;
         }

         *Versions = Nodes[0];
         free(Nodes);
      }
   }

   return hr;
}

VOID
FreeVsVersions(
   PVS_VERSION Version
)
{
   while (Version)
   {
      PVS_VERSION Next = Version->Next;

      free(Version->InstallDir);
      FreeStringList(Version->Configurations);
      FreeStringList(Version->ClPaths);
      free(Version);

      Version = Next;
   }
}

static BOOL
ContainsConfiguration(
   PVS_VERSION Version,
   PCWSTR DesiredName
)
{
   PSTRING_LIST Configuration = Version->Configurations;

   while (Configuration)
   {
      if (!wcscmp(Configuration->String, DesiredName))
      {
         return TRUE;
      }
 
      Configuration = Configuration->Next;
   }

   return FALSE;
}

static VOID
RemoveIf(
   PVS_VERSION *Head,
   BOOL (*Fn)(PVOID, PVS_VERSION),
   PVOID FnArg
)
{
   PVS_VERSION Current = *Head;

   while (Current)
   {
      PVS_VERSION Next = Current->Next;

      if (Fn(FnArg, Current))
      {
         *Head = Next;
         Current->Next = NULL;
         FreeVsVersions(Current);
      }
      else
      {
         Head = &Current->Next;
      }

      Current = Next;
   }
} 

static BOOL
MatchConfiguration(
   PVOID Configuration,
   PVS_VERSION Version
)
{
   PSTRING_LIST *ConfigHead = &Version->Configurations;
   PSTRING_LIST *ClPathHead = &Version->ClPaths;
   PSTRING_LIST Config = *ConfigHead;
   PSTRING_LIST ClPath = *ClPathHead;
   BOOL Found = FALSE;

   while (Config)
   {
      PSTRING_LIST NextConfig = Config->Next;
      PSTRING_LIST NextCl = ClPath->Next;

      if (!Found &&
          !wcscmp(Config->String, Configuration))
      {
         // Keep this one...
         //
         ConfigHead = &Config->Next;
         ClPathHead = &ClPath->Next;
         Found = TRUE;
      }
      else
      {
         *ConfigHead = NextConfig;
         *ClPathHead = NextCl;
         free(Config);
         free(ClPath);
      }

      Config = NextConfig;
      ClPath = NextCl;
   }

   return !Found;
}

static BOOL
MatchVersionSpec(
   PVOID VersionSpecp,
   PVS_VERSION Version
)
{
   PCLWRAPPER_VERSION_SPEC VersionSpec = VersionSpecp;
   return (VersionSpec->DesiredMajor != Version->Major ||
           VersionSpec->DesiredMinor != Version->Minor);
}

HRESULT
FindToolset(
   PCLWRAPPER_ARGS_BASE Args,
   PVS_VERSION *CompilersOut,
   PVS_VERSION *SdksOut
)
{
   HRESULT hr = S_OK;
   PVS_VERSION Compilers = NULL;
   PVS_VERSION Sdks = NULL;
   PCWSTR Arch = Args->DesiredArchitecture;

   hr = GetInstalledVsVersions(&Compilers);

   if (SUCCEEDED(hr) &&
       Arch)
   {
      RemoveIf(&Compilers, MatchConfiguration, (PVOID)Arch);

      if (!Compilers)
      {
         fprintf(stderr, "No compiler found to match -m%ls\n", Arch);
         hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
      }
   }

   if (SUCCEEDED(hr) &&
       Args->CompilerVersion.Specified)
   {
      RemoveIf(&Compilers, MatchVersionSpec, &Args->CompilerVersion);

      if (!Compilers)
      {
         DWORD Major = Args->CompilerVersion.DesiredMajor;
         DWORD Minor = Args->CompilerVersion.DesiredMinor;

         if (Arch)
            fprintf(stderr, "Could not find compiler v%d.%d with -m%ls\n",
                    Major, Minor, Arch);
         else
            fprintf(stderr, "Could not find compiler v%d.%d\n", Major, Minor);

         hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
      }
   }

   if (SUCCEEDED(hr))
   {
      hr = GetInstalledSdks(
         Arch && !wcscmp(Arch, L"ce"),
         &Sdks
      );
   }

   if (SUCCEEDED(hr) &&
       Args->SdkVersion.Specified)
   {
      RemoveIf(&Sdks, MatchVersionSpec, &Args->SdkVersion);

      if (!Sdks)
      {
         DWORD Major = Args->SdkVersion.DesiredMajor;
         DWORD Minor = Args->SdkVersion.DesiredMinor;

         fprintf(stderr, "Could not find SDK v%d.%d\n", Major, Minor);

         hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
      }
   }

   if (SUCCEEDED(hr) &&
       !Compilers)
   {
      fprintf(stderr, "No applicable compiler found.\n");
      hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
   }

   if (SUCCEEDED(hr) &&
       !Sdks)
   {
      fprintf(stderr, "No applicable SDK found.\n");
      hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
   }

   if (SUCCEEDED(hr))
   {
      *CompilersOut = Compilers;
      *SdksOut = Sdks;
      Compilers = Sdks = NULL;
   }
   FreeVsVersions(Compilers);
   FreeVsVersions(Sdks);
   return hr;
}

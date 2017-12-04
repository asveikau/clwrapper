/*
 * Copyright (c) 2017 Andrew Sveikauskas
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "clwrapper.h"
#include <stdio.h>
#include <intsafe.h>

static HRESULT
CcParseArgs(
   PCC_ARGS Args,
   PWSTR *CurrentArg
);

static HRESULT
FindInterestingWin10Version(
   PCWSTR SdkInstallDir,
   PWSTR *Result
);

static HRESULT
CcMain(
   INT Argc,
   PWSTR *Argv,
   PDWORD ReturnValue
)
{
   HRESULT hr = S_OK;
   CC_ARGS Args = {0};
   OUTPUT_STRING CommandLine = {0};
   PSTRING_LIST List = NULL;
   PVS_VERSION CompilerInfo = NULL;
   PVS_VERSION SdkInfo = NULL;
   BOOL Link = TRUE;

   hr = CcParseArgs(&Args, Argv + 1);

   // Pick a compiler and SDK...
   //
   if (SUCCEEDED(hr))
   {
      hr = FindToolset(&Args.Base, &CompilerInfo, &SdkInfo);
      if (FAILED(hr))
      {
         fprintf(stderr, "Failure to locate VS tools!\n");
      }
   }

   // SDK and compiler specific includes...
   //
   if (SUCCEEDED(hr))
   {
      PWSTR Includes[32], *p = Includes;
      PWSTR SdkVersion = NULL;

      if (SdkInfo->Major == 10)
      {
         *p++ = L"%s\\include\\%s\\shared";
         *p++ = SdkInfo->InstallDir;
         *p++ = L"%s\\include\\%s\\ucrt";
         *p++ = SdkInfo->InstallDir;
         *p++ = L"%s\\include\\%s\\um";
         *p++ = SdkInfo->InstallDir;
         *p++ = L"%s\\include\\%s\\winrt";
         *p++ = SdkInfo->InstallDir;

         hr = FindInterestingWin10Version(SdkInfo->InstallDir, &SdkVersion);
      }
      else
      {
         *p++ = L"%s\\include";
         *p++ = SdkInfo->InstallDir;
      }

      *p++ = L"%s\\..\\..\\VC\\include";
      *p++ = CompilerInfo->InstallDir;

      *p = NULL;

      p = Includes;

      while (SUCCEEDED(hr) && *p)
      {
         PWSTR Path = NULL;

         hr = HeapPrintf(&Path, p[0], p[1], SdkVersion);
         if (SUCCEEDED(hr))
         {
            hr = StringListAllocString(
               Path,
               Args.IncludePaths,
               &Args.IncludePaths
            );
         }
         free(Path);

         p += 2;
      }

      free(SdkVersion);
   }

   // SDK and compiler libraries...
   //
   if (SUCCEEDED(hr))
   {
      const ARCHITECTURE *Arch = FindArchByConfiguration(
         Args.Base.DesiredArchitecture
      );
      PCWSTR LibPaths[32], *p = LibPaths;
      PWSTR SdkVersion = NULL;

      if (SdkInfo->Major == 10)
      {
         hr = FindInterestingWin10Version(SdkInfo->InstallDir, &SdkVersion);

         *p++ = L"%s\\lib\\%s\\ucrt";
         *p++ = SdkInfo->InstallDir,
         *p++ = Arch ? Arch->SdkArchName : L"x86";
         *p++ = SdkVersion;

         *p++ = L"%s\\lib\\%s\\um";
         *p++ = SdkInfo->InstallDir,
         *p++ = Arch ? Arch->SdkArchName : L"x86";
         *p++ = SdkVersion;
      }
      else
      {
         *p++ = L"%s\\lib";
         *p++ = SdkInfo->InstallDir,
         *p++ = Arch ? Arch->SdkArchName : NULL;
         *p++ = NULL;
      }

      *p++ = L"%s\\..\\..\\VC\\lib";
      *p++ = CompilerInfo->InstallDir,
      *p++ = Arch ? Arch->ClArchName : NULL;
      *p++ = NULL;

      *p = NULL;
      p = LibPaths;

      while (SUCCEEDED(hr) && *p)
      {
         PWSTR Path = NULL;
         PWSTR OnHeap = NULL;
         PCWSTR Fmt = p[0]; 
         PCWSTR InstallDir = p[1];
         PCWSTR ArchString = p[2];
         PCWSTR Sdk = p[3];

         if (ArchString)
         {
            hr = HeapPrintf(&OnHeap, L"%s\\%%s", Fmt);
            Fmt = OnHeap;

            if (Sdk)
            {
               PCWSTR c = Sdk;
               Sdk = ArchString;
               ArchString = c;
            }
         }

         if (SUCCEEDED(hr))
            hr = HeapPrintf(&Path, Fmt, InstallDir, ArchString, Sdk);
         if (SUCCEEDED(hr))
         {
            hr = StringListAllocString(
               Path,
               Args.Base.LibraryPaths,
               &Args.Base.LibraryPaths
            );
         }

         free(Path);
         free(OnHeap);

         p += 4;
      }

      free(SdkVersion);
   }

   // CL depends on some DLLs in VS's "IDE" dir.
   //
   if (SUCCEEDED(hr))
   {
      hr = AddToPath(CompilerInfo->InstallDir);
   }

   // Build path to CL
   //
   if (SUCCEEDED(hr))
      hr = AppendString(L"\"", &CommandLine);
   if (SUCCEEDED(hr))
      hr = AppendString(CompilerInfo->ClPaths->String, &CommandLine);
   if (SUCCEEDED(hr))
      hr = AppendString(L"\" ", &CommandLine);

   //
   // Now we translate our args struct into a CL command line...
   //

   if (SUCCEEDED(hr))
      hr = AppendString(L"/nologo /EHsc /FS ", &CommandLine);

   if (SUCCEEDED(hr) && Args.Optimization)
   {
      WCHAR Output = Args.Optimization;
      WCHAR Buffer[] = L"/O? ";

      if (Output != L's')
      {
         INT Level = Output - L'0';

         if (Level > 2)
            Output = L'x';
      }

      Buffer[2] = Output;
      hr = AppendString(Buffer, &CommandLine);
   }

   if (SUCCEEDED(hr) && Args.Wall)
      hr = AppendString(L"/W3 ", &CommandLine);

   if (SUCCEEDED(hr) && Args.Werror)
      hr = AppendString(L"/WX ", &CommandLine);

   if (SUCCEEDED(hr))
   {
      WCHAR CrtFlag[] = L"/M? ";

      CrtFlag[2] = Args.Base.StaticCrt ? L'T' : L'D';

      hr = AppendString(CrtFlag, &CommandLine);
   }

   if (SUCCEEDED(hr) && Args.DisableRtti)
   {
      hr = AppendString(L"/GR- ", &CommandLine);
   }

   if (SUCCEEDED(hr))
   {
      WCHAR Type = 0;
      WCHAR AExe[] = L"a.exe";

      switch (Args.OutputType)
      {
      case CC_EXECUTABLE:
         Type = L'e';
         if (!Args.OutputName)
         {
            Args.OutputName = AExe;
         }
         break;
      case CC_OBJECT_FILE:
         Type = L'o';
         hr = AppendString(L"/c ", &CommandLine);
         Link = FALSE;
         break;
      case CC_SHARED_LIBRARY:
         Type = L'e';
         hr = AppendString(L"/LD ", &CommandLine);
         break;
      default:
         fprintf(stderr, "Unrecognized output type %x\n", Args.OutputType);
         hr = E_INVALIDARG;
      }

      if (SUCCEEDED(hr))
         hr = AppendString(L"/Zi ", &CommandLine);

      if (SUCCEEDED(hr) && Args.OutputName)
      {
         WCHAR Prefix[] = L"/F?";

         Prefix[2] = Type;
         hr = AppendString(Prefix, &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(Args.OutputName, &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(L" ", &CommandLine);

         if (SUCCEEDED(hr) && Link)
         {
            Prefix[2] = L'd';

            hr = AppendString(Prefix, &CommandLine);
            if (SUCCEEDED(hr))
            {
               PWSTR p = wcsrchr(Args.OutputName, L'.');
               if (p)
                  *p = 0;
               hr = AppendString(Args.OutputName, &CommandLine);
            }
            if (SUCCEEDED(hr))
               hr = AppendString(L".pdb ", &CommandLine);
         }
      }
   }

   if (SUCCEEDED(hr))
   {
      for (List = Args.Macros; List; List = List->Next)
      {
         hr = AppendString(L"/D", &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(List->String, &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(L" ", &CommandLine);
         if (FAILED(hr))
            break;
      }
   }

   if (SUCCEEDED(hr))
   {
      for (List = Args.IncludePaths; List; List = List->Next)
      {
         hr = AppendString(L"/I\"", &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(List->String, &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(L"\" ", &CommandLine);
         if (FAILED(hr))
            break;
      }
   }

   if (SUCCEEDED(hr))
   {
      for (List = Args.Inputs; List; List = List->Next)
      {
         hr = AppendString(List->String, &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(L" ", &CommandLine);
         if (FAILED(hr))
            break;
      }
   }

   if (SUCCEEDED(hr) && Link)
   {
      for (List = Args.Base.Libraries; List; List = List->Next)
      {
         hr = AppendString(List->String, &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(L".LIB ", &CommandLine);
         if (FAILED(hr))
            break;
      }
   }

   if (SUCCEEDED(hr) && Link && Args.Base.LibraryPaths)
   {
      hr = AppendString(L"/link ", &CommandLine);
   }

   if (SUCCEEDED(hr) && Link)
   {
      for (List = Args.Base.LibraryPaths; List; List = List->Next)
      {
         hr = AppendString(L"/LIBPATH:\"", &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(List->String, &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(L"\" ", &CommandLine);
         if (FAILED(hr))
            break;
      }
   }

   if (SUCCEEDED(hr))
   {
      hr = LaunchProcess(CommandLine.Buffer, ReturnValue);
   }

   FreeString(&CommandLine);
   FreeVsVersions(CompilerInfo);
   FreeVsVersions(SdkInfo);
   CcArgsFree(&Args);
   return hr;
}

// Starting with Win10, it seems the directory structure of non-CRT headers and
// libraries changes, with several target versions and not all of them usable.
// This function should find the highest number target where windows.h exists.
//
static HRESULT
FindInterestingWin10Version(
   PCWSTR SdkInstallDir,
   PWSTR *Result
)
{
   HRESULT hr = S_OK;
   PWSTR FindPath = NULL;
   PWSTR TempPath = NULL;
   WIN32_FIND_DATA FindData = {0};
   HANDLE FindHandle = INVALID_HANDLE_VALUE;
   WCHAR ResultBuffer[MAX_PATH] = {0};

   hr = HeapPrintf(&FindPath, L"%s\\include\\*", SdkInstallDir);
   if (SUCCEEDED(hr))
   {
      FindHandle = FindFirstFile(FindPath, &FindData);
      if (FindHandle == INVALID_HANDLE_VALUE)
         hr = HRESULT_FROM_WIN32(GetLastError());
   }

   if (SUCCEEDED(hr)) do
   {
      DWORD Attrs = 0;

      if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
          (FindData.cFileName[0] == L'.' &&
           (!FindData.cFileName[1] || !wcscmp(FindData.cFileName + 1, L"."))))
      {
         continue;
      }

      free(TempPath);
      TempPath = NULL;

      hr = HeapPrintf(
         &TempPath,
         L"%s\\include\\%s\\um\\windows.h",
         SdkInstallDir,
         FindData.cFileName
      );
      if (FAILED(hr))
         break;

      Attrs = GetFileAttributes(TempPath);
      if (Attrs == INVALID_FILE_ATTRIBUTES)
         continue;

      wcscpy(ResultBuffer, FindData.cFileName);

   } while (FindNextFile(FindHandle, &FindData));

   if (SUCCEEDED(hr))
   {
      if (!*ResultBuffer)
         hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
      else
      {
         DWORD Length = (wcslen(ResultBuffer) + 1) * sizeof(WCHAR);
         *Result = malloc(Length);
         if (!*Result)
            hr = E_OUTOFMEMORY;
         else
            memcpy(*Result, ResultBuffer, Length);
      }
   }
   
   if (FindHandle != INVALID_HANDLE_VALUE)
      FindClose(FindHandle);
   free(FindPath);
   free(TempPath);
   return hr;
}

static HRESULT
CcParseArgs(
   PCC_ARGS Args,
   PWSTR *CurrentArg
)
{
   HRESULT hr = S_OK;

   // Attempt to parse arguments...
   //
   while (*CurrentArg)
   {
      INT NumConsumed = 0;

      hr = CcParseArg(Args, CurrentArg, &NumConsumed);
      if (FAILED(hr))
         break;

      if (NumConsumed)
      {
         CurrentArg += NumConsumed;
      }
      else
      {
         hr = StringListAllocString(
            *CurrentArg++,
            Args->Inputs,
            &Args->Inputs
         );
         if (FAILED(hr))
            break;
      }
   }

   // Go through all the string lists and make sure they are in order.
   //
   if (SUCCEEDED(hr))
   {
      PSTRING_LIST *StringLists[] =
      {
         &Args->Base.LibraryPaths,
         &Args->Base.Libraries,
         &Args->Macros,
         &Args->IncludePaths,
         &Args->LinkerOptions,
         &Args->Inputs,
         NULL
      }, **p = StringLists;

      while (*p)
         StringListReverse(*p++);
   }

   return hr;
}

HRESULT
CcParseArg(
   PCC_ARGS Context,
   PWSTR *Arg,
   INT *NumConsumedOut
)
{
   HRESULT hr = S_OK;

   *NumConsumedOut = 0;

   hr = BaseParseArg(&Context->Base, Arg, NumConsumedOut);
   if (SUCCEEDED(hr) &&
       *NumConsumedOut)
   {
      Arg += *NumConsumedOut;
   }

   while (SUCCEEDED(hr) && *Arg)
   {
      if (!wcscmp(*Arg, L"-shared"))
      {
         if (Context->OutputType == CC_OBJECT_FILE)
         {
            fprintf(stderr, "-shared conflicts with -c\n");
            hr = E_INVALIDARG;
         }
         Context->OutputType = CC_SHARED_LIBRARY;
         ++*NumConsumedOut;
         ++Arg;
      } 
      else if (!wcscmp(*Arg, L"-c"))
      {
         if (Context->OutputType == CC_SHARED_LIBRARY)
         {
            fprintf(stderr, "-c conflicts with -shared\n");
            hr = E_INVALIDARG;
         }
         Context->OutputType = CC_OBJECT_FILE;
         ++*NumConsumedOut;
         ++Arg;
      }
      else if (!wcscmp(*Arg, L"-o"))
      {
         Context->OutputName = Arg[1];
         if (!Context->OutputName)
         {
            fprintf(stderr, "-o requires argument\n");
            hr = E_INVALIDARG;
         }
         *NumConsumedOut += 2;
         Arg += 2;
      }
      else if (!wcscmp(*Arg, L"-Wall"))
      {
         Context->Wall = TRUE;
         ++*NumConsumedOut;
         ++Arg;
      }
      else if (!wcscmp(*Arg, L"-Werror"))
      {
         Context->Werror = TRUE;
         ++*NumConsumedOut;
         ++Arg;
      }
      else if (!wcsncmp(*Arg, L"-D", 2))
      {
         hr = StringListAllocString(
            *Arg + 2,
            Context->Macros,
            &Context->Macros
         );
         ++*NumConsumedOut;
         ++Arg;
      }
      else if (!wcsncmp(*Arg, L"-I", 2))
      {
         hr = StringListAllocString(
            *Arg + 2,
            Context->IncludePaths,
            &Context->IncludePaths
         );
         ++*NumConsumedOut;
         ++Arg;
      }
      else if (!wcsncmp(*Arg, L"-O", 2))
      {
         PCWSTR Type = *Arg + 2;

         if (Type[1] ||
             !wcschr(L"s0123456789", Context->Optimization = Type[0]))
         {
            fprintf(stderr, "Unrecognized optimization: %ls\n", Type);
            hr = E_INVALIDARG;
            break;
         }

         ++*NumConsumedOut;
         ++Arg;
      }
      else if (!wcscmp(*Arg, L"-pthread"))
      {
         ++*NumConsumedOut;
         ++Arg;
      }
      else if (!wcscmp(*Arg, L"-fno-rtti"))
      {
         Context->DisableRtti = TRUE;
         ++*NumConsumedOut;
         ++Arg;
      }
      else
      {
         break;
      }
   }

   return hr;
}

VOID
CcArgsFree(
   PCC_ARGS Context
)
{
   BaseArgsFree(&Context->Base);

   FreeStringList(Context->Macros);
   FreeStringList(Context->IncludePaths);
   FreeStringList(Context->LinkerOptions);
   FreeStringList(Context->Inputs);
}

int main()
{
   INT Argc = 0;
   PWSTR *Args = CommandLineToArgvW(GetCommandLine(), &Argc);
   HRESULT hr = S_OK;
   DWORD ExitCode = 0;

   if (!Args)
   {
      DWORD Err = GetLastError();
      fprintf(stderr, "CommandLineToArgvW failed, GetLastError=0x%.8x\n", Err);
      return Err;
   }

   hr = CcMain(Argc, Args, &ExitCode);

   LocalFree(Args);
   if (FAILED(hr))
   {
      fprintf(stderr, "Failed with 0x%.8x\n", hr);
      return hr;
   }
   return ExitCode;
}

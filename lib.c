/*
 * Copyright (c) 2017 Andrew Sveikauskas
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "clwrapper.h"
#include <stdio.h>

static HRESULT
LibParseArgs(
   PCLWRAPPER_ARGS_BASE Context,
   PWSTR *Args,
   PSTRING_LIST *Inputs
);

static HRESULT
LibMain(
   INT Argc,
   PWSTR *Argv,
   PDWORD ReturnValue
)
{
   HRESULT hr = S_OK;
   CLWRAPPER_ARGS_BASE Args = {0};
   OUTPUT_STRING CommandLine = {0};
   PVS_VERSION CompilerInfo = NULL;
   PVS_VERSION SdkInfo = NULL;
   PWSTR ClPath = NULL;
   PWSTR LibPath = NULL;
   PSTRING_LIST Inputs = NULL;

   hr = LibParseArgs(&Args, Argv + 1, &Inputs);

   if (SUCCEEDED(hr))
   {
      hr = FindToolset(&Args, &CompilerInfo, &SdkInfo);
   }

   if (SUCCEEDED(hr))
   {
      const ARCHITECTURE *Arch = FindArchByConfiguration(
         Args.DesiredArchitecture
      );

      if (Arch)
      {
         hr = HeapPrintf(&ClPath, L"%s", Arch->ClFile);
         if (SUCCEEDED(hr))
            ClPath[wcslen(ClPath) - 5] = 0;
      }
      else
      {
         hr = HeapPrintf(&ClPath, L"..\\..\\VC\\bin\\");
      }
   }

   if (SUCCEEDED(hr))
   {
      hr = AddToPath(CompilerInfo->InstallDir);
   }

   if (SUCCEEDED(hr))
      hr = AppendString(L"\"", &CommandLine);
   if (SUCCEEDED(hr))
      hr = AppendString(CompilerInfo->InstallDir, &CommandLine);
   if (SUCCEEDED(hr))
      hr = AppendString(L"\\", &CommandLine);
   if (SUCCEEDED(hr))
      hr = AppendString(ClPath, &CommandLine);
   if (SUCCEEDED(hr))
      hr = AppendString(L"lib.exe\" ", &CommandLine);

   if (SUCCEEDED(hr))
   {
      PSTRING_LIST List;

      for (List = Inputs; List; List = List->Next)
      {
         hr = AppendString(List->String, &CommandLine);
         if (SUCCEEDED(hr))
            hr = AppendString(L" ", &CommandLine);
         if (FAILED(hr))
            break;
      }
   }

   if (SUCCEEDED(hr))
   {
      hr = LaunchProcess(CommandLine.Buffer, ReturnValue);
   }

   FreeString(&CommandLine);
   FreeStringList(Inputs);
   FreeVsVersions(CompilerInfo);
   FreeVsVersions(SdkInfo);
   BaseArgsFree(&Args);
   free(ClPath);

   return hr;
}

static HRESULT
LibParseArgs(
   PCLWRAPPER_ARGS_BASE Context,
   PWSTR *CurrentArg,
   PSTRING_LIST *InputsOut
)
{
   HRESULT hr = S_OK;
   PSTRING_LIST Inputs = NULL;

   // Attempt to parse arguments...
   //
   while (*CurrentArg)
   {
      INT NumConsumed = 0;

      hr = BaseParseArg(Context, CurrentArg, &NumConsumed);
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
            Inputs,
            &Inputs
         );
         if (FAILED(hr))
            break;
      }
   }

   if (SUCCEEDED(hr))
   {
      StringListReverse(&Inputs);
   }

   if (FAILED(hr))
   {
      FreeStringList(Inputs);
      Inputs = NULL;
   }

   *InputsOut = Inputs;

   return hr;
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

   hr = LibMain(Argc, Args, &ExitCode);

   LocalFree(Args);
   if (FAILED(hr))
   {
      fprintf(stderr, "Failed with 0x%.8x\n", hr);
      return hr;
   }
   return ExitCode;
}

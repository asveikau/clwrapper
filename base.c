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
SetVersion(
   PCWSTR Arg,
   PCLWRAPPER_VERSION_SPEC Version
)
{
   HRESULT hr = S_OK;

   swscanf(Arg, L"%hd.%hd", &Version->DesiredMajor, &Version->DesiredMinor);
   Version->Specified = TRUE;

   return hr;
}

HRESULT
BaseParseArg(
   PCLWRAPPER_ARGS_BASE Context,
   PWSTR *Arg,
   INT *NumConsumedOut
)
{
   HRESULT hr = S_OK;

   *NumConsumedOut = 0;

   while (*Arg && SUCCEEDED(hr))
   {
      if (!wcscmp(*Arg, L"-V"))
      {
         if (!Arg[1])
         {
            fprintf(stderr, "-V expects an argument\n");
            hr = E_INVALIDARG;
            break;
         }
         hr = SetVersion(Arg[1], &Context->CompilerVersion);
         Arg += 2;
         *NumConsumedOut += 2;
      }
      else if (!wcscmp(*Arg, L"-sdkversion"))
      {
         if (!Arg[1])
         {
            fprintf(stderr, "-V expects an argument\n");
            hr = E_INVALIDARG;
            break;
         }
         hr = SetVersion(Arg[1], &Context->SdkVersion);
         Arg += 2;
         *NumConsumedOut += 2;
      }
      else if (!wcscmp(*Arg, L"-static-crt"))
      {
         Context->StaticCrt = TRUE;
         ++Arg;
         ++*NumConsumedOut;
      }
      else if (!wcsncmp(*Arg, L"-m", 2))
      {
         Context->DesiredArchitecture = *Arg + 2;
         ++Arg;
         ++*NumConsumedOut;
      }
      else if (!wcsncmp(*Arg, L"-L", 2))
      {
         hr = StringListAllocString(
            *Arg + 2,
            Context->LibraryPaths,
            &Context->LibraryPaths
         );
         ++Arg;
         ++*NumConsumedOut;
      }
      else if (!wcsncmp(*Arg, L"-l", 2))
      {
         hr = StringListAllocString(
            *Arg + 2,
            Context->Libraries,
            &Context->Libraries
         );
         ++Arg;
         ++*NumConsumedOut;
      }
      else
      {
         break;
      }
   }

   return hr;
}

VOID
BaseArgsFree(
   PCLWRAPPER_ARGS_BASE Context
)
{
   FreeStringList(Context->LibraryPaths);
   FreeStringList(Context->Libraries);
}

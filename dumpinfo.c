/*
 * Copyright (c) 2017 Andrew Sveikauskas
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "clwrapper.h"

#include <stdio.h>

int main()
{
   HRESULT hr = S_OK;
   PVS_VERSION List = NULL;

   hr = GetInstalledVsVersions(&List);   

   if (SUCCEEDED(hr))
   {
      PVS_VERSION Current = List;
      while (Current)
      {
         PSTRING_LIST String = Current->Configurations;
         printf("%d.%d:\n", Current->Major, Current->Minor);
         printf("   Install Dir: %ls\n", Current->InstallDir);
         printf("   Configurations:");
         while (String)
         {
            printf(" %ls", String->String);
            String = String->Next;
         }
         puts("");
         Current = Current->Next;
      }
   }

   FreeVsVersions(List);
   List = NULL;

   if (SUCCEEDED(hr))
   {
      hr = GetInstalledSdks(FALSE, &List);
   }

   if (SUCCEEDED(hr) &&
       List)
   {
      PVS_VERSION Current = List;

      puts("SDKs:");
      while (Current)
      {
         printf(
            "   %d.%d at %ls\n",
            Current->Major,
            Current->Minor,
            Current->InstallDir
         );
         Current = Current->Next;
      }
   }
   
   FreeVsVersions(List);
   List = NULL;

   if (SUCCEEDED(hr))
   {
      hr = GetInstalledSdks(TRUE, &List);
   }

   if (SUCCEEDED(hr)
       && List)
   {
      PVS_VERSION Current = List;

      puts("WinCE SDKs:");
      while (Current)
      {
         printf(
            "   %d.%d at %ls\n",
            Current->Major,
            Current->Minor,
            Current->InstallDir
         );
         Current = Current->Next;
      }
   }

   FreeVsVersions(List);

   if (FAILED(hr))
      fprintf(stderr, "Failed with 0x%.8x\n", hr);
   return hr;
}

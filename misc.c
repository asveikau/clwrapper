/*
 * Copyright (c) 2017 Andrew Sveikauskas
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "clwrapper.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <intsafe.h>

HRESULT
StringListAlloc(
   DWORD NChars,
   PSTRING_LIST Next,
   PSTRING_LIST *Output
)
{
   HRESULT hr = S_OK;
   PSTRING_LIST ListNode;
   DWORD Size = 0;

   hr = DWordAdd(NChars, 1, &Size);
   if (SUCCEEDED(hr))
   {
      hr = DWordMult(Size, sizeof(WCHAR), &Size);
   }

   if (SUCCEEDED(hr))
   {
      hr = DWordAdd(Size, FIELD_OFFSET(STRING_LIST, String), &Size);
   }

   if (SUCCEEDED(hr))
   {
      ListNode = malloc(Size);
      if (!ListNode)
      {
         hr = E_OUTOFMEMORY;
      }
   }

   if (SUCCEEDED(hr))
   {
      ListNode->Next = Next;
      ListNode->String[0] = 0;

      *Output = ListNode;
   }

   return hr;
}

HRESULT
StringListAllocString(
   PCWSTR String,
   PSTRING_LIST Next,
   PSTRING_LIST *Output
)
{
   HRESULT hr = S_OK;
   DWORD Length = wcslen(String);

   hr = StringListAlloc(Length, Next, Output);
   if (SUCCEEDED(hr))
   {
      memcpy((*Output)->String, String, (Length + 1) * sizeof(WCHAR));
   }

   return hr;
}

VOID
FreeStringList(
   PSTRING_LIST List
)
{
   while (List)
   {
      PSTRING_LIST Next = List->Next;
      free(List);
      List = Next;
   }
}

VOID
StringListReverse(
   PSTRING_LIST *Head
)
{
   if (Head && *Head && (*Head)->Next)
   {
      PSTRING_LIST Prev = NULL;
      PSTRING_LIST Current = *Head;

      while (Current)
      {
         PSTRING_LIST Next = Current->Next;
         Current->Next = Prev;
         Prev = Current;
         Current = Next;
      }

      *Head = Prev;
   }
}

HRESULT
AllocateString(
   DWORD NumChars,
   POUTPUT_STRING String,
   PWSTR *Output
)
{
   HRESULT hr = S_OK;
   DWORD RequiredSize = 0;
   DWORD Alloc = String->AllocatedBytes;

   if (!Alloc)
      Alloc = 8;

   hr = DWordAdd(NumChars, String->Length, &RequiredSize);
   if (SUCCEEDED(hr))
      hr = DWordAdd(RequiredSize, 1, &RequiredSize);
   if (SUCCEEDED(hr))
      hr = DWordMult(RequiredSize, sizeof(WCHAR), &RequiredSize);

   while (SUCCEEDED(hr) && Alloc < RequiredSize)
   {
      hr = DWordMult(Alloc, 2, &Alloc);
   }

   if (SUCCEEDED(hr) &&
       Alloc != String->AllocatedBytes)
   {
      PVOID NewBuffer;

      if (String->Buffer)
         NewBuffer = realloc(String->Buffer, Alloc);
      else
         NewBuffer = malloc(Alloc);

      if (!NewBuffer)
         hr = E_OUTOFMEMORY;
      else
      {
         String->Buffer = NewBuffer;
         String->AllocatedBytes = Alloc;
      }
   }

   if (SUCCEEDED(hr))
   {
      PWSTR NewStart = String->Buffer + String->Length;

      NewStart[NumChars] = 0;

      if (Output)
         *Output = NewStart;

      String->Length += NumChars;
   }

   return hr;
}

HRESULT
AppendString(
   PCWSTR Src,
   POUTPUT_STRING Dst
)
{
   HRESULT hr = S_OK;
   PWSTR DstString = NULL;
   SIZE_T Length = wcslen(Src);

   hr = AllocateString(Length, Dst, &DstString);
   if (SUCCEEDED(hr))
      memcpy(DstString, Src, Length * sizeof(WCHAR));

   return hr;
}

VOID
FreeString(POUTPUT_STRING Str)
{
   free(Str->Buffer);
}

HRESULT
HeapVPrintf(
   PWSTR *Output,
   PCWSTR Fmt,
   va_list Ap
)
{
   HRESULT hr = S_OK;
   va_list Ap2 = Ap;
   int r = _vscwprintf(Fmt, Ap2);
   PVOID Buffer = NULL;
   DWORD Size = 0;

   hr = DWordAdd(r, 1, &Size);

   if (SUCCEEDED(hr))
      hr = DWordMult(Size, sizeof(WCHAR), &Size);

   if (SUCCEEDED(hr))
   {
      if (!(Buffer = malloc(Size)))
         hr = E_OUTOFMEMORY;
      else
         vswprintf(Buffer, r + 1, Fmt, Ap);
   }

   *Output = Buffer;
   return hr;
}

HRESULT
HeapPrintf(
   PWSTR *Output,
   PCWSTR Fmt,
   ...
)
{
   HRESULT hr;
   va_list ap;
   va_start(ap, Fmt);
   hr = HeapVPrintf(Output, Fmt, ap);
   va_end(ap);
   return hr;
}

HRESULT
GetStringValue(
   HKEY Key,
   PCWSTR ValueName,
   PWSTR *Out
)
{
   HRESULT hr = S_OK;
   DWORD Result = 0;
   PWSTR Value = NULL;
   DWORD BufferSize = 0;

   while (SUCCEEDED(hr))
   {
      DWORD Type = 0;
      DWORD Size = BufferSize;

      Result = RegQueryValueEx(
         Key,
         ValueName,
         NULL,
         &Type,
         (PVOID)Value,
         &Size
      );

      if ((Result == ERROR_MORE_DATA && Size > BufferSize) ||
          Size > BufferSize)
      {
         if (Value)
            Value = realloc(Value, Size);
         else
            Value = malloc(Size);
         if (!Value) 
         {
            hr = E_OUTOFMEMORY;
            break;
         }
         BufferSize = Size;
         continue;
      }
      else if (Result)
      {
         hr = HRESULT_FROM_WIN32(Result);
      }
      else if (Type != REG_SZ)
      {
         hr = E_UNEXPECTED;
      }
      else
      {
         PWSTR p = Value;
         SIZE_T s = Size / sizeof(WCHAR);
         BOOL Found = FALSE;

         while (!Found && s--)
         {
            if (!*p++)
               Found = TRUE;
         }

         if (!Found)
         {
            hr = E_UNEXPECTED;
         }
      }

      break;
   }

   if (FAILED(hr))
   {
      free(Value);
      Value = NULL;
   }

   *Out = Value;
   return hr;
}

HRESULT
EnumKey(
   PVOID Context,
   HKEY Parent,
   PCWSTR KeyName,
   HRESULT (*Fn)(PVOID Context, HKEY Key, PCWSTR ChildKey)
)
{
   HRESULT hr = S_OK;
   HKEY Key = NULL;
   DWORD Result = 0;
   DWORD Index = 0;
   PWSTR NameBuffer = NULL;
   DWORD NameBufferSize = 0;

   Result = RegOpenKeyEx(
      Parent,
      KeyName,
      0,
      KEY_ENUMERATE_SUB_KEYS,
      &Key
   );

   if (Result)
      hr = HRESULT_FROM_WIN32(Result);

   if (SUCCEEDED(hr))
   {
      NameBufferSize = 4096;
      NameBuffer = malloc(NameBufferSize * sizeof(WCHAR));
      if (!NameBuffer)
         hr = E_OUTOFMEMORY;
   }

   while (SUCCEEDED(hr))
   {
      DWORD DesiredSize = NameBufferSize;

      Result = RegEnumKeyEx(
         Key,
         Index,
         NameBuffer,
         &DesiredSize,
         NULL,
         NULL,
         NULL,
         NULL
      );

      if (Result == ERROR_MORE_DATA)
      {
         DWORD SizeInBytes = 0;

         hr = DWordMult(NameBufferSize, 2 * sizeof(WCHAR), &SizeInBytes);
         if (FAILED(hr))
            break;

         if (NameBuffer)
            NameBuffer = realloc(NameBuffer, SizeInBytes);
         else
            NameBuffer = malloc(SizeInBytes);

         if (!NameBuffer)
         {
            hr = E_OUTOFMEMORY;
            break;
         }

         NameBufferSize = DesiredSize;
         continue;
      }
      else if (Result == ERROR_NO_MORE_ITEMS)
      {
         break;
      }
      else if (Result)
      {
         hr = HRESULT_FROM_WIN32(Result);
         break;
      }

      hr = Fn(Context, Key, NameBuffer);
      ++Index;
   }

   free(NameBuffer);
   if (Key)
      RegCloseKey(Key);
   return hr;
}

HRESULT
AddToPath(
   PCWSTR NewPath
)
{
   HRESULT hr = S_OK;
   SIZE_T NewLength = wcslen(NewPath);
   PWSTR OldPath = NULL;
   DWORD OldLength = 0;
   DWORD TotalLength = 0;
   INT Offset = 0;

   OldLength = GetEnvironmentVariable(L"PATH", NULL, 0);

   hr = DWordAdd(OldLength, NewLength, &TotalLength);
   if (SUCCEEDED(hr))
      hr = DWordAdd(TotalLength, 2, &TotalLength);
   if (SUCCEEDED(hr))
      hr = DWordMult(TotalLength, sizeof(WCHAR), &TotalLength);

   if (SUCCEEDED(hr))
   {
      OldPath = malloc(TotalLength);
      if (!OldPath)
         hr = E_OUTOFMEMORY;
   }

   if (SUCCEEDED(hr) &&
       OldLength)
   {
      memset(OldPath, 0, TotalLength);

      GetEnvironmentVariable(L"PATH", OldPath, OldLength);
   }

   if (SUCCEEDED(hr))
   {
      if (*OldPath)
         wcscat(OldPath, L";");
      wcscat(OldPath, NewPath);

      if (!SetEnvironmentVariable(L"PATH", NewPath))
      {
         hr = HRESULT_FROM_WIN32(GetLastError());
      }
   }

   free(OldPath);
   return hr;
}

HRESULT
LaunchProcess(
   PCWSTR CommandLine,
   PDWORD ReturnValue
)
{
   HRESULT hr = S_OK;

   BOOL Res;
   PROCESS_INFORMATION ProcessInfo = {0};
   STARTUPINFO StartupInfo = {0};

   StartupInfo.cb = sizeof(StartupInfo);
   StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
   StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
   StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);

   Res = CreateProcess(
      NULL,
      CommandLine,
      NULL,
      NULL,
      TRUE,
      0,
      NULL,
      NULL,
      &StartupInfo,
      &ProcessInfo
   );

   if (!Res)
   {
      hr = HRESULT_FROM_WIN32(GetLastError());
   }

   if (SUCCEEDED(hr))
   {
      WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
      GetExitCodeProcess(ProcessInfo.hProcess, ReturnValue);
   }

   if (ProcessInfo.hProcess)
      CloseHandle(ProcessInfo.hProcess);
   if (ProcessInfo.hThread)
      CloseHandle(ProcessInfo.hThread);

   return hr;
}

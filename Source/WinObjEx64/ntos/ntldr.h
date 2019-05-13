/************************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2014 - 2019
*
*  TITLE:       NTLDR.H
*
*  VERSION:     1.12
*
*  DATE:        08 May 2019
*
*  Common header file for the NTLDR definitions.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
************************************************************************************/

#pragma once

/*
*
*  W32pServiceTable query related structures and definitions.
*
*/

typedef enum _RESOLVE_POINTER_TYPE {
    ForwarderString = 0,
    FunctionCode = 1
} RESOLVE_POINTER_TYPE;

typedef struct _RESOLVE_INFO {
    RESOLVE_POINTER_TYPE ResultType;
    union {
        LPCSTR ForwarderName;
        LPVOID Function;
    };
} RESOLVE_INFO, *PRESOLVE_INFO;

typedef struct _LOAD_MODULE_ENTRY {
    HMODULE hModule;
    struct _LOAD_MODULE_ENTRY *Next;
} LOAD_MODULE_ENTRY, *PLOAD_MODULE_ENTRY;

typedef struct _WIN32_SHADOWTABLE {
    ULONG Index;
    CHAR Name[256];
    ULONG_PTR KernelStubAddress;
    ULONG_PTR KernelStubTargetAddress;
    struct _WIN32_SHADOWTABLE *NextService;
} WIN32_SHADOWTABLE, *PWIN32_SHADOWTABLE;


_Success_(return != NULL)
LPCSTR NtRawIATEntryToImport(
    _In_ LPVOID Module,
    _In_ LPVOID IATEntry,
    _Out_opt_ LPCSTR *ImportModuleName);

_Success_(return != 0)
ULONG NtRawEnumExports(
    _In_ HANDLE HeapHandle,
    _In_ LPVOID Module,
    _Out_ PWIN32_SHADOWTABLE* Table);

NTSTATUS NtRawGetProcAddress(
    _In_ LPVOID Module,
    _In_ LPCSTR ProcName,
    _In_ PRESOLVE_INFO Pointer);

BOOLEAN NtLdrApiSetLoadFromPeb(
    _Out_ PULONG SchemaVersion,
    _Out_ PVOID* DataPointer);

_Success_(return == STATUS_SUCCESS)
NTSTATUS NtLdrApiSetResolveLibrary(
    _In_ PVOID Namespace,
    _In_ PUNICODE_STRING ApiSetToResolve,
    _In_opt_ PUNICODE_STRING ApiSetParentName,
    _Out_ PBOOL Resolved,
    _Out_ PUNICODE_STRING ResolvedHostLibraryName);

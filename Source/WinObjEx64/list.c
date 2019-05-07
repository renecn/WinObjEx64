/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2015 - 2019
*
*  TITLE:       LIST.C
*
*  VERSION:     1.74
*
*  DATE:        03 May 2019
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#include "global.h"
#include "treelist\treelist.h"

/*
* TreeListAddItem
*
* Purpose:
*
* Insert new treelist item.
*
*/
HTREEITEM TreeListAddItem(
    _In_ HWND TreeList,
    _In_opt_ HTREEITEM hParent,
    _In_ UINT mask,
    _In_ UINT state,
    _In_ UINT stateMask,
    _In_opt_ LPWSTR pszText,
    _In_opt_ PVOID subitems
)
{
    TVINSERTSTRUCT  tvitem;
    PTL_SUBITEMS    si = (PTL_SUBITEMS)subitems;

    RtlSecureZeroMemory(&tvitem, sizeof(tvitem));
    tvitem.hParent = hParent;
    tvitem.item.mask = mask;
    tvitem.item.state = state;
    tvitem.item.stateMask = stateMask;
    tvitem.item.pszText = pszText;
    tvitem.hInsertAfter = TVI_LAST;
    return TreeList_InsertTreeItem(TreeList, &tvitem, si);
}

/*
* GetNextSub
*
* Purpose:
*
* Returns next subitem in object full pathname.
*
*/
LPWSTR GetNextSub(
    _In_ LPWSTR ObjectFullPathName,
    _In_ LPWSTR Sub
)
{
    SIZE_T i;

    for (i = 0; (*ObjectFullPathName != 0) && (*ObjectFullPathName != '\\')
        && (i < MAX_PATH); i++, ObjectFullPathName++)
    {
        Sub[i] = *ObjectFullPathName;
    }
    Sub[i] = 0;

    if (*ObjectFullPathName == '\\')
        ObjectFullPathName++;

    return ObjectFullPathName;
}

/*
* ListToObject
*
* Purpose:
*
* Select and focus list view item by given object name.
*
*/
VOID ListToObject(
    _In_ LPWSTR ObjectName
)
{
    BOOL        currentfound = FALSE;
    INT         i, s;
    HTREEITEM   lastfound, item;
    LVITEM      lvitem;
    TVITEMEX    ritem;
    WCHAR       object[MAX_PATH + 1], sobject[MAX_PATH + 1];

    if (ObjectName == NULL)
        return;

    if (*ObjectName != '\\')
        return;
    ObjectName++;
    item = TreeView_GetRoot(g_hwndObjectTree);
    lastfound = item;

    while ((item != NULL) && (*ObjectName != 0)) {
        item = TreeView_GetChild(g_hwndObjectTree, item);
        object[0] = 0; //mars workaround
        RtlSecureZeroMemory(object, sizeof(object));
        ObjectName = GetNextSub(ObjectName, object);
        currentfound = FALSE;

        do {
            RtlSecureZeroMemory(&ritem, sizeof(ritem));
            RtlSecureZeroMemory(&sobject, sizeof(sobject));
            ritem.mask = TVIF_TEXT;
            ritem.hItem = item;
            ritem.cchTextMax = MAX_PATH;
            ritem.pszText = sobject;

            if (!TreeView_GetItem(g_hwndObjectTree, &ritem))
                break;

            if (_strcmpi(sobject, object) == 0) {
                if (item)
                    lastfound = item;
                break;
            }

            item = TreeView_GetNextSibling(g_hwndObjectTree, item);
        } while (item != NULL);
    }

    TreeView_SelectItem(g_hwndObjectTree, lastfound);

    if (currentfound) // final target was a subdir
        return;

    for (i = 0; i < MAXINT; i++) {
        RtlSecureZeroMemory(&lvitem, sizeof(lvitem));
        RtlSecureZeroMemory(&sobject, sizeof(sobject));
        lvitem.mask = LVIF_TEXT;
        lvitem.iItem = i;
        lvitem.cchTextMax = MAX_PATH;
        lvitem.pszText = sobject;
        if (!ListView_GetItem(g_hwndObjectList, &lvitem))
            break;

        if (_strcmpi(sobject, object) == 0) {
            s = ListView_GetSelectionMark(g_hwndObjectList);
            lvitem.mask = LVIF_STATE;
            lvitem.stateMask = LVIS_SELECTED | LVIS_FOCUSED;

            if (s >= 0) {
                lvitem.iItem = s;
                lvitem.state = 0;
                ListView_SetItem(g_hwndObjectList, &lvitem);
            }

            lvitem.iItem = i;
            lvitem.state = LVIS_SELECTED | LVIS_FOCUSED;
            ListView_SetItem(g_hwndObjectList, &lvitem);
            ListView_EnsureVisible(g_hwndObjectList, i, FALSE);
            SetFocus(g_hwndObjectList);
            return;
        }
    }
}

/*
* AddTreeViewItem
*
* Purpose:
*
* Add item to the tree view.
*
*/
HTREEITEM AddTreeViewItem(
    _In_ LPWSTR ItemName,
    _In_opt_ HTREEITEM Root
)
{
    TVINSERTSTRUCT	item;

    RtlSecureZeroMemory(&item, sizeof(item));
    item.hParent = Root;
    item.item.mask = TVIF_TEXT | TVIF_SELECTEDIMAGE;
    if (Root == NULL) {
        item.item.mask |= TVIF_STATE;
        item.item.state = TVIS_EXPANDED;
        item.item.stateMask = TVIS_EXPANDED;
    }
    item.item.iSelectedImage = 1;
    item.item.pszText = ItemName;

    return TreeView_InsertItem(g_hwndObjectTree, &item);
}

/*
* ListObjectDirectoryTree
*
* Purpose:
*
* List given directory to the treeview.
*
*/
VOID ListObjectDirectoryTree(
    _In_ LPWSTR SubDirName,
    _In_opt_ HANDLE RootHandle,
    _In_opt_ HTREEITEM ViewRootHandle
)
{
    NTSTATUS            status;
    ULONG               ctx, rlen;
    HANDLE              hDirectory = NULL;
    OBJECT_ATTRIBUTES   objattr;
    UNICODE_STRING      objname;

    POBJECT_DIRECTORY_INFORMATION objinf;

    ViewRootHandle = AddTreeViewItem(SubDirName, ViewRootHandle);
    RtlInitUnicodeString(&objname, SubDirName);
    InitializeObjectAttributes(&objattr, &objname, OBJ_CASE_INSENSITIVE, RootHandle, NULL);
    status = NtOpenDirectoryObject(&hDirectory, DIRECTORY_QUERY, &objattr);
    if (!NT_SUCCESS(status)) {
        return;
    }

    ctx = 0;
    do {

        //
        // Wine implementation of NtQueryDirectoryObject interface is very basic and incomplete.
        // It doesn't work if no input buffer specified and does not return required buffer size.
        //
        if (g_WinObj.IsWine) {
            rlen = 1024 * 64;
        }
        else {
            rlen = 0;
            status = NtQueryDirectoryObject(hDirectory, NULL, 0, TRUE, FALSE, &ctx, &rlen);
            if (status != STATUS_BUFFER_TOO_SMALL)
                break;
        }

        objinf = (POBJECT_DIRECTORY_INFORMATION)supHeapAlloc((SIZE_T)rlen);
        if (objinf == NULL)
            break;

        status = NtQueryDirectoryObject(
            hDirectory, 
            objinf, 
            rlen, 
            TRUE, 
            FALSE, 
            &ctx, 
            &rlen);

        if (!NT_SUCCESS(status)) {
            supHeapFree(objinf);
            break;
        }

        if (0 == _strncmpi(
            objinf->TypeName.Buffer, 
            OBTYPE_NAME_DIRECTORY,
            objinf->TypeName.Length / sizeof(WCHAR)))
        {
            ListObjectDirectoryTree(
                objinf->Name.Buffer, 
                hDirectory, 
                ViewRootHandle);
        }

        supHeapFree(objinf);

    } while (TRUE);

    NtClose(hDirectory);
}

/*
* AddListViewItem
*
* Purpose:
*
* Add item to the object listview.
*
*/
VOID AddListViewItem(
    _In_ HANDLE hObjectRootDirectory,
    _In_ POBJECT_DIRECTORY_INFORMATION objinf
)
{
    BOOL    bFound = FALSE;
    INT     index;
    SIZE_T  cch;
    LVITEM  lvitem;
    WCHAR   szBuffer[MAX_PATH + 1];

    if (!objinf) return;

    RtlSecureZeroMemory(&lvitem, sizeof(lvitem));
    lvitem.mask = LVIF_TEXT | LVIF_IMAGE;
    lvitem.iSubItem = 0;
    lvitem.pszText = objinf->Name.Buffer;
    lvitem.iItem = MAXINT;
    lvitem.iImage = ObManagerGetImageIndexByTypeName(objinf->TypeName.Buffer);
    index = ListView_InsertItem(g_hwndObjectList, &lvitem);

    lvitem.mask = LVIF_TEXT;
    lvitem.iSubItem = 1;
    lvitem.pszText = objinf->TypeName.Buffer;
    lvitem.iItem = index;
    ListView_SetItem(g_hwndObjectList, &lvitem);

    cch = objinf->TypeName.Length / sizeof(WCHAR);
    RtlSecureZeroMemory(&szBuffer, sizeof(szBuffer));

    //check SymbolicLink
    if (_strncmpi(objinf->TypeName.Buffer, OBTYPE_NAME_SYMBOLIC_LINK, cch) == 0) {
       
        bFound = supQueryLinkTarget(hObjectRootDirectory,
            &objinf->Name, 
            szBuffer, 
            MAX_PATH * sizeof(WCHAR));
        
        goto Done;
    }

    //check Section
    if (_strncmpi(objinf->TypeName.Buffer, OBTYPE_NAME_SECTION, cch) == 0) {
        
        bFound = supQuerySectionFileInfo(hObjectRootDirectory,
            &objinf->Name, 
            szBuffer, 
            MAX_PATH);

        goto Done;
    }

    //check Driver
    if (_strncmpi(objinf->TypeName.Buffer, OBTYPE_NAME_DRIVER, cch) == 0) {
        
        bFound = supQueryDriverDescription(
            objinf->Name.Buffer,
            szBuffer, 
            MAX_PATH);

        goto Done;
    }

    //check Device
    if (_strncmpi(objinf->TypeName.Buffer, OBTYPE_NAME_DEVICE, cch) == 0) {
        
        bFound = supQueryDeviceDescription(
            objinf->Name.Buffer,
            szBuffer, 
            MAX_PATH);

        goto Done;
    }

    //check WindowStation
    if (_strncmpi(objinf->TypeName.Buffer, OBTYPE_NAME_WINSTATION, cch) == 0) {
        
        bFound = supQueryWinstationDescription(
            objinf->Name.Buffer,
            szBuffer, 
            MAX_PATH);

        goto Done;
    }

    //check Type
    if (_strncmpi(objinf->TypeName.Buffer, OBTYPE_NAME_TYPE, cch) == 0) {

        bFound = supQueryTypeInfo(
            objinf->Name.Buffer,
            szBuffer, 
            MAX_PATH);

    }

Done:
    //
    // Finally add information column if something found.
    //
    if (bFound != FALSE) {
        lvitem.mask = LVIF_TEXT;
        lvitem.iSubItem = 2;
        lvitem.pszText = szBuffer;
        lvitem.iItem = index;
        ListView_SetItem(g_hwndObjectList, &lvitem);
    }
}

/*
* ListObjectsInDirectory
*
* Purpose:
*
* List given directory to the listview.
*
*/
VOID ListObjectsInDirectory(
    _In_ LPWSTR lpObjectDirectory
)
{
    NTSTATUS            status;
    ULONG               ctx, rlen;
    HANDLE              hDirectory = NULL;
    OBJECT_ATTRIBUTES   objattr;
    UNICODE_STRING      objname;

    POBJECT_DIRECTORY_INFORMATION objinf;

    ListView_DeleteAllItems(g_hwndObjectList);
    RtlInitUnicodeString(&objname, lpObjectDirectory);
    InitializeObjectAttributes(&objattr, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = NtOpenDirectoryObject(&hDirectory, DIRECTORY_QUERY, &objattr);
    if (!NT_SUCCESS(status))
        return;

    ctx = 0;
    do {

        //
        // Wine implementation of NtQueryDirectoryObject interface is very basic and incomplete.
        // It doesn't work if no input buffer specified and does not return required buffer size.
        //
        if (g_WinObj.IsWine) {
            rlen = 1024 * 64;
        }
        else {
            rlen = 0;
            status = NtQueryDirectoryObject(hDirectory, NULL, 0, TRUE, FALSE, &ctx, &rlen);
            if (status != STATUS_BUFFER_TOO_SMALL)
                break;
        }

        objinf = (POBJECT_DIRECTORY_INFORMATION)supHeapAlloc((SIZE_T)rlen);
        if (objinf == NULL)
            break;

        status = NtQueryDirectoryObject(hDirectory, objinf, rlen, TRUE, FALSE, &ctx, &rlen);
        if (!NT_SUCCESS(status)) {
            supHeapFree(objinf);
            break;
        }

        AddListViewItem(hDirectory, objinf);

        supHeapFree(objinf);

    } while (TRUE);

    NtClose(hDirectory);
}

/*
* FindObject
*
* Purpose:
*
* Find object by given name in object directory.
*
*/
VOID FindObject(
    _In_ LPWSTR DirName,
    _In_opt_ LPWSTR NameSubstring,
    _In_opt_ LPWSTR TypeName,
    _In_ PFO_LIST_ITEM *List
)
{
    NTSTATUS            status;
    ULONG               ctx, rlen;
    HANDLE              hDirectory = NULL;
    SIZE_T              sdlen;
    LPWSTR              newdir;
    OBJECT_ATTRIBUTES   objattr;
    UNICODE_STRING      objname;
    PFO_LIST_ITEM       tmp;

    POBJECT_DIRECTORY_INFORMATION objinf;

    RtlInitUnicodeString(&objname, DirName);
    sdlen = _strlen(DirName);
    InitializeObjectAttributes(&objattr, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = NtOpenDirectoryObject(&hDirectory, DIRECTORY_QUERY, &objattr);
    if (!NT_SUCCESS(status))
        return;

    ctx = 0;
    do {
        //
        // Wine implementation of NtQueryDirectoryObject interface is very basic and incomplete.
        // It doesn't work if no input buffer specified and does not return required buffer size.
        //
        if (g_WinObj.IsWine != FALSE) {
            rlen = 1024 * 64;
        }
        else {
            rlen = 0;
            status = NtQueryDirectoryObject(hDirectory, NULL, 0, TRUE, FALSE, &ctx, &rlen);
            if (status != STATUS_BUFFER_TOO_SMALL)
                break;
        }

        objinf = (POBJECT_DIRECTORY_INFORMATION)supHeapAlloc((SIZE_T)rlen);
        if (objinf == NULL)
            break;

        status = NtQueryDirectoryObject(hDirectory, objinf, rlen, TRUE, FALSE, &ctx, &rlen);
        if (!NT_SUCCESS(status)) {
            supHeapFree(objinf);
            break;
        }

        if ((_strstri(objinf->Name.Buffer, NameSubstring) != 0) || (NameSubstring == NULL))
            if ((_strcmpi(objinf->TypeName.Buffer, TypeName) == 0) || (TypeName == NULL)) {

                tmp = (PFO_LIST_ITEM)supHeapAlloc(sizeof(FO_LIST_ITEM) +
                    objinf->Name.Length +
                    objinf->TypeName.Length +
                    (sdlen + 4) * sizeof(WCHAR));

                if (tmp == NULL) {
                    supHeapFree(objinf);
                    break;
                }
                tmp->Prev = *List;
                tmp->ObjectName = tmp->NameBuffer;
                tmp->ObjectType = tmp->NameBuffer + sdlen + 2 + objinf->Name.Length / sizeof(WCHAR);
                _strcpy(tmp->ObjectName, DirName);
                if ((DirName[0] == '\\') && (DirName[1] == 0)) {
                    _strncpy(tmp->ObjectName + sdlen, 1 + objinf->Name.Length / sizeof(WCHAR),
                        objinf->Name.Buffer, objinf->Name.Length / sizeof(WCHAR));
                }
                else {
                    tmp->ObjectName[sdlen] = '\\';
                    _strncpy(tmp->ObjectName + sdlen + 1, 1 + objinf->Name.Length / sizeof(WCHAR),
                        objinf->Name.Buffer, objinf->Name.Length / sizeof(WCHAR));
                }
                _strncpy(tmp->ObjectType, 1 + objinf->TypeName.Length / sizeof(WCHAR),
                    objinf->TypeName.Buffer, objinf->TypeName.Length / sizeof(WCHAR));
                *List = tmp;
            };

        if (_strcmpi(objinf->TypeName.Buffer, OBTYPE_NAME_DIRECTORY) == 0) {

            newdir = (LPWSTR)supHeapAlloc((sdlen + 4) * sizeof(WCHAR) + objinf->Name.Length);
            if (newdir != NULL) {
                _strcpy(newdir, DirName);
                if ((DirName[0] == '\\') && (DirName[1] == 0)) {
                    _strncpy(newdir + sdlen, 1 + objinf->Name.Length / sizeof(WCHAR),
                        objinf->Name.Buffer, objinf->Name.Length / sizeof(WCHAR));
                }
                else {
                    newdir[sdlen] = '\\';
                    _strncpy(newdir + sdlen + 1, 1 + objinf->Name.Length / sizeof(WCHAR),
                        objinf->Name.Buffer, objinf->Name.Length / sizeof(WCHAR));
                }
                FindObject(newdir, NameSubstring, TypeName, List);
                supHeapFree(newdir);
            }
        }

        supHeapFree(objinf);

    } while (TRUE);

    NtClose(hDirectory);
}

#include "stdafx.h"
#include "SmartRenameItem.h"
#include "helpers.h"

int CSmartRenameItem::s_id = 0;

IFACEMETHODIMP_(ULONG) CSmartRenameItem::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

IFACEMETHODIMP_(ULONG) CSmartRenameItem::Release()
{
    long refCount = InterlockedDecrement(&m_refCount);

    if (refCount == 0)
    {
        delete this;
    }
    return refCount;
}

IFACEMETHODIMP CSmartRenameItem::QueryInterface(_In_ REFIID riid, _Outptr_ void** ppv)
{
    static const QITAB qit[] = {
        QITABENT(CSmartRenameItem, ISmartRenameItem),
        QITABENT(CSmartRenameItem, ISmartRenameItemFactory),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP CSmartRenameItem::get_path(_Outptr_ PWSTR* path)
{
    CSRWSharedAutoLock lock(&m_lock);
    HRESULT hr = m_spsi ? S_OK : E_FAIL;
    if (SUCCEEDED(hr))
    {
        hr = m_spsi->GetDisplayName(SIGDN_FILESYSPATH, path);
    }
    return hr;
}

IFACEMETHODIMP CSmartRenameItem::get_shellItem(_Outptr_ IShellItem** ppsi)
{
    ppsi = nullptr;
    CSRWSharedAutoLock lock(&m_lock);
    HRESULT hr = (m_spsi) ? S_OK : E_FAIL;
    if (SUCCEEDED(hr))
    {
        hr = m_spsi->QueryInterface(IID_PPV_ARGS(ppsi));
    }
    return hr;
}

IFACEMETHODIMP CSmartRenameItem::get_originalName(_Outptr_ PWSTR* originalName)
{
    CSRWSharedAutoLock lock(&m_lock);
    HRESULT hr = m_spsi ? S_OK : E_FAIL;
    if (SUCCEEDED(hr))
    {
        hr = m_spsi->GetDisplayName(SIGDN_NORMALDISPLAY, originalName);
    }
    return hr;
}

IFACEMETHODIMP CSmartRenameItem::put_newName(_In_ PCWSTR newName)
{
    CSRWSharedAutoLock lock(&m_lock);
    CoTaskMemFree(m_newName);
    m_newName = StrDup(newName);
    return m_newName ? S_OK : E_OUTOFMEMORY;
}

IFACEMETHODIMP CSmartRenameItem::get_newName(_Outptr_ PWSTR* newName)
{
    CSRWSharedAutoLock lock(&m_lock);
    HRESULT hr = m_newName ? S_OK : E_FAIL;
    if (SUCCEEDED(hr))
    {
        *newName = StrDup(m_newName);
        hr = (*newName) ? S_OK : E_OUTOFMEMORY;
    }
    return hr;
}

IFACEMETHODIMP CSmartRenameItem::get_isFolder(_Out_ bool* isFolder)
{
    CSRWSharedAutoLock lock(&m_lock);
    *isFolder = m_isFolder;
    return S_OK;
}

IFACEMETHODIMP CSmartRenameItem::get_isSubFolderContent(_Out_ bool* isSubFolderContent)
{
    CSRWSharedAutoLock lock(&m_lock);
    *isSubFolderContent = m_isSubFolderContent;
    return S_OK;
}

IFACEMETHODIMP CSmartRenameItem::put_isSubFolderContent(_In_ bool isSubFolderContent)
{
    CSRWSharedAutoLock lock(&m_lock);
    m_isSubFolderContent = isSubFolderContent;
    return S_OK;
}

IFACEMETHODIMP CSmartRenameItem::get_isDirty(_Out_ bool* isDirty)
{
    CSRWSharedAutoLock lock(&m_lock);
    *isDirty = m_isDirty;
    return S_OK;
}

IFACEMETHODIMP CSmartRenameItem::get_shouldRename(_Out_ bool* shouldRename)
{
    CSRWSharedAutoLock lock(&m_lock);
    *shouldRename = m_shouldRename;
    return S_OK;
}

IFACEMETHODIMP CSmartRenameItem::put_shouldRename(_In_ bool shouldRename)
{
    CSRWSharedAutoLock lock(&m_lock);
    m_shouldRename = shouldRename;
    return S_OK;
}
IFACEMETHODIMP CSmartRenameItem::get_id(_Out_ int* id)
{
    CSRWSharedAutoLock lock(&m_lock);
    *id = m_id;
    return S_OK;
}

IFACEMETHODIMP CSmartRenameItem::get_iconIndex(_Out_ int* iconIndex)
{
    *iconIndex = m_iconIndex;
    return S_OK;
}

IFACEMETHODIMP CSmartRenameItem::get_depth(_Out_ UINT* depth)
{
    *depth = m_depth;
    return S_OK;
}

IFACEMETHODIMP CSmartRenameItem::put_depth(_In_ int depth)
{
    m_depth = depth;
    return S_OK;
}

IFACEMETHODIMP CSmartRenameItem::Reset()
{
    CSRWSharedAutoLock lock(&m_lock);
    CoTaskMemFree(m_newName);
    m_newName = nullptr;
    m_isDirty = false;
    return S_OK;
}

HRESULT CSmartRenameItem::s_CreateInstance(_In_opt_ IShellItem* psi, _In_ REFIID iid, _Outptr_ void** resultInterface)
{
    *resultInterface = nullptr;

    CSmartRenameItem *newRenameItem = new CSmartRenameItem();
    HRESULT hr = newRenameItem ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr))
    {
        if (psi != nullptr)
        {
            hr = newRenameItem->_Init(psi);
        }

        if (SUCCEEDED(hr))
        {
            hr = newRenameItem->QueryInterface(iid, resultInterface);
        }

        newRenameItem->Release();
    }
    return hr;
}

CSmartRenameItem::CSmartRenameItem() :
    m_refCount(1),
    m_id(++s_id)
{
}

CSmartRenameItem::~CSmartRenameItem()
{
    CoTaskMemFree(m_newName);
}

HRESULT CSmartRenameItem::_Init(_In_ IShellItem* psi)
{
    HRESULT hr = psi->QueryInterface(IID_PPV_ARGS(&m_spsi));
    if (SUCCEEDED(hr))
    {
        PWSTR path = nullptr;
        if (SUCCEEDED(get_path(&path)))
        {
            GetIconIndexFromPath(path, &m_iconIndex);
            CoTaskMemFree(path);
        }

        // Check if we are a folder now so we can check this attribute quickly later
        SFGAOF att = 0;
        if (SUCCEEDED(m_spsi->GetAttributes(SFGAO_STREAM | SFGAO_FOLDER, &att)))
        {
            // Some items can be both folders and streams (ex: zip folders).
            m_isFolder = (att & SFGAO_FOLDER) && !(att & SFGAO_STREAM);
        }
    }

    return hr;
}

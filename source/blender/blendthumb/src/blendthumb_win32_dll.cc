/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup blendthumb
 *
 * Thumbnail from Blend file extraction for MS-Windows (DLL).
 */

#include <new>
#include <objbase.h>
#include <shlobj.h> /* For #SHChangeNotify */
#include <shlwapi.h>
#include <thumbcache.h> /* For IThumbnailProvider */

extern HRESULT CBlendThumb_CreateInstance(REFIID riid, void **ppv);

#define SZ_CLSID_BLENDTHUMBHANDLER L"{D45F043D-F17F-4e8a-8435-70971D9FA46D}"
#define SZ_BLENDTHUMBHANDLER L"Blender Thumbnail Handler"
const CLSID CLSID_BlendThumbHandler = {
    0xd45f043d, 0xf17f, 0x4e8a, {0x84, 0x35, 0x70, 0x97, 0x1d, 0x9f, 0xa4, 0x6d}};

typedef HRESULT (*PFNCREATEINSTANCE)(REFIID riid, void **ppvObject);
struct CLASS_OBJECT_INIT {
  const CLSID *pClsid;
  PFNCREATEINSTANCE pfnCreate;
};

/* Add classes supported by this module here. */
const CLASS_OBJECT_INIT c_rgClassObjectInit[] = {
    {&CLSID_BlendThumbHandler, CBlendThumb_CreateInstance}};

long g_cRefModule = 0;

/** Handle the DLL's module */
HINSTANCE g_hInst = nullptr;

/** Standard DLL functions. */
STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void *)
{
  if (dwReason == DLL_PROCESS_ATTACH) {
    g_hInst = hInstance;
    DisableThreadLibraryCalls(hInstance);
  }
  return TRUE;
}

STDAPI DllCanUnloadNow()
{
  /* Only allow the DLL to be unloaded after all outstanding references have been released. */
  return (g_cRefModule == 0) ? S_OK : S_FALSE;
}

void DllAddRef()
{
  InterlockedIncrement(&g_cRefModule);
}

void DllRelease()
{
  InterlockedDecrement(&g_cRefModule);
}

class CClassFactory : public IClassFactory {
 public:
  static HRESULT CreateInstance(REFCLSID clsid,
                                const CLASS_OBJECT_INIT *pClassObjectInits,
                                size_t cClassObjectInits,
                                REFIID riid,
                                void **ppv)
  {
    *ppv = nullptr;
    HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;
    for (size_t i = 0; i < cClassObjectInits; i++) {
      if (clsid == *pClassObjectInits[i].pClsid) {
        IClassFactory *pClassFactory = new (std::nothrow)
            CClassFactory(pClassObjectInits[i].pfnCreate);
        hr = pClassFactory ? S_OK : E_OUTOFMEMORY;
        if (SUCCEEDED(hr)) {
          hr = pClassFactory->QueryInterface(riid, ppv);
          pClassFactory->Release();
        }
        /* Match found. */
        break;
      }
    }
    return hr;
  }

  CClassFactory(PFNCREATEINSTANCE pfnCreate) : _cRef(1), _pfnCreate(pfnCreate)
  {
    DllAddRef();
  }

  /** #IUnknown */
  IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
  {
    static const QITAB qit[] = {QITABENT(CClassFactory, IClassFactory), {0}};
    return QISearch(this, qit, riid, ppv);
  }

  IFACEMETHODIMP_(ULONG) AddRef()
  {
    return InterlockedIncrement(&_cRef);
  }

  IFACEMETHODIMP_(ULONG) Release()
  {
    long cRef = InterlockedDecrement(&_cRef);
    if (cRef == 0) {
      delete this;
    }
    return cRef;
  }

  /** #IClassFactory */
  IFACEMETHODIMP CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
  {
    return punkOuter ? CLASS_E_NOAGGREGATION : _pfnCreate(riid, ppv);
  }

  IFACEMETHODIMP LockServer(BOOL fLock)
  {
    if (fLock) {
      DllAddRef();
    }
    else {
      DllRelease();
    }
    return S_OK;
  }

 private:
  ~CClassFactory()
  {
    DllRelease();
  }

  long _cRef;
  PFNCREATEINSTANCE _pfnCreate;
};

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv)
{
  return CClassFactory::CreateInstance(
      clsid, c_rgClassObjectInit, ARRAYSIZE(c_rgClassObjectInit), riid, ppv);
}

/**
 * A struct to hold the information required for a registry entry.
 */
struct REGISTRY_ENTRY {
  HKEY hkeyRoot;
  PCWSTR pszKeyName;
  PCWSTR pszValueName;
  DWORD dwValueType;
  /** These two fields could/should have been a union, but C++ */
  PCWSTR pszData;
  /** Only lets you initialize the first field in a union. */
  DWORD dwData;
};

/**
 * Creates a registry key (if needed) and sets the default value of the key.
 */
HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY *pRegistryEntry)
{
  HKEY hKey;
  HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyExW(pRegistryEntry->hkeyRoot,
                                                  pRegistryEntry->pszKeyName,
                                                  0,
                                                  nullptr,
                                                  REG_OPTION_NON_VOLATILE,
                                                  KEY_SET_VALUE,
                                                  nullptr,
                                                  &hKey,
                                                  nullptr));
  if (SUCCEEDED(hr)) {
    /* All this just to support #REG_DWORD. */
    DWORD size;
    DWORD data;
    BYTE *lpData = (LPBYTE)pRegistryEntry->pszData;
    switch (pRegistryEntry->dwValueType) {
      case REG_SZ:
        size = ((DWORD)wcslen(pRegistryEntry->pszData) + 1) * sizeof(WCHAR);
        break;
      case REG_DWORD:
        size = sizeof(DWORD);
        data = pRegistryEntry->dwData;
        lpData = (BYTE *)&data;
        break;
      default:
        return E_INVALIDARG;
    }

    hr = HRESULT_FROM_WIN32(RegSetValueExW(
        hKey, pRegistryEntry->pszValueName, 0, pRegistryEntry->dwValueType, lpData, size));
    RegCloseKey(hKey);
  }
  return hr;
}

/**
 * Registers this COM server.
 */
STDAPI DllRegisterServer()
{
  HRESULT hr;

  WCHAR szModuleName[MAX_PATH];

  if (!GetModuleFileNameW(g_hInst, szModuleName, ARRAYSIZE(szModuleName))) {
    hr = HRESULT_FROM_WIN32(GetLastError());
  }
  else {
    const REGISTRY_ENTRY rgRegistryEntries[] = {
        /* `RootKey KeyName ValueName ValueType Data` */
        {HKEY_CURRENT_USER,
         L"Software\\Classes\\CLSID\\" SZ_CLSID_BLENDTHUMBHANDLER,
         nullptr,
         REG_SZ,
         SZ_BLENDTHUMBHANDLER},
        {HKEY_CURRENT_USER,
         L"Software\\Classes\\CLSID\\" SZ_CLSID_BLENDTHUMBHANDLER L"\\InProcServer32",
         nullptr,
         REG_SZ,
         szModuleName},
        {HKEY_CURRENT_USER,
         L"Software\\Classes\\CLSID\\" SZ_CLSID_BLENDTHUMBHANDLER L"\\InProcServer32",
         L"ThreadingModel",
         REG_SZ,
         L"Apartment"},
        {HKEY_CURRENT_USER,
         L"Software\\Classes\\.blend\\",
         L"Treatment",
         REG_DWORD,
         0,
         0}, /* This doesn't appear to do anything. */
        {HKEY_CURRENT_USER,
         L"Software\\Classes\\.blend\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}",
         nullptr,
         REG_SZ,
         SZ_CLSID_BLENDTHUMBHANDLER},
    };

    hr = S_OK;
    for (int i = 0; i < ARRAYSIZE(rgRegistryEntries) && SUCCEEDED(hr); i++) {
      hr = CreateRegKeyAndSetValue(&rgRegistryEntries[i]);
    }
  }
  if (SUCCEEDED(hr)) {
    /* This tells the shell to invalidate the thumbnail cache.
     * This is important because any `.blend` files viewed before registering this handler
     * would otherwise show cached blank thumbnails. */
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  }
  return hr;
}

/**
 * Unregisters this COM server
 */
STDAPI DllUnregisterServer()
{
  HRESULT hr = S_OK;

  const PCWSTR rgpszKeys[] = {
      L"Software\\Classes\\CLSID\\" SZ_CLSID_BLENDTHUMBHANDLER,
      L"Software\\Classes\\.blend\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}"};

  /* Delete the registry entries. */
  for (int i = 0; i < ARRAYSIZE(rgpszKeys) && SUCCEEDED(hr); i++) {
    hr = HRESULT_FROM_WIN32(RegDeleteTreeW(HKEY_CURRENT_USER, rgpszKeys[i]));
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
      /* If the registry entry has already been deleted, say S_OK. */
      hr = S_OK;
    }
  }
  return hr;
}

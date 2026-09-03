#include <windows.h>
#include <shobjidl.h>
#include <shldisp.h>
#include <urlmon.h>
#include <wininet.h>
#include <string>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#define ID_PATH_EDIT   101
#define ID_BROWSE_BTN  102
#define ID_TYPE_COMBO  103
#define ID_INSTALL_BTN 104

const std::wstring MELON_URL = L"https://github.com/LavaGang/MelonLoader/releases/latest/download/MelonLoader.x64.zip";
const std::wstring MOD_DLL_URL = L"https://github.com/aiden507/Stumble-Fun/releases/download/Mod/Stumble.Fun.dll";

std::wstring OpenFolderPicker(HWND hwnd) {
    std::wstring out = L"";
    IFileOpenDialog* pDlg = NULL;

    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&pDlg))) {
        DWORD flags;
        if (SUCCEEDED(pDlg->GetOptions(&flags))) {
            pDlg->SetOptions(flags | FOS_PICKFOLDERS);
        }

        if (SUCCEEDED(pDlg->Show(hwnd))) {
            IShellItem* pItem;
            if (SUCCEEDED(pDlg->GetResult(&pItem))) {
                PWSTR path = NULL;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    out = path;
                    CoTaskMemFree(path);
                }
                pItem->Release();
            }
        }
        pDlg->Release();
    }
    return out;
}

bool UnpackZip(const std::wstring& zipPath, const std::wstring& targetDir) {
    IShellDispatch* pShell = NULL;
    if (FAILED(CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void**)&pShell))) {
        return false;
    }

    bool status = false;
    Folder* pDest = NULL;

    VARIANT vDest;
    VariantInit(&vDest);
    vDest.vt = VT_BSTR;
    vDest.bstrVal = SysAllocString(targetDir.c_str());

    if (SUCCEEDED(pShell->NameSpace(vDest, &pDest))) {
        Folder* pSrc = NULL;

        VARIANT vSrc;
        VariantInit(&vSrc);
        vSrc.vt = VT_BSTR;
        vSrc.bstrVal = SysAllocString(zipPath.c_str());

        if (SUCCEEDED(pShell->NameSpace(vSrc, &pSrc))) {
            FolderItems* pItems = NULL;
            if (SUCCEEDED(pSrc->Items(&pItems))) {
                VARIANT vFlags;
                VariantInit(&vFlags);
                vFlags.vt = VT_I4;
                vFlags.lVal = 4 | 16;

                VARIANT vItem;
                VariantInit(&vItem);
                vItem.vt = VT_DISPATCH;
                vItem.pdispVal = pItems;

                if (SUCCEEDED(pDest->CopyHere(vItem, vFlags))) {
                    status = true;
                }
                pItems->Release();
            }
            pSrc->Release();
        }
        VariantClear(&vSrc);
        pDest->Release();
    }
    VariantClear(&vDest);
    pShell->Release();

    return status;
}

bool FetchFileToTemp(const std::wstring& url, const std::wstring& tempFileName, std::wstring& outPath) {
    WCHAR tempFolder[MAX_PATH];
    if (GetTempPath(MAX_PATH, tempFolder) == 0) return false;

    outPath = std::wstring(tempFolder) + tempFileName;

    DeleteUrlCacheEntry(url.c_str());
    DeleteFile(outPath.c_str());

    HRESULT hr = URLDownloadToFile(NULL, url.c_str(), outPath.c_str(), 0, NULL);
    return SUCCEEDED(hr);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hEdit, hCombo;

    switch (msg) {
    case WM_CREATE: {
        int x = 180;
        int w = 240;

        CreateWindow(L"STATIC", L"Game Path:", WS_CHILD | WS_VISIBLE | SS_CENTER, x, 90, w, 16, hwnd, NULL, NULL, NULL);
        hEdit = CreateWindow(L"EDIT", L"Put Stumble Guys Path Here", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, x, 110, 180, 20, hwnd, (HMENU)ID_PATH_EDIT, NULL, NULL);
        CreateWindow(L"BUTTON", L"Select", WS_CHILD | WS_VISIBLE, x + 185, 110, 55, 20, hwnd, (HMENU)ID_BROWSE_BTN, NULL, NULL);

        CreateWindow(L"STATIC", L"Download:", WS_CHILD | WS_VISIBLE | SS_CENTER, x, 160, w, 16, hwnd, NULL, NULL, NULL);
        hCombo = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, x, 180, w, 100, hwnd, (HMENU)ID_TYPE_COMBO, NULL, NULL);

        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Mod");
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);

        CreateWindow(L"BUTTON", L"Install", WS_CHILD | WS_VISIBLE, x + 40, 240, 160, 24, hwnd, (HMENU)ID_INSTALL_BTN, NULL, NULL);
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);

        if (id == ID_BROWSE_BTN) {
            std::wstring res = OpenFolderPicker(hwnd);
            if (!res.empty()) {
                SetWindowText(hEdit, res.c_str());
            }
        }
        else if (id == ID_INSTALL_BTN) {
            WCHAR buf[MAX_PATH];
            GetWindowText(hEdit, buf, MAX_PATH);
            std::wstring gameDir = buf;

            if (gameDir.empty()) {
                MessageBox(hwnd, L"Select a valid path.", L"Error", MB_ICONERROR);
                break;
            }

            UrlMkSetSessionOption(URLMON_OPTION_USERAGENT, (void*)"Mozilla/5.0", 11, 0);

            std::wstring tempZipPath;
            if (!FetchFileToTemp(MELON_URL, L"melon_loader_temp.zip", tempZipPath)) {
                MessageBox(hwnd, L"Failed downloading MelonLoader zip.", L"Error", MB_ICONERROR);
                break;
            }

            if (!UnpackZip(tempZipPath, gameDir)) {
                MessageBox(hwnd, L"Extraction failed.", L"Error", MB_ICONERROR);
                DeleteFile(tempZipPath.c_str());
                break;
            }
            DeleteFile(tempZipPath.c_str());

            std::wstring tempDllPath;
            if (!FetchFileToTemp(MOD_DLL_URL, L"mod_temp.dll", tempDllPath)) {
                MessageBox(hwnd, L"Failed to download DLL file.", L"Error", MB_ICONERROR);
                break;
            }

            std::wstring modsDir = gameDir + L"\\Mods";
            CreateDirectory(modsDir.c_str(), NULL);

            std::wstring targetDll = modsDir + L"\\mod.dll";
            if (MoveFileEx(tempDllPath.c_str(), targetDll.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                MessageBox(hwnd, L"Installation completed!", L"Success", MB_OK | MB_ICONINFORMATION);
            }
            else {
                MessageBox(hwnd, L"Failed moving DLL from temp to Mods folder.", L"Error", MB_ICONERROR);
                DeleteFile(tempDllPath.c_str());
            }
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    const WCHAR clsName[] = L"InstallerWndClass";

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = clsName;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, clsName, L"Installer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 450,
        NULL, NULL, hInst, NULL
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, show);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return 0;
}
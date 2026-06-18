#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commdlg.h>
#include <gdiplus.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <string>
#include <vector>

using namespace Gdiplus;

namespace
{
    const wchar_t kWindowClass[] = L"PhotoViewerMainWindow";
    const wchar_t kWindowTitle[] = L"Photo Viewer";
    const wchar_t kIconFileName[] = L"photo.ico";
    const int IDI_APP_ICON = 101;

    const int ID_FILE_OPEN = 1001;
    const int ID_FILE_EXIT = 1002;
    const int ID_VIEW_PREV = 1003;
    const int ID_VIEW_NEXT = 1004;
    const int ID_VIEW_ZOOM_IN = 1005;
    const int ID_VIEW_ZOOM_OUT = 1006;
    const int ID_VIEW_FIT = 1007;
    const int ID_VIEW_ACTUAL = 1008;
    const int ID_VIEW_FULLSCREEN = 1009;
    const int ID_VIEW_SLIDESHOW = 1010;

    const int ID_BTN_OPEN = 2000;
    const int ID_BTN_PREV = 2001;
    const int ID_BTN_NEXT = 2002;
    const int ID_BTN_ZOOM_OUT = 2003;
    const int ID_BTN_ZOOM_IN = 2004;
    const int ID_BTN_FIT = 2005;
    const int ID_BTN_ACTUAL = 2006;
    const int ID_BTN_FULLSCREEN = 2007;
    const int ID_BTN_SLIDESHOW = 2008;
    const int ID_BTN_CLOSE = 2009;

    const int ID_CTX_COPY_IMAGE = 3001;
    const int ID_CTX_COPY_PATH = 3002;
    const int ID_CTX_OPEN_LOCATION = 3003;
    const int ID_CTX_ZOOM_IN = 3004;
    const int ID_CTX_ZOOM_OUT = 3005;
    const int ID_CTX_FIT = 3006;
    const int ID_CTX_ACTUAL = 3007;
    const int ID_CTX_PREV = 3008;
    const int ID_CTX_NEXT = 3009;
    const int ID_CTX_FULLSCREEN = 3010;
    const int ID_CTX_SLIDESHOW = 3011;
    const int ID_CTX_SET_WALLPAPER = 3012;
    const int ID_CTX_SET_LOCKSCREEN = 3013;

    const int kToolbarHeight = 56;
    const int kStatusHeight = 30;
    const int kCanvasPadding = 14;
    const double kZoomStep = 1.2;
    const double kMinZoom = 0.05;
    const double kMaxZoom = 32.0;

    enum IconKind
    {
        IconOpen,
        IconPrev,
        IconNext,
        IconZoomOut,
        IconZoomIn,
        IconFit,
        IconActual,
        IconFullscreen,
        IconSlideshow,
        IconClose,
        IconImagePlaceholder
    };

    struct ToolbarItem
    {
        int id;
        const wchar_t* label;
        IconKind icon;
        RECT rect;
    };

    struct AppState
    {
        HWND hwnd = nullptr;
        ULONG_PTR gdiplusToken = 0;
        Bitmap* image = nullptr;
        std::wstring currentPath;
        std::vector<std::wstring> folderImages;
        int currentIndex = -1;

        double zoom = 1.0;
        bool fitToWindow = true;
        double panX = 0.0;
        double panY = 0.0;
        bool dragging = false;
        POINT dragStart = {};
        double panStartX = 0.0;
        double panStartY = 0.0;

        bool trackingMouse = false;
        int hotItem = 0;
        int pressedItem = 0;

        HFONT fontUi = nullptr;
        HFONT fontUiBold = nullptr;
        HFONT fontStatus = nullptr;
        HFONT fontEmptyTitle = nullptr;
        HFONT fontEmptyBody = nullptr;
        HFONT fontEmptyHint = nullptr;

        RECT toolbarRect = {};
        RECT statusRect = {};
        RECT viewportRect = {};
        RECT zoomValueRect = {};
        RECT closeButtonRect = {};
        std::array<ToolbarItem, 9> toolbarItems = {};
        HICON appIconLarge = nullptr;
        HICON appIconSmall = nullptr;
        bool isFullscreen = false;
        bool isSlideshow = false;
        bool slideshowForcedFullscreen = false;
        WINDOWPLACEMENT savedPlacement = {};
        DWORD savedStyle = 0;
        DWORD savedExStyle = 0;
        UINT_PTR slideshowTimerId = 1;
        UINT slideshowIntervalMs = 2500;
        bool prefersLightTheme = true;
    };

    AppState g_app;

    std::wstring ToLower(const std::wstring& value)
    {
        std::wstring result = value;
        std::transform(result.begin(), result.end(), result.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
        return result;
    }

    bool EndsWithImageExtension(const std::wstring& path)
    {
        const std::wstring lower = ToLower(path);
        return lower.size() > 4 &&
            (lower.rfind(L".jpg") == lower.size() - 4 ||
             lower.rfind(L".jpeg") == lower.size() - 5 ||
             lower.rfind(L".png") == lower.size() - 4 ||
             lower.rfind(L".bmp") == lower.size() - 4 ||
             lower.rfind(L".gif") == lower.size() - 4 ||
             lower.rfind(L".tif") == lower.size() - 4 ||
             lower.rfind(L".tiff") == lower.size() - 5 ||
             lower.rfind(L".ico") == lower.size() - 4);
    }

    std::wstring GetFileNameOnly(const std::wstring& path)
    {
        const size_t pos = path.find_last_of(L"\\/");
        return (pos == std::wstring::npos) ? path : path.substr(pos + 1);
    }

    std::wstring GetFolderOnly(const std::wstring& path)
    {
        const size_t pos = path.find_last_of(L"\\/");
        return (pos == std::wstring::npos) ? L"." : path.substr(0, pos);
    }

    std::wstring GetAbsolutePath(const std::wstring& path)
    {
        wchar_t buffer[MAX_PATH] = {};
        DWORD size = GetFullPathNameW(path.c_str(), MAX_PATH, buffer, nullptr);
        if (size == 0 || size >= MAX_PATH)
        {
            return path;
        }
        return buffer;
    }

    RECT MakeRect(int left, int top, int right, int bottom)
    {
        RECT rc = { left, top, right, bottom };
        return rc;
    }

    bool PointInRect(const RECT& rc, int x, int y)
    {
        return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
    }

    Color RgbColor(BYTE r, BYTE g, BYTE b, BYTE a = 255)
    {
        return Color(a, r, g, b);
    }

    void FillSolidRect(HDC hdc, const RECT& rc, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
    }

    template <typename T>
    T LoadProcAddress(HMODULE module, const char* name)
    {
        FARPROC proc = GetProcAddress(module, name);
        T fn = nullptr;
        std::memcpy(&fn, &proc, sizeof(fn));
        return fn;
    }

    void EnableDpiAwareness()
    {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32)
        {
            using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
            const auto setProcessDpiAwarenessContext =
                LoadProcAddress<SetProcessDpiAwarenessContextFn>(user32, "SetProcessDpiAwarenessContext");
            if (setProcessDpiAwarenessContext)
            {
                const HANDLE perMonitorV2 = reinterpret_cast<HANDLE>(-4);
                if (setProcessDpiAwarenessContext(perMonitorV2))
                {
                    return;
                }
            }
        }

        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        if (shcore)
        {
            using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
            const auto setProcessDpiAwareness =
                LoadProcAddress<SetProcessDpiAwarenessFn>(shcore, "SetProcessDpiAwareness");
            if (setProcessDpiAwareness)
            {
                const int processPerMonitorDpiAware = 2;
                if (SUCCEEDED(setProcessDpiAwareness(processPerMonitorDpiAware)))
                {
                    FreeLibrary(shcore);
                    return;
                }
            }
            FreeLibrary(shcore);
        }

        SetProcessDPIAware();
    }

    std::wstring GetModuleFolder()
    {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring modulePath = path;
        const size_t pos = modulePath.find_last_of(L"\\/");
        return (pos == std::wstring::npos) ? L"." : modulePath.substr(0, pos);
    }

    std::wstring GetIconPath()
    {
        return GetModuleFolder() + L"\\" + kIconFileName;
    }

    void LoadAppIcons(HINSTANCE hInstance)
    {
        g_app.appIconLarge = static_cast<HICON>(LoadImageW(
            hInstance,
            MAKEINTRESOURCEW(IDI_APP_ICON),
            IMAGE_ICON,
            32,
            32,
            LR_DEFAULTCOLOR));
        g_app.appIconSmall = static_cast<HICON>(LoadImageW(
            hInstance,
            MAKEINTRESOURCEW(IDI_APP_ICON),
            IMAGE_ICON,
            16,
            16,
            LR_DEFAULTCOLOR));

        if (!g_app.appIconLarge || !g_app.appIconSmall)
        {
            const std::wstring iconPath = GetIconPath();
            if (!g_app.appIconLarge)
            {
                g_app.appIconLarge = static_cast<HICON>(LoadImageW(
                    nullptr,
                    iconPath.c_str(),
                    IMAGE_ICON,
                    32,
                    32,
                    LR_LOADFROMFILE | LR_DEFAULTCOLOR));
            }
            if (!g_app.appIconSmall)
            {
                g_app.appIconSmall = static_cast<HICON>(LoadImageW(
                    nullptr,
                    iconPath.c_str(),
                    IMAGE_ICON,
                    16,
                    16,
                    LR_LOADFROMFILE | LR_DEFAULTCOLOR));
            }
        }
    }

    bool ReadWindowsAppsLightTheme()
    {
        HKEY key = nullptr;
        const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
        if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        {
            return true;
        }

        DWORD value = 1;
        DWORD size = sizeof(value);
        const LONG result = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(key);
        if (result != ERROR_SUCCESS)
        {
            return true;
        }
        return value != 0;
    }

    void RefreshThemePreference()
    {
        g_app.prefersLightTheme = ReadWindowsAppsLightTheme();
    }

    COLORREF GetCanvasBackgroundColor()
    {
        return g_app.prefersLightTheme ? RGB(244, 246, 249) : RGB(22, 24, 29);
    }

    Color GetCanvasFrameColor()
    {
        return g_app.prefersLightTheme ? RgbColor(214, 219, 228) : RgbColor(43, 47, 55);
    }

    Color GetEmptyIconColor()
    {
        return g_app.prefersLightTheme ? RgbColor(122, 132, 148) : RgbColor(88, 96, 112);
    }

    COLORREF GetEmptyTitleTextColor()
    {
        return g_app.prefersLightTheme ? RGB(52, 58, 67) : RGB(239, 242, 247);
    }

    COLORREF GetEmptyBodyTextColor()
    {
        return g_app.prefersLightTheme ? RGB(90, 98, 110) : RGB(171, 178, 189);
    }

    COLORREF GetEmptyHintTextColor()
    {
        return g_app.prefersLightTheme ? RGB(126, 133, 145) : RGB(125, 132, 143);
    }

    void CreateFonts()
    {
        HDC screenDc = GetDC(g_app.hwnd);
        const int dpi = screenDc ? GetDeviceCaps(screenDc, LOGPIXELSY) : 96;
        if (screenDc)
        {
            ReleaseDC(g_app.hwnd, screenDc);
        }
        const int uiHeight = -MulDiv(13, dpi, 96);
        const int statusHeight = -MulDiv(12, dpi, 96);
        const int emptyTitleHeight = -MulDiv(24, dpi, 96);
        const int emptyBodyHeight = -MulDiv(14, dpi, 96);
        const int emptyHintHeight = -MulDiv(12, dpi, 96);

        g_app.fontUi = CreateFontW(uiHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.fontUiBold = CreateFontW(uiHeight, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.fontStatus = CreateFontW(statusHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.fontEmptyTitle = CreateFontW(emptyTitleHeight, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.fontEmptyBody = CreateFontW(emptyBodyHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_app.fontEmptyHint = CreateFontW(emptyHintHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    void DestroyFonts()
    {
        const HICON largeIcon = g_app.appIconLarge;
        const HICON smallIcon = g_app.appIconSmall;
        DeleteObject(g_app.fontUi);
        DeleteObject(g_app.fontUiBold);
        DeleteObject(g_app.fontStatus);
        DeleteObject(g_app.fontEmptyTitle);
        DeleteObject(g_app.fontEmptyBody);
        DeleteObject(g_app.fontEmptyHint);
        if (largeIcon)
        {
            DestroyIcon(largeIcon);
        }
        if (smallIcon && smallIcon != largeIcon)
        {
            DestroyIcon(smallIcon);
        }
        g_app.appIconLarge = nullptr;
        g_app.appIconSmall = nullptr;
        g_app.fontUi = nullptr;
        g_app.fontUiBold = nullptr;
        g_app.fontStatus = nullptr;
        g_app.fontEmptyTitle = nullptr;
        g_app.fontEmptyBody = nullptr;
        g_app.fontEmptyHint = nullptr;
    }

    void ReleaseImage()
    {
        delete g_app.image;
        g_app.image = nullptr;
    }

    RECT GetViewportRect(HWND hwnd)
    {
        RECT client = {};
        GetClientRect(hwnd, &client);
        const int toolbarHeight = g_app.isFullscreen ? 0 : kToolbarHeight;
        const int statusHeight = g_app.isFullscreen ? 0 : kStatusHeight;
        const int padding = g_app.isFullscreen ? 0 : kCanvasPadding;
        client.top += toolbarHeight;
        client.bottom -= statusHeight;

        client.left += padding;
        client.right -= padding;
        client.top += padding;
        client.bottom -= padding;

        if (client.right < client.left)
        {
            client.right = client.left;
        }
        if (client.bottom < client.top)
        {
            client.bottom = client.top;
        }
        return client;
    }

    void LayoutUi(HWND hwnd)
    {
        RECT client = {};
        GetClientRect(hwnd, &client);

        const int toolbarHeight = g_app.isFullscreen ? 0 : kToolbarHeight;
        const int statusHeight = g_app.isFullscreen ? 0 : kStatusHeight;
        g_app.toolbarRect = MakeRect(0, 0, client.right, toolbarHeight);
        g_app.statusRect = MakeRect(0, client.bottom - statusHeight, client.right, client.bottom);
        g_app.viewportRect = GetViewportRect(hwnd);

        if (g_app.isFullscreen)
        {
            g_app.closeButtonRect = {};
            return;
        }

        const int top = 10;
        const int height = 34;
        const int gap = 6;
        const int groupGap = 18;
        int x = 14;

        g_app.toolbarItems[0] = { ID_BTN_OPEN, L"Open", IconOpen, MakeRect(x, top, x + 92, top + height) };
        x += 92 + gap;
        g_app.toolbarItems[1] = { ID_BTN_PREV, L"Previous", IconPrev, MakeRect(x, top, x + 96, top + height) };
        x += 96 + gap;
        g_app.toolbarItems[2] = { ID_BTN_NEXT, L"Next", IconNext, MakeRect(x, top, x + 82, top + height) };
        x += 82 + groupGap;
        g_app.toolbarItems[3] = { ID_BTN_ZOOM_OUT, L"Zoom Out", IconZoomOut, MakeRect(x, top, x + 96, top + height) };
        x += 96 + gap;
        g_app.zoomValueRect = MakeRect(x, top, x + 72, top + height);
        x += 72 + gap;
        g_app.toolbarItems[4] = { ID_BTN_ZOOM_IN, L"Zoom In", IconZoomIn, MakeRect(x, top, x + 92, top + height) };
        x += 92 + groupGap;
        g_app.toolbarItems[5] = { ID_BTN_FIT, L"Fit to Window", IconFit, MakeRect(x, top, x + 122, top + height) };
        x += 122 + gap;
        g_app.toolbarItems[6] = { ID_BTN_ACTUAL, L"Actual Size", IconActual, MakeRect(x, top, x + 110, top + height) };
        x += 110 + groupGap;
        g_app.toolbarItems[7] = { ID_BTN_FULLSCREEN, L"Full Screen", IconFullscreen, MakeRect(x, top, x + 116, top + height) };
        x += 116 + gap;
        g_app.toolbarItems[8] = { ID_BTN_SLIDESHOW, L"Slide Show", IconSlideshow, MakeRect(x, top, x + 108, top + height) };

        g_app.closeButtonRect = MakeRect(client.right - 104, 10, client.right - 12, 44);
    }

    double GetFitZoom(const RECT& viewport)
    {
        if (!g_app.image)
        {
            return 1.0;
        }

        const UINT imgW = g_app.image->GetWidth();
        const UINT imgH = g_app.image->GetHeight();
        const int viewW = std::max(1L, viewport.right - viewport.left);
        const int viewH = std::max(1L, viewport.bottom - viewport.top);

        if (imgW == 0 || imgH == 0)
        {
            return 1.0;
        }

        const double scaleX = static_cast<double>(viewW) / static_cast<double>(imgW);
        const double scaleY = static_cast<double>(viewH) / static_cast<double>(imgH);
        return std::max(kMinZoom, std::min(scaleX, scaleY));
    }

    double GetDisplayZoom(const RECT& viewport)
    {
        return g_app.fitToWindow ? GetFitZoom(viewport) : g_app.zoom;
    }

    void ClampPan(const RECT& viewport, double displayZoom)
    {
        if (!g_app.image)
        {
            g_app.panX = 0.0;
            g_app.panY = 0.0;
            return;
        }

        const double drawW = static_cast<double>(g_app.image->GetWidth()) * displayZoom;
        const double drawH = static_cast<double>(g_app.image->GetHeight()) * displayZoom;
        const double viewW = static_cast<double>(viewport.right - viewport.left);
        const double viewH = static_cast<double>(viewport.bottom - viewport.top);

        if (drawW <= viewW)
        {
            g_app.panX = 0.0;
        }
        else
        {
            const double limitX = (drawW - viewW) / 2.0;
            if (g_app.panX < -limitX) g_app.panX = -limitX;
            if (g_app.panX > limitX) g_app.panX = limitX;
        }

        if (drawH <= viewH)
        {
            g_app.panY = 0.0;
        }
        else
        {
            const double limitY = (drawH - viewH) / 2.0;
            if (g_app.panY < -limitY) g_app.panY = -limitY;
            if (g_app.panY > limitY) g_app.panY = limitY;
        }
    }

    RECT GetImageDrawRect(HWND hwnd)
    {
        RECT viewport = GetViewportRect(hwnd);
        RECT rect = viewport;
        if (!g_app.image)
        {
            return rect;
        }

        const double displayZoom = GetDisplayZoom(viewport);
        ClampPan(viewport, displayZoom);

        const double drawW = static_cast<double>(g_app.image->GetWidth()) * displayZoom;
        const double drawH = static_cast<double>(g_app.image->GetHeight()) * displayZoom;
        const double viewW = static_cast<double>(viewport.right - viewport.left);
        const double viewH = static_cast<double>(viewport.bottom - viewport.top);

        const double left = viewport.left + (viewW - drawW) / 2.0 + g_app.panX;
        const double top = viewport.top + (viewH - drawH) / 2.0 + g_app.panY;

        rect.left = static_cast<LONG>(left);
        rect.top = static_cast<LONG>(top);
        rect.right = static_cast<LONG>(left + drawW);
        rect.bottom = static_cast<LONG>(top + drawH);
        return rect;
    }

    int GetZoomPercent()
    {
        const double zoomValue = GetDisplayZoom(g_app.viewportRect);
        return static_cast<int>(zoomValue * 100.0 + 0.5);
    }

    std::wstring BuildWindowTitle()
    {
        return g_app.currentPath.empty() ? std::wstring(kWindowTitle) : GetFileNameOnly(g_app.currentPath);
    }

    void UpdateWindowTitle()
    {
        SetWindowTextW(g_app.hwnd, BuildWindowTitle().c_str());
    }

    bool IsImageLoaded()
    {
        return g_app.image != nullptr;
    }

    bool IsToolbarEnabled(int id)
    {
        switch (id)
        {
        case ID_BTN_OPEN:
            return true;
        case ID_BTN_PREV:
            return IsImageLoaded() && g_app.currentIndex > 0;
        case ID_BTN_NEXT:
            return IsImageLoaded() &&
                g_app.currentIndex >= 0 &&
                g_app.currentIndex < static_cast<int>(g_app.folderImages.size()) - 1;
        case ID_BTN_ZOOM_OUT:
        case ID_BTN_ZOOM_IN:
        case ID_BTN_FIT:
        case ID_BTN_ACTUAL:
            return IsImageLoaded();
        case ID_BTN_FULLSCREEN:
            return true;
        case ID_BTN_SLIDESHOW:
            return IsImageLoaded() && g_app.folderImages.size() > 1;
        case ID_BTN_CLOSE:
            return true;
        default:
            return false;
        }
    }

    void InvalidateViewer()
    {
        UpdateWindowTitle();
        InvalidateRect(g_app.hwnd, nullptr, FALSE);
    }

    void ScanFolderImages(const std::wstring& filePath)
    {
        g_app.folderImages.clear();
        g_app.currentIndex = -1;

        const std::wstring absolutePath = GetAbsolutePath(filePath);
        const std::wstring folder = GetFolderOnly(absolutePath);
        WIN32_FIND_DATAW findData = {};
        HANDLE handle = FindFirstFileW((folder + L"\\*").c_str(), &findData);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                continue;
            }

            const std::wstring name = findData.cFileName;
            if (!EndsWithImageExtension(name))
            {
                continue;
            }

            g_app.folderImages.push_back(folder + L"\\" + name);
        }
        while (FindNextFileW(handle, &findData));

        FindClose(handle);

        std::sort(g_app.folderImages.begin(), g_app.folderImages.end(),
            [](const std::wstring& a, const std::wstring& b)
            {
                return ToLower(a) < ToLower(b);
            });

        for (size_t i = 0; i < g_app.folderImages.size(); ++i)
        {
            if (ToLower(g_app.folderImages[i]) == ToLower(absolutePath))
            {
                g_app.currentIndex = static_cast<int>(i);
                break;
            }
        }
    }

    bool LoadImageFile(const std::wstring& filePath, bool showErrors)
    {
        const std::wstring absolutePath = GetAbsolutePath(filePath);
        ReleaseImage();

        Bitmap* newBitmap = new Bitmap(absolutePath.c_str());
        if (!newBitmap || newBitmap->GetLastStatus() != Ok)
        {
            delete newBitmap;
            g_app.currentPath.clear();
            g_app.folderImages.clear();
            g_app.currentIndex = -1;
            g_app.fitToWindow = true;
            g_app.zoom = 1.0;
            g_app.panX = 0.0;
            g_app.panY = 0.0;
            InvalidateViewer();

            if (showErrors)
            {
                MessageBoxW(g_app.hwnd,
                    L"Cannot open this file or the format is unsupported.",
                    kWindowTitle,
                    MB_ICONERROR | MB_OK);
            }
            return false;
        }

        g_app.image = newBitmap;
        g_app.currentPath = absolutePath;
        g_app.fitToWindow = true;
        g_app.zoom = 1.0;
        g_app.panX = 0.0;
        g_app.panY = 0.0;
        ScanFolderImages(absolutePath);
        InvalidateViewer();
        return true;
    }

    void OpenImageDialog()
    {
        wchar_t fileName[MAX_PATH] = {};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_app.hwnd;
        ofn.lpstrFilter =
            L"Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff;*.ico\0"
            L"All Files\0*.*\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

        if (GetOpenFileNameW(&ofn))
        {
            LoadImageFile(fileName, true);
        }
    }

    void NavigateRelative(int delta)
    {
        if (g_app.folderImages.empty() || g_app.currentIndex < 0)
        {
            return;
        }

        const int nextIndex = g_app.currentIndex + delta;
        if (nextIndex < 0 || nextIndex >= static_cast<int>(g_app.folderImages.size()))
        {
            return;
        }

        LoadImageFile(g_app.folderImages[nextIndex], true);
    }

    void ZoomAt(double factor, int anchorX, int anchorY)
    {
        if (!g_app.image)
        {
            return;
        }

        RECT viewport = GetViewportRect(g_app.hwnd);
        const double oldDisplayZoom = GetDisplayZoom(viewport);
        double newZoom = oldDisplayZoom * factor;
        if (newZoom < kMinZoom) newZoom = kMinZoom;
        if (newZoom > kMaxZoom) newZoom = kMaxZoom;

        const double centerX = (viewport.left + viewport.right) / 2.0;
        const double centerY = (viewport.top + viewport.bottom) / 2.0;
        const double relativeX = static_cast<double>(anchorX) - centerX - g_app.panX;
        const double relativeY = static_cast<double>(anchorY) - centerY - g_app.panY;

        g_app.fitToWindow = false;
        g_app.zoom = newZoom;

        if (oldDisplayZoom > 0.0)
        {
            const double scaleRatio = newZoom / oldDisplayZoom;
            g_app.panX -= relativeX * (scaleRatio - 1.0);
            g_app.panY -= relativeY * (scaleRatio - 1.0);
        }

        ClampPan(viewport, g_app.zoom);
        InvalidateViewer();
    }

    void FitToWindow()
    {
        if (!g_app.image)
        {
            return;
        }
        g_app.fitToWindow = true;
        g_app.panX = 0.0;
        g_app.panY = 0.0;
        InvalidateViewer();
    }

    void ActualSize()
    {
        if (!g_app.image)
        {
            return;
        }
        g_app.fitToWindow = false;
        g_app.zoom = 1.0;
        g_app.panX = 0.0;
        g_app.panY = 0.0;
        InvalidateViewer();
    }

    bool CopyTextToClipboard(const std::wstring& text)
    {
        if (!OpenClipboard(g_app.hwnd))
        {
            return false;
        }

        EmptyClipboard();
        const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory)
        {
            CloseClipboard();
            return false;
        }

        void* dest = GlobalLock(memory);
        if (!dest)
        {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }

        memcpy(dest, text.c_str(), bytes);
        GlobalUnlock(memory);

        if (!SetClipboardData(CF_UNICODETEXT, memory))
        {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }

        CloseClipboard();
        return true;
    }

    bool CopyImageToClipboard()
    {
        if (!g_app.image)
        {
            return false;
        }

        HBITMAP hBitmap = nullptr;
        if (g_app.image->GetHBITMAP(Color::Black, &hBitmap) != Ok || !hBitmap)
        {
            return false;
        }

        if (!OpenClipboard(g_app.hwnd))
        {
            DeleteObject(hBitmap);
            return false;
        }

        EmptyClipboard();
        if (!SetClipboardData(CF_BITMAP, hBitmap))
        {
            DeleteObject(hBitmap);
            CloseClipboard();
            return false;
        }

        CloseClipboard();
        return true;
    }

    void OpenFileLocation()
    {
        if (g_app.currentPath.empty())
        {
            return;
        }

        std::wstring args = L"/select,\"" + g_app.currentPath + L"\"";
        ShellExecuteW(g_app.hwnd, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }

    void HandleCopyImage()
    {
        if (!CopyImageToClipboard())
        {
            MessageBoxW(g_app.hwnd, L"Copy image failed.", kWindowTitle, MB_ICONERROR | MB_OK);
        }
    }

    void HandleCopyPath()
    {
        if (g_app.currentPath.empty() || !CopyTextToClipboard(g_app.currentPath))
        {
            MessageBoxW(g_app.hwnd, L"Copy file path failed.", kWindowTitle, MB_ICONERROR | MB_OK);
        }
    }

    bool RunHiddenProcessAndWait(const std::wstring& commandLine, DWORD timeoutMs, DWORD* exitCode)
    {
        STARTUPINFOW si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::wstring mutableCommandLine = commandLine;
        if (!CreateProcessW(nullptr, mutableCommandLine.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            return false;
        }

        const DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
        bool ok = false;
        if (waitResult == WAIT_OBJECT_0)
        {
            ok = GetExitCodeProcess(pi.hProcess, exitCode);
        }
        else
        {
            TerminateProcess(pi.hProcess, 1);
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return ok && waitResult == WAIT_OBJECT_0;
    }

    std::wstring EscapeForPowerShellSingleQuote(const std::wstring& value)
    {
        std::wstring escaped;
        escaped.reserve(value.size() + 8);
        for (wchar_t ch : value)
        {
            if (ch == L'\'')
            {
                escaped += L"''";
            }
            else
            {
                escaped += ch;
            }
        }
        return escaped;
    }

    bool SetDesktopBackground()
    {
        if (g_app.currentPath.empty())
        {
            return false;
        }

        return SystemParametersInfoW(
            SPI_SETDESKWALLPAPER,
            0,
            const_cast<wchar_t*>(g_app.currentPath.c_str()),
            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE) != FALSE;
    }

    bool SetLockScreenBackgroundBestEffort()
    {
        if (g_app.currentPath.empty())
        {
            return false;
        }

        const std::wstring escapedPath = EscapeForPowerShellSingleQuote(g_app.currentPath);
        const std::wstring script =
            L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -Command "
            L"\"Add-Type -AssemblyName System.Runtime.WindowsRuntime; "
            L"$null=[Windows.Storage.StorageFile,Windows.Storage,ContentType=WindowsRuntime]; "
            L"$null=[Windows.System.UserProfile.LockScreen,Windows.System.UserProfile,ContentType=WindowsRuntime]; "
            L"$fileOp=[Windows.Storage.StorageFile]::GetFileFromPathAsync('" + escapedPath + L"'); "
            L"$file=[System.WindowsRuntimeSystemExtensions]::AsTask($fileOp).Result; "
            L"$setOp=[Windows.System.UserProfile.LockScreen]::SetImageFileAsync($file); "
            L"[System.WindowsRuntimeSystemExtensions]::AsTask($setOp).Wait()\"";

        DWORD exitCode = 1;
        return RunHiddenProcessAndWait(script, 15000, &exitCode) && exitCode == 0;
    }

    void ToggleFullscreen(bool forceState);

    void StopSlideshow()
    {
        if (!g_app.isSlideshow)
        {
            return;
        }

        KillTimer(g_app.hwnd, g_app.slideshowTimerId);
        g_app.isSlideshow = false;
        const bool shouldExitFullscreen = g_app.slideshowForcedFullscreen;
        g_app.slideshowForcedFullscreen = false;
        if (shouldExitFullscreen && g_app.isFullscreen)
        {
            ToggleFullscreen(false);
        }
        else
        {
            InvalidateViewer();
        }
    }

    void StartSlideshow()
    {
        if (!g_app.image || g_app.folderImages.size() <= 1)
        {
            return;
        }

        if (g_app.isSlideshow)
        {
            return;
        }

        g_app.slideshowForcedFullscreen = !g_app.isFullscreen;
        if (!g_app.isFullscreen)
        {
            ToggleFullscreen(true);
        }

        g_app.isSlideshow = true;
        SetTimer(g_app.hwnd, g_app.slideshowTimerId, g_app.slideshowIntervalMs, nullptr);
        InvalidateViewer();
    }

    void ToggleSlideshow()
    {
        if (g_app.isSlideshow)
        {
            StopSlideshow();
        }
        else
        {
            StartSlideshow();
        }
    }

    void ToggleFullscreen(bool forceState)
    {
        if (forceState == g_app.isFullscreen)
        {
            return;
        }

        if (forceState)
        {
            g_app.savedStyle = static_cast<DWORD>(GetWindowLongW(g_app.hwnd, GWL_STYLE));
            g_app.savedExStyle = static_cast<DWORD>(GetWindowLongW(g_app.hwnd, GWL_EXSTYLE));
            g_app.savedPlacement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(g_app.hwnd, &g_app.savedPlacement);

            MONITORINFO monitorInfo = {};
            monitorInfo.cbSize = sizeof(monitorInfo);
            GetMonitorInfoW(MonitorFromWindow(g_app.hwnd, MONITOR_DEFAULTTONEAREST), &monitorInfo);

            SetWindowLongW(g_app.hwnd, GWL_STYLE, g_app.savedStyle & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU));
            SetWindowLongW(g_app.hwnd, GWL_EXSTYLE, g_app.savedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE));
            SetWindowPos(g_app.hwnd, HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            g_app.isFullscreen = true;
        }
        else
        {
            SetWindowLongW(g_app.hwnd, GWL_STYLE, g_app.savedStyle);
            SetWindowLongW(g_app.hwnd, GWL_EXSTYLE, g_app.savedExStyle);
            SetWindowPlacement(g_app.hwnd, &g_app.savedPlacement);
            SetWindowPos(g_app.hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            g_app.isFullscreen = false;
        }

        LayoutUi(g_app.hwnd);
        InvalidateViewer();
    }

    void DrawIcon(Graphics& graphics, const RectF& rc, IconKind icon, const Color& color)
    {
        Pen pen(color, 1.7f);
        pen.SetLineJoin(LineJoinRound);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        SolidBrush brush(color);

        const float x = rc.X;
        const float y = rc.Y;
        const float w = rc.Width;
        const float h = rc.Height;
        const float midX = x + w / 2.0f;
        const float midY = y + h / 2.0f;

        switch (icon)
        {
        case IconOpen:
        {
            GraphicsPath path;
            path.AddLine(x + 2.0f, y + 5.0f, x + w * 0.40f, y + 5.0f);
            path.AddLine(x + w * 0.40f, y + 5.0f, x + w * 0.48f, y + 2.0f);
            path.AddLine(x + w * 0.48f, y + 2.0f, x + w - 3.0f, y + 2.0f);
            path.AddLine(x + w - 3.0f, y + 2.0f, x + w - 3.0f, y + h - 5.0f);
            path.AddLine(x + w - 3.0f, y + h - 5.0f, x + 2.0f, y + h - 5.0f);
            path.CloseFigure();
            graphics.DrawPath(&pen, &path);
            graphics.DrawLine(&pen, x + 3.0f, y + 8.0f, x + w - 3.0f, y + 8.0f);
            break;
        }
        case IconPrev:
            graphics.DrawLine(&pen, x + w - 3.0f, y + 2.0f, x + 4.0f, midY);
            graphics.DrawLine(&pen, x + 4.0f, midY, x + w - 3.0f, y + h - 2.0f);
            break;
        case IconNext:
            graphics.DrawLine(&pen, x + 3.0f, y + 2.0f, x + w - 4.0f, midY);
            graphics.DrawLine(&pen, x + w - 4.0f, midY, x + 3.0f, y + h - 2.0f);
            break;
        case IconZoomOut:
            graphics.DrawEllipse(&pen, x + 2.0f, y + 2.0f, w - 7.0f, h - 7.0f);
            graphics.DrawLine(&pen, x + w - 6.0f, y + h - 6.0f, x + w - 1.0f, y + h - 1.0f);
            graphics.DrawLine(&pen, x + 5.0f, midY, x + w - 10.0f, midY);
            break;
        case IconZoomIn:
            graphics.DrawEllipse(&pen, x + 2.0f, y + 2.0f, w - 7.0f, h - 7.0f);
            graphics.DrawLine(&pen, x + w - 6.0f, y + h - 6.0f, x + w - 1.0f, y + h - 1.0f);
            graphics.DrawLine(&pen, x + 5.0f, midY, x + w - 10.0f, midY);
            graphics.DrawLine(&pen, midX - 2.0f, y + 5.0f, midX - 2.0f, y + h - 10.0f);
            break;
        case IconFit:
            graphics.DrawLine(&pen, x + 3.0f, y + 7.0f, x + 3.0f, y + 3.0f);
            graphics.DrawLine(&pen, x + 3.0f, y + 3.0f, x + 7.0f, y + 3.0f);
            graphics.DrawLine(&pen, x + w - 3.0f, y + 7.0f, x + w - 3.0f, y + 3.0f);
            graphics.DrawLine(&pen, x + w - 3.0f, y + 3.0f, x + w - 7.0f, y + 3.0f);
            graphics.DrawLine(&pen, x + 3.0f, y + h - 7.0f, x + 3.0f, y + h - 3.0f);
            graphics.DrawLine(&pen, x + 3.0f, y + h - 3.0f, x + 7.0f, y + h - 3.0f);
            graphics.DrawLine(&pen, x + w - 3.0f, y + h - 7.0f, x + w - 3.0f, y + h - 3.0f);
            graphics.DrawLine(&pen, x + w - 3.0f, y + h - 3.0f, x + w - 7.0f, y + h - 3.0f);
            break;
        case IconActual:
        {
            graphics.DrawRectangle(&pen, x + 2.5f, y + 2.5f, w - 5.0f, h - 5.0f);
            StringFormat format;
            format.SetAlignment(StringAlignmentCenter);
            format.SetLineAlignment(StringAlignmentCenter);
            Font font(L"Segoe UI", 8.0f, FontStyleBold, UnitPixel);
            RectF textRect(x, y, w, h);
            graphics.DrawString(L"1:1", -1, &font, textRect, &format, &brush);
            break;
        }
        case IconFullscreen:
            graphics.DrawLine(&pen, x + 3.0f, y + 8.0f, x + 3.0f, y + 3.0f);
            graphics.DrawLine(&pen, x + 3.0f, y + 3.0f, x + 8.0f, y + 3.0f);
            graphics.DrawLine(&pen, x + w - 3.0f, y + 8.0f, x + w - 3.0f, y + 3.0f);
            graphics.DrawLine(&pen, x + w - 3.0f, y + 3.0f, x + w - 8.0f, y + 3.0f);
            graphics.DrawLine(&pen, x + 3.0f, y + h - 8.0f, x + 3.0f, y + h - 3.0f);
            graphics.DrawLine(&pen, x + 3.0f, y + h - 3.0f, x + 8.0f, y + h - 3.0f);
            graphics.DrawLine(&pen, x + w - 3.0f, y + h - 8.0f, x + w - 3.0f, y + h - 3.0f);
            graphics.DrawLine(&pen, x + w - 3.0f, y + h - 3.0f, x + w - 8.0f, y + h - 3.0f);
            break;
        case IconSlideshow:
        {
            PointF triangle[] = {
                PointF(x + 5.0f, y + 3.0f),
                PointF(x + w - 4.0f, midY),
                PointF(x + 5.0f, y + h - 3.0f)
            };
            graphics.FillPolygon(&brush, triangle, 3);
            break;
        }
        case IconClose:
            graphics.DrawLine(&pen, x + 3.0f, y + 3.0f, x + w - 3.0f, y + h - 3.0f);
            graphics.DrawLine(&pen, x + w - 3.0f, y + 3.0f, x + 3.0f, y + h - 3.0f);
            break;
        case IconImagePlaceholder:
        {
            Pen framePen(RgbColor(88, 96, 112), 2.0f);
            graphics.DrawRectangle(&framePen, x + 3.0f, y + 3.0f, w - 6.0f, h - 6.0f);
            graphics.FillEllipse(&brush, x + w - 18.0f, y + 10.0f, 8.0f, 8.0f);
            PointF points[] = {
                PointF(x + 10.0f, y + h - 14.0f),
                PointF(x + 22.0f, y + h - 28.0f),
                PointF(x + 34.0f, y + h - 17.0f),
                PointF(x + 48.0f, y + h - 35.0f),
                PointF(x + w - 10.0f, y + h - 14.0f)
            };
            graphics.DrawLines(&framePen, points, 5);
            break;
        }
        }
    }

    void DrawRoundedPanel(Graphics& graphics, const RECT& rc, int radius, const Color& fill, const Color& border)
    {
        GraphicsPath path;
        const REAL left = static_cast<REAL>(rc.left);
        const REAL top = static_cast<REAL>(rc.top);
        const REAL width = static_cast<REAL>(rc.right - rc.left);
        const REAL height = static_cast<REAL>(rc.bottom - rc.top);
        const REAL diameter = static_cast<REAL>(radius * 2);

        path.AddArc(left, top, diameter, diameter, 180.0f, 90.0f);
        path.AddArc(left + width - diameter, top, diameter, diameter, 270.0f, 90.0f);
        path.AddArc(left + width - diameter, top + height - diameter, diameter, diameter, 0.0f, 90.0f);
        path.AddArc(left, top + height - diameter, diameter, diameter, 90.0f, 90.0f);
        path.CloseFigure();

        SolidBrush fillBrush(fill);
        Pen borderPen(border, 1.0f);
        graphics.FillPath(&fillBrush, &path);
        graphics.DrawPath(&borderPen, &path);
    }

    void DrawTextLine(HDC hdc, HFONT font, COLORREF color, const RECT& rc, UINT format, const std::wstring& text)
    {
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, color);
        RECT drawRc = rc;
        DrawTextW(hdc, text.c_str(), -1, &drawRc, format);
        SelectObject(hdc, oldFont);
    }

    void DrawCloseButton(HDC hdc, Graphics& graphics);

    void DrawToolbarButton(HDC hdc, Graphics& graphics, const ToolbarItem& item)
    {
        const bool enabled = IsToolbarEnabled(item.id);
        const bool hot = g_app.hotItem == item.id;
        const bool pressed = g_app.pressedItem == item.id;

        Color fill = RgbColor(250, 250, 252);
        Color border = RgbColor(229, 232, 238);
        Color text = enabled ? RgbColor(31, 35, 41) : RgbColor(170, 175, 184);

        if (pressed)
        {
            fill = RgbColor(232, 237, 244);
            border = RgbColor(210, 216, 226);
        }
        else if (hot && enabled)
        {
            fill = RgbColor(243, 246, 250);
            border = RgbColor(218, 224, 233);
        }

        DrawRoundedPanel(graphics, item.rect, 7, fill, border);

        RectF iconRect(
            static_cast<REAL>(item.rect.left + 11),
            static_cast<REAL>(item.rect.top + 9),
            16.0f,
            16.0f);
        DrawIcon(graphics, iconRect, item.icon, text);

        RECT textRc = item.rect;
        textRc.left += 32;
        DrawTextLine(hdc, g_app.fontUi, RGB(text.GetR(), text.GetG(), text.GetB()), textRc,
            DT_SINGLELINE | DT_VCENTER | DT_LEFT, item.label);
    }

    void DrawToolbar(HDC hdc, Graphics& graphics)
    {
        if (g_app.isFullscreen)
        {
            return;
        }

        FillSolidRect(hdc, g_app.toolbarRect, RGB(248, 249, 251));

        Pen borderPen(RgbColor(225, 228, 234), 1.0f);
        graphics.DrawLine(&borderPen, 0.0f, static_cast<REAL>(g_app.toolbarRect.bottom - 1),
            static_cast<REAL>(g_app.toolbarRect.right), static_cast<REAL>(g_app.toolbarRect.bottom - 1));

        for (const ToolbarItem& item : g_app.toolbarItems)
        {
            DrawToolbarButton(hdc, graphics, item);
        }

        DrawRoundedPanel(graphics, g_app.zoomValueRect, 7, RgbColor(255, 255, 255), RgbColor(229, 232, 238));
        RECT zoomTextRc = g_app.zoomValueRect;
        DrawTextLine(hdc, g_app.fontUiBold, RGB(42, 46, 53), zoomTextRc,
            DT_SINGLELINE | DT_VCENTER | DT_CENTER,
            std::to_wstring(GetZoomPercent()) + L"%");

        std::array<int, 3> dividerX = {
            g_app.toolbarItems[2].rect.right + 9,
            g_app.toolbarItems[4].rect.right + 9,
            g_app.toolbarItems[6].rect.right + 9
        };
        for (int x : dividerX)
        {
            graphics.DrawLine(&borderPen, static_cast<REAL>(x), 13.0f, static_cast<REAL>(x), 43.0f);
        }

        DrawCloseButton(hdc, graphics);
    }

    void DrawCloseButton(HDC hdc, Graphics& graphics)
    {
        if (g_app.isFullscreen || g_app.closeButtonRect.right <= g_app.closeButtonRect.left)
        {
            return;
        }

        const bool hot = g_app.hotItem == ID_BTN_CLOSE;
        const bool pressed = g_app.pressedItem == ID_BTN_CLOSE;
        Color fill = RgbColor(252, 252, 253);
        Color border = RgbColor(227, 231, 237);
        Color text = RgbColor(64, 70, 80);

        if (pressed)
        {
            fill = RgbColor(242, 220, 220);
            border = RgbColor(222, 176, 176);
            text = RgbColor(151, 31, 31);
        }
        else if (hot)
        {
            fill = RgbColor(250, 235, 235);
            border = RgbColor(230, 198, 198);
            text = RgbColor(151, 31, 31);
        }

        DrawRoundedPanel(graphics, g_app.closeButtonRect, 7, fill, border);
        RectF iconRect(
            static_cast<REAL>(g_app.closeButtonRect.left + 12),
            static_cast<REAL>(g_app.closeButtonRect.top + 6),
            14.0f,
            14.0f);
        DrawIcon(graphics, iconRect, IconClose, text);

        RECT textRc = g_app.closeButtonRect;
        textRc.left += 34;
        DrawTextLine(hdc, g_app.fontUi, RGB(text.GetR(), text.GetG(), text.GetB()), textRc,
            DT_SINGLELINE | DT_VCENTER | DT_LEFT, L"Close");
    }

    void DrawEmptyState(HDC hdc, Graphics& graphics)
    {
        const RECT& viewport = g_app.viewportRect;
        const int centerX = (viewport.left + viewport.right) / 2;
        const int centerY = (viewport.top + viewport.bottom) / 2;

        Rect iconBox(centerX - 36, centerY - 88, 72, 72);
        DrawIcon(graphics, RectF(static_cast<REAL>(iconBox.X), static_cast<REAL>(iconBox.Y),
            static_cast<REAL>(iconBox.Width), static_cast<REAL>(iconBox.Height)),
            IconImagePlaceholder, GetEmptyIconColor());

        RECT titleRc = MakeRect(viewport.left + 32, centerY - 5, viewport.right - 32, centerY + 28);
        RECT bodyRc = MakeRect(viewport.left + 32, centerY + 28, viewport.right - 32, centerY + 54);
        RECT hintRc = MakeRect(viewport.left + 32, centerY + 58, viewport.right - 32, centerY + 80);

        DrawTextLine(hdc, g_app.fontEmptyTitle, GetEmptyTitleTextColor(), titleRc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, L"Drop an image here");
        DrawTextLine(hdc, g_app.fontEmptyBody, GetEmptyBodyTextColor(), bodyRc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, L"or use File > Open");
        DrawTextLine(hdc, g_app.fontEmptyHint, GetEmptyHintTextColor(), hintRc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, L"Supports JPG, PNG, BMP, GIF, TIFF, and ICO");
    }

    std::array<std::wstring, 4> BuildStatusParts()
    {
        if (!g_app.image)
        {
            return {
                L"No image loaded",
                L"-",
                std::to_wstring(GetZoomPercent()) + L"%",
                L"0 / 0"
            };
        }

        return {
            GetFileNameOnly(g_app.currentPath),
            std::to_wstring(g_app.image->GetWidth()) + L" x " + std::to_wstring(g_app.image->GetHeight()),
            std::to_wstring(GetZoomPercent()) + L"%",
            std::to_wstring(g_app.currentIndex + 1) + L" / " + std::to_wstring(g_app.folderImages.size())
        };
    }

    void DrawStatusBar(HDC hdc, Graphics& graphics)
    {
        if (g_app.isFullscreen)
        {
            return;
        }

        FillSolidRect(hdc, g_app.statusRect, RGB(247, 248, 250));
        Pen borderPen(RgbColor(225, 228, 234), 1.0f);
        graphics.DrawLine(&borderPen, 0.0f, static_cast<REAL>(g_app.statusRect.top),
            static_cast<REAL>(g_app.statusRect.right), static_cast<REAL>(g_app.statusRect.top));

        const auto parts = BuildStatusParts();
        const std::array<int, 4> widths = { 230, 150, 80, 72 };
        int x = 12;

        for (size_t i = 0; i < parts.size(); ++i)
        {
            RECT partRc = MakeRect(x, g_app.statusRect.top, x + widths[i], g_app.statusRect.bottom);
            DrawTextLine(hdc, g_app.fontStatus, RGB(82, 88, 98), partRc,
                DT_SINGLELINE | DT_VCENTER | DT_LEFT, parts[i]);
            x += widths[i];
            if (i + 1 != parts.size())
            {
                graphics.DrawLine(&borderPen, static_cast<REAL>(x), static_cast<REAL>(g_app.statusRect.top + 7),
                    static_cast<REAL>(x), static_cast<REAL>(g_app.statusRect.bottom - 7));
                x += 12;
            }
        }

    }

    void DrawOverlayBadge(HDC hdc, Graphics& graphics)
    {
        if (!g_app.isFullscreen)
        {
            return;
        }

        std::wstring text = g_app.isSlideshow ? L"Slide Show  |  Esc to exit" : L"Full Screen  |  Esc to exit";
        RECT badgeRect = MakeRect(g_app.viewportRect.right - 236, g_app.viewportRect.top + 16,
            g_app.viewportRect.right - 16, g_app.viewportRect.top + 48);
        DrawRoundedPanel(graphics, badgeRect, 8, RgbColor(28, 31, 37, 220), RgbColor(63, 68, 79, 220));
        DrawTextLine(hdc, g_app.fontUi, RGB(236, 239, 244), badgeRect,
            DT_SINGLELINE | DT_VCENTER | DT_CENTER, text);
    }

    void ShowContextMenu(HWND hwnd, int x, int y)
    {
        HMENU menu = CreatePopupMenu();
        if (!menu)
        {
            return;
        }

        AppendMenuW(menu, MF_STRING | (g_app.image ? MF_ENABLED : MF_GRAYED), ID_CTX_COPY_IMAGE, L"Copy Image");
        AppendMenuW(menu, MF_STRING | (!g_app.currentPath.empty() ? MF_ENABLED : MF_GRAYED), ID_CTX_COPY_PATH, L"Copy File Path");
        AppendMenuW(menu, MF_STRING | (!g_app.currentPath.empty() ? MF_ENABLED : MF_GRAYED), ID_CTX_OPEN_LOCATION, L"Open File Location");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (!g_app.currentPath.empty() ? MF_ENABLED : MF_GRAYED), ID_CTX_SET_WALLPAPER, L"Set as Desktop Background");
        AppendMenuW(menu, MF_STRING | (!g_app.currentPath.empty() ? MF_ENABLED : MF_GRAYED), ID_CTX_SET_LOCKSCREEN, L"Set as Lock Screen Background");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (g_app.image ? MF_ENABLED : MF_GRAYED), ID_CTX_ZOOM_IN, L"Zoom In");
        AppendMenuW(menu, MF_STRING | (g_app.image ? MF_ENABLED : MF_GRAYED), ID_CTX_ZOOM_OUT, L"Zoom Out");
        AppendMenuW(menu, MF_STRING | (g_app.image ? MF_ENABLED : MF_GRAYED), ID_CTX_FIT, L"Fit to Window");
        AppendMenuW(menu, MF_STRING | (g_app.image ? MF_ENABLED : MF_GRAYED), ID_CTX_ACTUAL, L"Actual Size 100%");
        AppendMenuW(menu, MF_STRING, ID_CTX_FULLSCREEN, g_app.isFullscreen ? L"Exit Full Screen" : L"Enter Full Screen");
        AppendMenuW(menu, MF_STRING | ((g_app.image && g_app.folderImages.size() > 1) ? MF_ENABLED : MF_GRAYED),
            ID_CTX_SLIDESHOW, g_app.isSlideshow ? L"Stop Slide Show" : L"Start Slide Show");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (g_app.currentIndex > 0 ? MF_ENABLED : MF_GRAYED), ID_CTX_PREV, L"Previous Image");
        AppendMenuW(menu, MF_STRING |
            ((g_app.currentIndex >= 0 && g_app.currentIndex < static_cast<int>(g_app.folderImages.size()) - 1) ? MF_ENABLED : MF_GRAYED),
            ID_CTX_NEXT, L"Next Image");

        TrackPopupMenu(menu, TPM_RIGHTBUTTON, x, y, 0, hwnd, nullptr);
        DestroyMenu(menu);
    }

    void PaintViewer(HWND hwnd)
    {
        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client = {};
        GetClientRect(hwnd, &client);
        const int clientWidth = std::max(1L, client.right - client.left);
        const int clientHeight = std::max(1L, client.bottom - client.top);

        HDC memDc = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = memDc ? CreateCompatibleBitmap(hdc, clientWidth, clientHeight) : nullptr;
        HBITMAP oldBitmap = (memDc && memBitmap) ? static_cast<HBITMAP>(SelectObject(memDc, memBitmap)) : nullptr;
        HDC paintDc = (memDc && memBitmap) ? memDc : hdc;

        Graphics graphics(paintDc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
        FillSolidRect(paintDc, client, RGB(248, 249, 251));

        DrawToolbar(paintDc, graphics);
        DrawStatusBar(paintDc, graphics);

        const RECT& viewport = g_app.viewportRect;
        FillSolidRect(paintDc, viewport, GetCanvasBackgroundColor());

        Pen framePen(GetCanvasFrameColor(), 1.0f);
        graphics.DrawRectangle(&framePen,
            static_cast<REAL>(viewport.left),
            static_cast<REAL>(viewport.top),
            static_cast<REAL>(viewport.right - viewport.left - 1),
            static_cast<REAL>(viewport.bottom - viewport.top - 1));

        if (!g_app.image)
        {
            DrawEmptyState(paintDc, graphics);
            if (memDc && memBitmap)
            {
                BitBlt(hdc, 0, 0, clientWidth, clientHeight, memDc, 0, 0, SRCCOPY);
            }
            if (memDc)
            {
                if (oldBitmap)
                {
                    SelectObject(memDc, oldBitmap);
                }
                if (memBitmap)
                {
                    DeleteObject(memBitmap);
                }
                DeleteDC(memDc);
            }
            EndPaint(hwnd, &ps);
            return;
        }

        RECT drawRect = GetImageDrawRect(hwnd);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        graphics.SetCompositingQuality(CompositingQualityHighSpeed);
        graphics.DrawImage(g_app.image,
            static_cast<INT>(drawRect.left),
            static_cast<INT>(drawRect.top),
            static_cast<INT>(drawRect.right - drawRect.left),
            static_cast<INT>(drawRect.bottom - drawRect.top));

        DrawOverlayBadge(paintDc, graphics);

        if (memDc && memBitmap)
        {
            BitBlt(hdc, 0, 0, clientWidth, clientHeight, memDc, 0, 0, SRCCOPY);
        }
        if (memDc)
        {
            if (oldBitmap)
            {
                SelectObject(memDc, oldBitmap);
            }
            if (memBitmap)
            {
                DeleteObject(memBitmap);
            }
            DeleteDC(memDc);
        }

        EndPaint(hwnd, &ps);
    }

    int HitTestToolbar(int x, int y)
    {
        if (g_app.isFullscreen)
        {
            return 0;
        }

        for (const ToolbarItem& item : g_app.toolbarItems)
        {
            if (PointInRect(item.rect, x, y))
            {
                return item.id;
            }
        }
        return 0;
    }

    int HitTestCustomButton(int x, int y)
    {
        const int toolbarId = HitTestToolbar(x, y);
        if (toolbarId != 0)
        {
            return toolbarId;
        }

        if (!g_app.isFullscreen && PointInRect(g_app.closeButtonRect, x, y))
        {
            return ID_BTN_CLOSE;
        }

        return 0;
    }

    void UpdateHotItem(int x, int y)
    {
        const int newHotItem = HitTestCustomButton(x, y);
        if (newHotItem != g_app.hotItem)
        {
            g_app.hotItem = newHotItem;
            InvalidateViewer();
        }

        if (!g_app.trackingMouse)
        {
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = g_app.hwnd;
            if (TrackMouseEvent(&tme))
            {
                g_app.trackingMouse = true;
            }
        }
    }

    void ClearHotItem()
    {
        g_app.trackingMouse = false;
        if (g_app.hotItem != 0)
        {
            g_app.hotItem = 0;
            InvalidateViewer();
        }
    }

    void HandleCommand(int commandId)
    {
        switch (commandId)
        {
        case ID_FILE_OPEN:
        case ID_BTN_OPEN:
            OpenImageDialog();
            break;
        case ID_FILE_EXIT:
            PostMessageW(g_app.hwnd, WM_CLOSE, 0, 0);
            break;
        case ID_BTN_PREV:
        case ID_CTX_PREV:
        case ID_VIEW_PREV:
            NavigateRelative(-1);
            break;
        case ID_BTN_NEXT:
        case ID_CTX_NEXT:
        case ID_VIEW_NEXT:
            NavigateRelative(1);
            break;
        case ID_BTN_ZOOM_IN:
        case ID_CTX_ZOOM_IN:
        case ID_VIEW_ZOOM_IN:
        {
            const RECT viewport = g_app.viewportRect;
            ZoomAt(kZoomStep, (viewport.left + viewport.right) / 2, (viewport.top + viewport.bottom) / 2);
            break;
        }
        case ID_BTN_ZOOM_OUT:
        case ID_CTX_ZOOM_OUT:
        case ID_VIEW_ZOOM_OUT:
        {
            const RECT viewport = g_app.viewportRect;
            ZoomAt(1.0 / kZoomStep, (viewport.left + viewport.right) / 2, (viewport.top + viewport.bottom) / 2);
            break;
        }
        case ID_BTN_FIT:
        case ID_CTX_FIT:
        case ID_VIEW_FIT:
            FitToWindow();
            break;
        case ID_BTN_ACTUAL:
        case ID_CTX_ACTUAL:
        case ID_VIEW_ACTUAL:
            ActualSize();
            break;
        case ID_BTN_FULLSCREEN:
        case ID_CTX_FULLSCREEN:
        case ID_VIEW_FULLSCREEN:
            ToggleFullscreen(!g_app.isFullscreen);
            break;
        case ID_BTN_SLIDESHOW:
        case ID_CTX_SLIDESHOW:
        case ID_VIEW_SLIDESHOW:
            ToggleSlideshow();
            break;
        case ID_BTN_CLOSE:
            PostMessageW(g_app.hwnd, WM_CLOSE, 0, 0);
            break;
        case ID_CTX_COPY_IMAGE:
            HandleCopyImage();
            break;
        case ID_CTX_COPY_PATH:
            HandleCopyPath();
            break;
        case ID_CTX_OPEN_LOCATION:
            OpenFileLocation();
            break;
        case ID_CTX_SET_WALLPAPER:
            if (!SetDesktopBackground())
            {
                MessageBoxW(g_app.hwnd, L"Set desktop background failed.", kWindowTitle, MB_ICONERROR | MB_OK);
            }
            break;
        case ID_CTX_SET_LOCKSCREEN:
            if (!SetLockScreenBackgroundBestEffort())
            {
                MessageBoxW(g_app.hwnd,
                    L"Set lock screen background failed on this Windows setup.",
                    kWindowTitle,
                    MB_ICONERROR | MB_OK);
            }
            break;
        default:
            break;
        }
    }

    void HandleDropFiles(HDROP drop)
    {
        wchar_t fileName[MAX_PATH] = {};
        if (DragQueryFileW(drop, 0, fileName, MAX_PATH) > 0)
        {
            LoadImageFile(fileName, true);
        }
        DragFinish(drop);
    }

    void HandleMouseWheel(short delta, int x, int y)
    {
        if (!g_app.image)
        {
            return;
        }
        ZoomAt(delta > 0 ? kZoomStep : (1.0 / kZoomStep), x, y);
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
            g_app.hwnd = hwnd;
            DragAcceptFiles(hwnd, TRUE);
            RefreshThemePreference();
            CreateFonts();
            LayoutUi(hwnd);
            UpdateWindowTitle();
            if (g_app.appIconLarge)
            {
                SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_app.appIconLarge));
            }
            if (g_app.appIconSmall)
            {
                SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_app.appIconSmall));
                SendMessageW(hwnd, WM_SETICON, ICON_SMALL2, reinterpret_cast<LPARAM>(g_app.appIconSmall));
            }
            return 0;

        case WM_SIZE:
            LayoutUi(hwnd);
            InvalidateViewer();
            return 0;

        case WM_COMMAND:
            HandleCommand(LOWORD(wParam));
            return 0;

        case WM_DROPFILES:
            HandleDropFiles(reinterpret_cast<HDROP>(wParam));
            return 0;

        case WM_TIMER:
            if (wParam == g_app.slideshowTimerId && g_app.isSlideshow)
            {
                if (!g_app.folderImages.empty())
                {
                    int nextIndex = g_app.currentIndex + 1;
                    if (nextIndex >= static_cast<int>(g_app.folderImages.size()))
                    {
                        nextIndex = 0;
                    }
                    LoadImageFile(g_app.folderImages[nextIndex], true);
                }
                return 0;
            }
            break;

        case WM_MOUSEWHEEL:
            HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_MOUSEMOVE:
            if (g_app.dragging)
            {
                g_app.panX = g_app.panStartX + (GET_X_LPARAM(lParam) - g_app.dragStart.x);
                g_app.panY = g_app.panStartY + (GET_Y_LPARAM(lParam) - g_app.dragStart.y);
                ClampPan(g_app.viewportRect, GetDisplayZoom(g_app.viewportRect));
                InvalidateViewer();
            }
            else
            {
                UpdateHotItem(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            return 0;

        case WM_MOUSELEAVE:
            ClearHotItem();
            return 0;

        case WM_LBUTTONDOWN:
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const int itemId = HitTestCustomButton(x, y);
            if (itemId != 0 && IsToolbarEnabled(itemId))
            {
                g_app.pressedItem = itemId;
                SetCapture(hwnd);
                InvalidateViewer();
                return 0;
            }

            if (g_app.image)
            {
                const double displayZoom = GetDisplayZoom(g_app.viewportRect);
                const double fitZoom = GetFitZoom(g_app.viewportRect);
                if (!g_app.fitToWindow && displayZoom > fitZoom && PointInRect(g_app.viewportRect, x, y))
                {
                    g_app.dragging = true;
                    g_app.dragStart.x = x;
                    g_app.dragStart.y = y;
                    g_app.panStartX = g_app.panX;
                    g_app.panStartY = g_app.panY;
                    SetCapture(hwnd);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP:
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);

            if (g_app.pressedItem != 0)
            {
                const int activated = (HitTestCustomButton(x, y) == g_app.pressedItem) ? g_app.pressedItem : 0;
                g_app.pressedItem = 0;
                ReleaseCapture();
                InvalidateViewer();
                if (activated != 0)
                {
                    HandleCommand(activated);
                }
                return 0;
            }

            if (g_app.dragging)
            {
                g_app.dragging = false;
                ReleaseCapture();
                return 0;
            }
            return 0;
        }

        case WM_RBUTTONUP:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &pt);
            ShowContextMenu(hwnd, pt.x, pt.y);
            return 0;
        }

        case WM_CONTEXTMENU:
            if (reinterpret_cast<HWND>(wParam) == hwnd)
            {
                ShowContextMenu(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == 'C' || wParam == 'c'))
            {
                HandleCopyImage();
                return 0;
            }
            switch (wParam)
            {
            case VK_LEFT:
                NavigateRelative(-1);
                return 0;
            case VK_RIGHT:
                NavigateRelative(1);
                return 0;
            case VK_ESCAPE:
                if (g_app.isSlideshow)
                {
                    StopSlideshow();
                }
                if (g_app.isFullscreen)
                {
                    ToggleFullscreen(false);
                }
                else
                {
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                }
                return 0;
            case 'F':
            case 'f':
                FitToWindow();
                return 0;
            case VK_F11:
                if (g_app.isSlideshow)
                {
                    StopSlideshow();
                }
                else
                {
                    ToggleFullscreen(!g_app.isFullscreen);
                }
                return 0;
            case '1':
                ActualSize();
                return 0;
            case VK_ADD:
            case VK_OEM_PLUS:
            {
                const RECT viewport = g_app.viewportRect;
                ZoomAt(kZoomStep, (viewport.left + viewport.right) / 2, (viewport.top + viewport.bottom) / 2);
                return 0;
            }
            case VK_SUBTRACT:
            case VK_OEM_MINUS:
            {
                const RECT viewport = g_app.viewportRect;
                ZoomAt(1.0 / kZoomStep, (viewport.left + viewport.right) / 2, (viewport.top + viewport.bottom) / 2);
                return 0;
            }
            default:
                break;
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            RefreshThemePreference();
            InvalidateViewer();
            return 0;

        case WM_NCHITTEST:
        {
            const LRESULT hit = DefWindowProcW(hwnd, message, wParam, lParam);
            if (hit == HTLEFT || hit == HTRIGHT || hit == HTTOP || hit == HTTOPLEFT ||
                hit == HTTOPRIGHT || hit == HTBOTTOM || hit == HTBOTTOMLEFT || hit == HTBOTTOMRIGHT)
            {
                return hit;
            }

            if (!g_app.isFullscreen)
            {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hwnd, &pt);
                const bool overButton = HitTestCustomButton(pt.x, pt.y) != 0;
                if (!overButton && pt.y >= 0 && pt.y < kToolbarHeight)
                {
                    return HTCAPTION;
                }
            }
            return HTCLIENT;
        }

        case WM_PAINT:
            PaintViewer(hwnd);
            return 0;

        case WM_DESTROY:
            StopSlideshow();
            DestroyFonts();
            ReleaseImage();
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    EnableDpiAwareness();

    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&g_app.gdiplusToken, &gdiplusStartupInput, nullptr) != Ok)
    {
        MessageBoxW(nullptr, L"GDI+ startup failed.", kWindowTitle, MB_ICONERROR | MB_OK);
        return 1;
    }

    LoadAppIcons(hInstance);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = g_app.appIconLarge ? g_app.appIconLarge : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = g_app.appIconSmall ? g_app.appIconSmall : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&wc))
    {
        GdiplusShutdown(g_app.gdiplusToken);
        MessageBoxW(nullptr, L"Window class registration failed.", kWindowTitle, MB_ICONERROR | MB_OK);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1180, 780,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd)
    {
        GdiplusShutdown(g_app.gdiplusToken);
        MessageBoxW(nullptr, L"Main window creation failed.", kWindowTitle, MB_ICONERROR | MB_OK);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow == 0 ? SW_SHOWNORMAL : nCmdShow);
    UpdateWindow(hwnd);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv)
    {
        if (argc > 1 && argv[1] && argv[1][0] != L'\0')
        {
            LoadImageFile(argv[1], true);
        }
        LocalFree(argv);
    }

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(g_app.gdiplusToken);
    return static_cast<int>(msg.wParam);
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dshow.h>
#include <initguid.h>
#include <olectl.h>
#include <stdint.h>
#include <shlobj.h>

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#include "resource.h"

// --- STB Image Write Implementation ---
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Add after existing includes
#include <vector>
#include <algorithm>

// Structure to hold resolution info
struct Resolution {
    int width;
    int height;
    std::wstring displayName;
};

// Add new global variables
static HWND g_hResolutionCombo = nullptr;
static std::vector<Resolution> g_availableResolutions;
static int g_desiredWidth = 0;
static int g_desiredHeight = 0;

// Function to enumerate supported resolutions from a camera
static std::vector<Resolution> GetSupportedResolutions(IBaseFilter* cameraFilter) {
    std::vector<Resolution> resolutions;
    
    // Get the capture output pin
    IEnumPins* enumPins = nullptr;
    IPin* pin = nullptr;
    IAMStreamConfig* streamConfig = nullptr;
    
    if (FAILED(cameraFilter->EnumPins(&enumPins)))
        return resolutions;
    
    while (enumPins->Next(1, &pin, nullptr) == S_OK) {
        PIN_DIRECTION dir;
        if (SUCCEEDED(pin->QueryDirection(&dir)) && dir == PINDIR_OUTPUT) {
            // Try to get IAMStreamConfig from this pin
            if (SUCCEEDED(pin->QueryInterface(IID_IAMStreamConfig, (void**)&streamConfig))) {
                int count = 0, size = 0;
                
                // Get number of media types and their size
                if (SUCCEEDED(streamConfig->GetNumberOfCapabilities(&count, &size)) && 
                    size == sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
                    
                    for (int i = 0; i < count; i++) {
                        AM_MEDIA_TYPE* pmt = nullptr;
                        VIDEO_STREAM_CONFIG_CAPS caps;
                        
                        if (SUCCEEDED(streamConfig->GetStreamCaps(i, &pmt, (BYTE*)&caps))) {
                            if (pmt->formattype == FORMAT_VideoInfo) {
                                VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
                                
                                int w = vih->bmiHeader.biWidth;
                                int h = abs(vih->bmiHeader.biHeight);
                                
                                // Check if this resolution is already in the list
                                bool found = false;
                                for (const auto& res : resolutions) {
                                    if (res.width == w && res.height == h) {
                                        found = true;
                                        break;
                                    }
                                }
                                
                                if (!found) {
                                    Resolution res;
                                    res.width = w;
                                    res.height = h;
                                    res.displayName = std::to_wstring(w) + L" x " + std::to_wstring(h);
                                    resolutions.push_back(res);
                                }
                            }
                            
                            // Free the media type
                            if (pmt->cbFormat != 0) {
                                CoTaskMemFree(pmt->pbFormat);
                            }
                            if (pmt->pUnk) {
                                pmt->pUnk->Release();
                            }
                            CoTaskMemFree(pmt);
                        }
                    }
                }
                
                streamConfig->Release();
            }
        }
        pin->Release();
    }
    
    enumPins->Release();
    
    // Sort resolutions by total pixels (area)
    std::sort(resolutions.begin(), resolutions.end(), [](const Resolution& a, const Resolution& b) {
        return (a.width * a.height) < (b.width * b.height);
    });
    
    return resolutions;
}

// Function to populate resolution combo box
static void PopulateResolutionList(IBaseFilter* cameraFilter) {
    SendMessage(g_hResolutionCombo, CB_RESETCONTENT, 0, 0);
    g_availableResolutions.clear();
    
    if (cameraFilter) {
        g_availableResolutions = GetSupportedResolutions(cameraFilter);
        
        for (size_t i = 0; i < g_availableResolutions.size(); i++) {
            SendMessage(g_hResolutionCombo, CB_ADDSTRING, 0, 
                       (LPARAM)g_availableResolutions[i].displayName.c_str());
        }
        
        // Select the highest resolution by default
        if (!g_availableResolutions.empty()) {
            SendMessage(g_hResolutionCombo, CB_SETCURSEL, 
                       (WPARAM)(g_availableResolutions.size() - 1), 0);
        }
    }
}

// Function to set the camera resolution
static bool SetCameraResolution(IBaseFilter* cameraFilter, int width, int height) {
    IEnumPins* enumPins = nullptr;
    IPin* pin = nullptr;
    IAMStreamConfig* streamConfig = nullptr;
    bool result = false;
    
    if (FAILED(cameraFilter->EnumPins(&enumPins)))
        return false;
    
    while (enumPins->Next(1, &pin, nullptr) == S_OK) {
        PIN_DIRECTION dir;
        if (SUCCEEDED(pin->QueryDirection(&dir)) && dir == PINDIR_OUTPUT) {
            if (SUCCEEDED(pin->QueryInterface(IID_IAMStreamConfig, (void**)&streamConfig))) {
                int count = 0, size = 0;
                
                if (SUCCEEDED(streamConfig->GetNumberOfCapabilities(&count, &size)) && 
                    size == sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
                    
                    for (int i = 0; i < count; i++) {
                        AM_MEDIA_TYPE* pmt = nullptr;
                        VIDEO_STREAM_CONFIG_CAPS caps;
                        
                        if (SUCCEEDED(streamConfig->GetStreamCaps(i, &pmt, (BYTE*)&caps))) {
                            if (pmt->formattype == FORMAT_VideoInfo) {
                                VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
                                
                                int w = vih->bmiHeader.biWidth;
                                int h = abs(vih->bmiHeader.biHeight);
                                
                                // Check if this matches our desired resolution
                                if (w == width && h == height) {
                                    // Set the media type
                                    if (SUCCEEDED(streamConfig->SetFormat(pmt))) {
                                        result = true;
                                    }
                                }
                            }
                            
                            // Free the media type
                            if (pmt->cbFormat != 0) {
                                CoTaskMemFree(pmt->pbFormat);
                            }
                            if (pmt->pUnk) {
                                pmt->pUnk->Release();
                            }
                            CoTaskMemFree(pmt);
                            
                            if (result) break;
                        }
                    }
                }
                
                streamConfig->Release();
                if (result) break;
            }
        }
        pin->Release();
    }
    
    enumPins->Release();
    return result;
}

// --- DirectShow SampleGrabber Interfaces ---
EXTERN_C const CLSID CLSID_SampleGrabber;
EXTERN_C const IID IID_ISampleGrabber;
EXTERN_C const IID IID_ISampleGrabberCB;
DEFINE_GUID(CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID(IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce, 0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f);
DEFINE_GUID(IID_ISampleGrabberCB, 0x0579154a, 0x2b53, 0x4994, 0xb0, 0xd0, 0xe7, 0x73, 0x14, 0x8e, 0xff, 0x85);

struct ISampleGrabberCB : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SampleCB(double, IMediaSample*) = 0;
    virtual HRESULT STDMETHODCALLTYPE BufferCB(double, BYTE*, long) = 0;
};
struct ISampleGrabber : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long*, long*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample**) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB*, long) = 0;
};

// --- Global UI & DirectShow handles ---
static HWND g_hDialog = nullptr;
static HWND g_hCameraCombo = nullptr;
static HWND g_hStatusBar = nullptr;
static HWND g_hBrightnessSlider = nullptr;
static HWND g_hContrastSlider = nullptr;

static std::vector<IMoniker*> g_cameraMonikers;
static IGraphBuilder* graph = nullptr;
static ICaptureGraphBuilder2* builder = nullptr;
static IBaseFilter* camera = nullptr;
static IBaseFilter* grabberFilter = nullptr;
static ISampleGrabber* grabber = nullptr;
static IMediaControl* mediaControl = nullptr;
static IVideoWindow* videoWindow = nullptr;

// --- Frame Processing State ---
static std::mutex g_frameMutex;
static std::vector<uint8_t> g_rawFrameRGB;
static int g_width = 0, g_height = 0;

static std::atomic<int> g_brightness{0}; // Range: -100 to 100
static std::atomic<int> g_contrast{0};   // Range: -100 to 100
static std::atomic<int> g_jpegQuality{90}; // JPEG quality 1-100

// Captures folder path
static std::wstring g_capturesPath;

template<typename T> static void SafeRelease(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

// Custom SampleGrabber Callback
class SampleGrabberCB : public ISampleGrabberCB {
    long m_ref = 1;
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ISampleGrabberCB) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override {
        long r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP SampleCB(double, IMediaSample*) override { return E_NOTIMPL; }
    STDMETHODIMP BufferCB(double, BYTE* buffer, long len) override {
        if (!buffer || g_width <= 0 || g_height <= 0) return S_OK;

        std::lock_guard<std::mutex> lock(g_frameMutex);
        size_t expectedSize = (size_t)g_width * g_height * 3;
        
        if (g_rawFrameRGB.size() != expectedSize) {
            g_rawFrameRGB.resize(expectedSize);
        }

        // Direct copy of incoming RGB24 frame data
        memcpy(g_rawFrameRGB.data(), buffer, expectedSize);
        return S_OK;
    }
};
static SampleGrabberCB* g_callback = nullptr;

// --- Initialize Captures folder ---
static void InitializeCapturesFolder() {
    // Get the executable directory
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    
    // Find the last backslash to get directory
    std::wstring exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    
    // Create Captures folder path
    g_capturesPath = exeDir + L"\\Captures";
    
    // Create directory if it doesn't exist
    CreateDirectoryW(g_capturesPath.c_str(), nullptr);
}

// Generate timestamp filename
static std::wstring GenerateTimestampFilename() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm_now;
    localtime_s(&tm_now, &time_t_now);
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::wstringstream wss;
    wss << L"Capture_" 
        << std::put_time(&tm_now, L"%Y%m%d_%H%M%S")
        << L"_" << ms.count() << L"ms.jpg";
    
    return wss.str();
}

// --- DirectShow Enumeration & Initialization ---
static void CleanupCameraList() {
    for (auto* m : g_cameraMonikers) SafeRelease(m);
    g_cameraMonikers.clear();
}

static void PopulateCameraList() {
    SendMessage(g_hCameraCombo, CB_RESETCONTENT, 0, 0);
    ICreateDevEnum* devEnum = nullptr; 
    IEnumMoniker* enumMon = nullptr;
    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&devEnum))) return;
    if (FAILED(devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMon, 0))) { SafeRelease(devEnum); return; }
    
    SafeRelease(devEnum);
    if (enumMon) {
        IMoniker* moniker = nullptr;
        while (enumMon->Next(1, &moniker, nullptr) == S_OK) {
            std::wstring name = L"Unknown Camera";
            IPropertyBag* propBag = nullptr;
            if (SUCCEEDED(moniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&propBag))) {
                VARIANT var; VariantInit(&var);
                if (SUCCEEDED(propBag->Read(L"FriendlyName", &var, 0))) name = var.bstrVal;
                VariantClear(&var); SafeRelease(propBag);
            }
            SendMessage(g_hCameraCombo, CB_ADDSTRING, 0, (LPARAM)name.c_str());
            g_cameraMonikers.push_back(moniker);
        }
        SafeRelease(enumMon);
    }
    if (!g_cameraMonikers.empty()) SendMessage(g_hCameraCombo, CB_SETCURSEL, 0, 0);
}

static bool GetSelectedCamera(IBaseFilter** outCamera) {
    int sel = (int)SendMessage(g_hCameraCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)g_cameraMonikers.size()) return false;
    return SUCCEEDED(g_cameraMonikers[sel]->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)outCamera));
}

static void CleanupCamera() {
    if (mediaControl) mediaControl->Stop();
    if (videoWindow) {
        videoWindow->put_Visible(OAFALSE);
        videoWindow->put_Owner((OAHWND)NULL);
        SafeRelease(videoWindow);
    }
    if (g_callback) { g_callback->Release(); g_callback = nullptr; }
    SafeRelease(mediaControl); SafeRelease(grabber); SafeRelease(grabberFilter);
    SafeRelease(camera); SafeRelease(builder); SafeRelease(graph);
}

static bool InitCamera() {
    if (FAILED(CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&graph))) return false;
    if (FAILED(CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&builder))) return false;
    builder->SetFiltergraph(graph);

    if (!GetSelectedCamera(&camera)) { 
        MessageBoxW(g_hDialog, L"Failed to bind to selected camera.", L"Error", MB_OK | MB_ICONERROR); 
        return false; 
    }
    graph->AddFilter(camera, L"Camera");

    // Set desired resolution before building the graph
    if (g_desiredWidth > 0 && g_desiredHeight > 0) {
        SetCameraResolution(camera, g_desiredWidth, g_desiredHeight);
    }

    if (FAILED(CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&grabberFilter))) return false;
    graph->AddFilter(grabberFilter, L"Grabber");
    if (FAILED(grabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&grabber))) return false;
    
    AM_MEDIA_TYPE mt = {}; 
    mt.majortype = MEDIATYPE_Video; 
    mt.subtype = MEDIASUBTYPE_RGB24; 
    mt.formattype = FORMAT_VideoInfo;
    grabber->SetMediaType(&mt);

    if (FAILED(builder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, camera, grabberFilter, nullptr))) return false;

    // Attach preview video stream to Static Control UI
    if (SUCCEEDED(graph->QueryInterface(IID_IVideoWindow, (void**)&videoWindow))) {
        HWND hVideoHolder = GetDlgItem(g_hDialog, IDC_PREVIEW_VIDEO);
        videoWindow->put_Owner((OAHWND)hVideoHolder);
        videoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
        RECT rc; GetClientRect(hVideoHolder, &rc);
        videoWindow->SetWindowPosition(0, 0, rc.right, rc.bottom);
    }

    AM_MEDIA_TYPE actual = {};
    if (FAILED(grabber->GetConnectedMediaType(&actual))) return false;
    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)actual.pbFormat;
    g_width = vih->bmiHeader.biWidth;
    g_height = std::abs(vih->bmiHeader.biHeight);

    // Update status with actual resolution
    std::wstring statusMsg = L"Camera Running - Resolution: " + 
                            std::to_wstring(g_width) + L"x" + std::to_wstring(g_height);
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)statusMsg.c_str());

    g_callback = new SampleGrabberCB();
    grabber->SetOneShot(FALSE); 
    grabber->SetBufferSamples(FALSE);
    if (FAILED(grabber->SetCallback(g_callback, 1))) return false;

    return SUCCEEDED(graph->QueryInterface(IID_IMediaControl, (void**)&mediaControl));
}

// --- Image Processing & Saving (JPEG with stb_image_write) ---
// --- Image Processing & Saving (JPEG with stb_image_write) ---
static void ProcessAndSaveJPEG(const std::wstring& filepath) {
    std::lock_guard<std::mutex> lock(g_frameMutex);
    if (g_rawFrameRGB.empty()) {
        MessageBoxW(g_hDialog, L"No active frame available to save.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    // Process Frame with current Brightness & Contrast settings
    float bVal = (float)g_brightness.load();
    float cVal = (float)g_contrast.load();
    
    // Contrast factor calculation formula
    float factor = (259.0f * (cVal + 255.0f)) / (255.0f * (259.0f - cVal));

    // Create processed buffer
    std::vector<uint8_t> processedRGB(g_width * g_height * 3);
    
    // Process and convert BGR (DirectShow default) to RGB
    for (int y = 0; y < g_height; ++y) {
        for (int x = 0; x < g_width; ++x) {
            // DirectShow typically provides bottom-up BGR data
            // Convert from bottom-up to top-down, and BGR to RGB
            int srcY = g_height - 1 - y; // Flip vertically
            int srcIdx = (srcY * g_width + x) * 3;
            int dstIdx = (y * g_width + x) * 3;
            
            // Get BGR values from source
            float b = static_cast<float>(g_rawFrameRGB[srcIdx]);     // Blue
            float g = static_cast<float>(g_rawFrameRGB[srcIdx + 1]); // Green
            float r = static_cast<float>(g_rawFrameRGB[srcIdx + 2]); // Red
            
            // Apply Contrast
            r = factor * (r - 128.0f) + 128.0f;
            g = factor * (g - 128.0f) + 128.0f;
            b = factor * (b - 128.0f) + 128.0f;
            
            // Apply Brightness
            r += bVal;
            g += bVal;
            b += bVal;
            
            // Clamp and store in RGB order
            processedRGB[dstIdx] = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));     // Red
            processedRGB[dstIdx + 1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)); // Green
            processedRGB[dstIdx + 2] = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)); // Blue
        }
    }

    // Convert to narrow string for stb_image_write
    int chars_written = WideCharToMultiByte(CP_UTF8, 0, filepath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8Path(chars_written - 1, 0); // -1 to exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, filepath.c_str(), -1, &utf8Path[0], chars_written, nullptr, nullptr);

    // Write JPEG using stb_image_write
    // Note: stb_image_write expects RGB data, top-down order
    int result = stbi_write_jpg(utf8Path.c_str(), g_width, g_height, 3, processedRGB.data(), g_jpegQuality.load());
    
    if (result != 0) {
        std::wstring msg = L"Photo saved to:\n" + filepath;
        MessageBoxW(g_hDialog, msg.c_str(), L"Photo Saved", MB_OK | MB_ICONINFORMATION);
        SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Photo saved successfully!");
    } else {
        MessageBoxW(g_hDialog, L"Could not save JPEG file.", L"Error", MB_OK | MB_ICONERROR);
    }
}

// Quick capture - saves directly to Captures folder
static void QuickCapture() {
    if (g_capturesPath.empty()) {
        InitializeCapturesFolder();
    }
    
    std::wstring filename = GenerateTimestampFilename();
    std::wstring fullPath = g_capturesPath + L"\\" + filename;
    
    ProcessAndSaveJPEG(fullPath);
}

// --- Main Window Proc ---
// --- Main Window Proc ---
static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            g_hDialog = hDlg;
            g_hCameraCombo = GetDlgItem(hDlg, IDC_CAMERA_COMBO);
            g_hResolutionCombo = GetDlgItem(hDlg, IDC_RESOLUTION_COMBO);
            g_hStatusBar = GetDlgItem(hDlg, IDC_STATUSBAR);
            g_hBrightnessSlider = GetDlgItem(hDlg, IDC_BRIGHTNESS_SLIDER);
            g_hContrastSlider = GetDlgItem(hDlg, IDC_CONTRAST_SLIDER);

            // Setup Trackbars (-100 to 100)
            SendMessage(g_hBrightnessSlider, TBM_SETRANGE, TRUE, MAKELPARAM(-100, 100));
            SendMessage(g_hBrightnessSlider, TBM_SETPOS, TRUE, 0);

            SendMessage(g_hContrastSlider, TBM_SETRANGE, TRUE, MAKELPARAM(-100, 100));
            SendMessage(g_hContrastSlider, TBM_SETPOS, TRUE, 0);

            // Initialize Captures folder
            InitializeCapturesFolder();

            EnableWindow(GetDlgItem(hDlg, IDC_STOP_BUTTON), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_TAKE_PHOTO_BUTTON), FALSE);

            // Populate camera list first
            PopulateCameraList();
            
            // Populate resolutions for the default camera selection
            if (!g_cameraMonikers.empty()) {
                IBaseFilter* tempCamera = nullptr;
                if (SUCCEEDED(g_cameraMonikers[0]->BindToObject(nullptr, nullptr, 
                                                                  IID_IBaseFilter, (void**)&tempCamera))) {
                    PopulateResolutionList(tempCamera);
                    tempCamera->Release();
                }
            }
            
            SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
            return TRUE;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            
            // Handle combo box selection changes
            if (wmEvent == CBN_SELCHANGE) {
                if (wmId == IDC_CAMERA_COMBO) {
                    int sel = (int)SendMessage(g_hCameraCombo, CB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)g_cameraMonikers.size()) {
                        IBaseFilter* tempCamera = nullptr;
                        if (SUCCEEDED(g_cameraMonikers[sel]->BindToObject(nullptr, nullptr, 
                                                                          IID_IBaseFilter, (void**)&tempCamera))) {
                            PopulateResolutionList(tempCamera);
                            tempCamera->Release();
                        }
                    }
                    return TRUE;
                }
                else if (wmId == IDC_RESOLUTION_COMBO) {
                    int sel = (int)SendMessage(g_hResolutionCombo, CB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)g_availableResolutions.size()) {
                        g_desiredWidth = g_availableResolutions[sel].width;
                        g_desiredHeight = g_availableResolutions[sel].height;
                    }
                    return TRUE;
                }
            }
            
            // Handle button commands
            switch (wmId) {
                case IDC_START_BUTTON:
                    if (!InitCamera()) break;
                    if (FAILED(mediaControl->Run())) { CleanupCamera(); break; }
                    
                    EnableWindow(GetDlgItem(hDlg, IDC_START_BUTTON), FALSE);
                    EnableWindow(GetDlgItem(hDlg, IDC_STOP_BUTTON), TRUE);
                    EnableWindow(GetDlgItem(hDlg, IDC_TAKE_PHOTO_BUTTON), TRUE);
                    EnableWindow(g_hCameraCombo, FALSE);
                    EnableWindow(g_hResolutionCombo, FALSE);  // Disable resolution combo while camera is running
                    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Camera Running...");
                    break;

                case IDC_STOP_BUTTON:
                    CleanupCamera();
                    EnableWindow(GetDlgItem(hDlg, IDC_START_BUTTON), TRUE);
                    EnableWindow(GetDlgItem(hDlg, IDC_STOP_BUTTON), FALSE);
                    EnableWindow(GetDlgItem(hDlg, IDC_TAKE_PHOTO_BUTTON), FALSE);
                    EnableWindow(g_hCameraCombo, TRUE);
                    EnableWindow(g_hResolutionCombo, TRUE);  // Re-enable resolution combo
                    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Camera Stopped");
                    break;

                case IDC_RESET_ADJUST_BTN:
                    g_brightness.store(0);
                    g_contrast.store(0);
                    SendMessage(g_hBrightnessSlider, TBM_SETPOS, TRUE, 0);
                    SendMessage(g_hContrastSlider, TBM_SETPOS, TRUE, 0);
                    SetWindowTextW(GetDlgItem(hDlg, IDC_BRIGHTNESS_VAL), L"0");
                    SetWindowTextW(GetDlgItem(hDlg, IDC_CONTRAST_VAL), L"0");
                    break;

                case IDC_TAKE_PHOTO_BUTTON:
                    QuickCapture();
                    break;

                case IDCANCEL:
                    DestroyWindow(hDlg);
                    break;
            }
            return TRUE;
        }

        case WM_HSCROLL: {
            // Trackbar scroll events
            HWND hControl = (HWND)lParam;
            if (hControl == g_hBrightnessSlider) {
                int pos = (int)SendMessage(g_hBrightnessSlider, TBM_GETPOS, 0, 0);
                g_brightness.store(pos);
                SetWindowTextW(GetDlgItem(hDlg, IDC_BRIGHTNESS_VAL), std::to_wstring(pos).c_str());
            } else if (hControl == g_hContrastSlider) {
                int pos = (int)SendMessage(g_hContrastSlider, TBM_GETPOS, 0, 0);
                g_contrast.store(pos);
                SetWindowTextW(GetDlgItem(hDlg, IDC_CONTRAST_VAL), std::to_wstring(pos).c_str());
            }
            return TRUE;
        }

        case WM_DESTROY:
            CleanupCamera();
            CleanupCameraList();
            PostQuitMessage(0);
            return TRUE;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 1;
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_UPDOWN_CLASS };
    InitCommonControlsEx(&icex);
    
    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), nullptr, DlgProc, 0);
    
    CoUninitialize();
    return 0;
}
// SPDX-License-Identifier: GPL-3.0-or-later
// Observe Raw Input without replacing another component's device registration.
#include <commctrl.h>
static std::atomic<bool> mouseAllowed;
static std::atomic<int> mouseX{},mouseY{};
static std::atomic<unsigned long long> mouseReads{},mousePackets{};
static HWND mouseWindow;
static DWORD mouseRetryAt;
static bool mouseOwnRegistration;
static void ClearMouse() {mouseAllowed=false;mouseX=0;mouseY=0;}
static bool MouseForeground() {
    if(!Focused())return false;
    CURSORINFO cursor{sizeof(CURSORINFO)};
    return GetCursorInfo(&cursor) && !(cursor.flags&CURSOR_SHOWING);
}
static LRESULT CALLBACK MouseWindowProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR,DWORD_PTR) {
    if(message==WM_INPUT) {
        RAWINPUT input{};UINT size=sizeof(input);
        const UINT copied=GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam),RID_INPUT,&input,&size,sizeof(RAWINPUTHEADER));
        if(copied!=UINT(-1) && copied>=sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE) && input.header.dwType==RIM_TYPEMOUSE) {
            ++mouseReads;
            if(mouseAllowed.load() && MouseForeground() && !(input.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE)) {
                const int dx=std::clamp(static_cast<int>(input.data.mouse.lLastX),-10000,10000);
                const int dy=std::clamp(static_cast<int>(input.data.mouse.lLastY),-10000,10000);
                mouseX.fetch_add(dx);mouseY.fetch_add(dy);if(dx || dy)++mousePackets;
            }
        }
    }
    if(message==WM_KILLFOCUS || message==WM_ACTIVATEAPP && !wparam)ClearMouse();
    if(message==WM_NCDESTROY) {
        ClearMouse();RemoveWindowSubclass(window,MouseWindowProc,0x50354650);
        mouseWindow=nullptr;mouseOwnRegistration=false;
    }
    // Preserve original delivery and native WM_INPUT cleanup.
    return DefSubclassProc(window,message,wparam,lparam);
}
static BOOL CALLBACK FindMouseWindow(HWND window,LPARAM output) {
    DWORD pid{};DWORD thread=GetWindowThreadProcessId(window,&pid);
    if(pid==GetCurrentProcessId() && thread==GetCurrentThreadId() && IsWindowVisible(window) && !GetWindow(window,GW_OWNER)) {
        *reinterpret_cast<HWND*>(output)=window;return FALSE;
    }
    return TRUE;
}
static void InitializeMouse() {
    if(mouseWindow || GetTickCount()-mouseRetryAt<1000)return;
    mouseRetryAt=GetTickCount();
    HWND window{};EnumWindows(FindMouseWindow,reinterpret_cast<LPARAM>(&window));
    if(!window)return;
    UINT count{};
    if(GetRegisteredRawInputDevices(nullptr,&count,sizeof(RAWINPUTDEVICE))==UINT(-1) || count>256)return;
    std::vector<RAWINPUTDEVICE> registered(count);
    if(count && GetRegisteredRawInputDevices(registered.data(),&count,sizeof(RAWINPUTDEVICE))==UINT(-1))return;
    bool existing=false;
    for(const auto& device:registered)if(device.usUsagePage==1 && device.usUsage==2) {
        existing=true;
        if(device.hwndTarget)window=device.hwndTarget;
    }
    // SetWindowSubclass must run on the window's owning thread.
    DWORD pid{};
    if(GetWindowThreadProcessId(window,&pid)!=GetCurrentThreadId() || pid!=GetCurrentProcessId())return;
    if(!SetWindowSubclass(window,MouseWindowProc,0x50354650,0))return;
    if(!existing) {
        RAWINPUTDEVICE request{1,2,0,window};
        if(!RegisterRawInputDevices(&request,1,sizeof(request))) {RemoveWindowSubclass(window,MouseWindowProc,0x50354650);return;}
        mouseOwnRegistration=true;
    }
    mouseWindow=window;
    Log("Mouse Raw Input ready: window=%p reused_registration=%d",window,existing);
}

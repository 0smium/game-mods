// SPDX-License-Identifier: GPL-3.0-or-later
// P5RFirstPerson 1.0.0-rc1. Layouts/signatures derived from rirurin/p5r-freecam
// (17a6aeca) and OpenGFD (16773940). See THIRD_PARTY_NOTICES.md.
// Native movement magnitude/collision and scripted camera commands are retained.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>
#include <safetyhook.hpp>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>
#include "controls.hpp"
#include "settings_policy.hpp"

using Addr = uintptr_t;
namespace safetyhook {
    extern thread_local int p5_near_error, p5_near_allocator_error;
    extern thread_local unsigned long p5_virtual_alloc_error, p5_virtual_alloc2_error;
}
struct Vec3 { float x,y,z; };
struct Matrix { float m[16]; };
static HMODULE selfModule;
static Addr imageBase, globalAddress;
static size_t imageSize;
static wchar_t directory[MAX_PATH], iniPath[MAX_PATH], logPath[MAX_PATH], statusPath[MAX_PATH];
static SRWLOCK logLock = SRWLOCK_INIT;
static SafetyHookInline moveHook, inputVectorHook, cameraHook, endHook, eventHook, menuHooks[4];
static SafetyHookInline renderHook;
static int interactionMode=2,renderScopeDepth;
static uint64_t renderCalls,retainedFrames;
static Addr cameraFunction, endFunction;
static bool wanted=true, active, faulted;
static DWORD mainThread, uiUntil;
static Addr boundTask; static uint64_t boundUid;
static bool modernLook=true;
static CameraTuning::Values tuning{165,110,5};
static Addr padButtons,padRight,padLeft;
static p5fp::Look look;
static p5fp::LookSettings lookSettings;
static p5fp::Gait gait;
static Addr lookFieldTask,lookFieldWork,lookPc,lookRoot,lookScene; static uint64_t lookFieldUid;
static bool lookReady,rootReady;
static Vec3 previousRoot;
static float lastPadX,lastPadY,frameSeconds;
static uint64_t inputCorrections;
static LARGE_INTEGER clockFrequency{},previousClock{};
static bool hidePlayer=true;
static const char* reason="waiting for field update";
static uint64_t moves, cameraUpdates, applications, menuCalls[4]{}, menuTrue[4]{};
static DWORD lastStatus;
static Addr lastSequence, lastField;
static int lastSequenceId=-1;
static bool Focused() { DWORD pid=0; GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId(); }
static float FrameSeconds() {
    LARGE_INTEGER now;QueryPerformanceCounter(&now);
    float dt=previousClock.QuadPart?static_cast<float>(double(now.QuadPart-previousClock.QuadPart)/double(clockFrequency.QuadPart)):0.f;
    previousClock=now;return dt;
}

template<class T> bool Read(Addr p,T& out) {
    if(p<0x10000 || p>UINTPTR_MAX-sizeof(T)) return false;
    __try { out=*reinterpret_cast<const T*>(p); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> T Get(Addr p) { T v{}; Read(p,v); return v; }
template<class T> bool Write(Addr p,const T& v) {
    if(p<0x10000 || p>UINTPTR_MAX-sizeof(T)) return false;
    __try { *reinterpret_cast<T*>(p)=v; return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool NameEquals(Addr task,const char* expected) {
    Addr str=Get<Addr>(task+0x18); int len=Get<int>(task+0x20);
    if(!str || len!=static_cast<int>(strlen(expected)) || len<0 || len>100) return false;
    for(int i=0;i<len;i++) { char c; if(!Read(str+i,c) || c!=expected[i]) return false; }
    return true;
}
static void Log(const char* fmt,...) {
    char msg[3072]; va_list a; va_start(a,fmt); vsnprintf(msg,sizeof(msg),fmt,a); va_end(a);
    SYSTEMTIME t; GetLocalTime(&t);
    AcquireSRWLockExclusive(&logLock);
    FILE* f=nullptr; _wfopen_s(&f,logPath,L"ab");
    if(f) { fprintf(f,"[%02u:%02u:%02u.%03u] %s\n",t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,msg); fclose(f); }
    ReleaseSRWLockExclusive(&logLock);
}
struct Tasks { Addr camera{}, field{}, sequence{}; bool blocked{}; int count{}; bool battle{}; };
#include "mouse_input.inl"

static Tasks FindTasks() {
    Tasks found;
    Addr task=Get<Addr>(globalAddress+0x2c60); // update tail; GfdTask::iter_update
    for(int i=0;task && i<2048;i++) {
        int status=Get<int>(task);
        if(status<1 || status>3) break;
        found.count++;
        if(status==2) {
            if(NameEquals(task,"battle"))found.battle=true;
            if(NameEquals(task,"field camera CTRL")) found.camera=task;
            if(NameEquals(task,"fld_main")) found.field=task;
            if(NameEquals(task,"sequence_observer") || NameEquals(task,"sequence observer")) found.sequence=task;
            if(NameEquals(task,"battle") || NameEquals(task,"event_proc") || NameEquals(task,"CAMP_MAIN") ||
               NameEquals(task,"CMP_SYSTEM_SIMPLE") || NameEquals(task,"fld_scn_change") ||
               NameEquals(task,"fld_pc_wire_anim")) found.blocked=true;
        }
        Addr next=Get<Addr>(task+0x80); if(next==task) break; task=next;
    }
    return found;
}
static bool IsExecutable(Addr p) {
    if(p<imageBase || p>=imageBase+imageSize) return false;
    MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m))) return false;
    return m.State==MEM_COMMIT && !(m.Protect&PAGE_GUARD) && (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static bool Finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&fabsf(a.x)<1e7f&&fabsf(a.y)<1e7f&&fabsf(a.z)<1e7f; }
static bool ValidView(const Matrix& m) {
    for(float f:m.m) if(!std::isfinite(f)) return false;
    if(fabsf(m.m[3])>.01f || fabsf(m.m[7])>.01f || fabsf(m.m[11])>.01f || fabsf(m.m[15]-1)>.01f) return false;
    for(int r=0;r<3;r++) {
        float n=m.m[r]*m.m[r]+m.m[4+r]*m.m[4+r]+m.m[8+r]*m.m[8+r];
        if(fabsf(n-1)>.03f) return false;
    }
    return true;
}
static Matrix EyeView(const Matrix& original,Vec3 eye) {
    Matrix result=original;
    for(int r=0;r<3;r++) result.m[12+r]=-(original.m[r]*eye.x+original.m[4+r]*eye.y+original.m[8+r]*eye.z);
    return result;
}
static float VerticalFov(float horizontal,float aspect) {
    return 2.f*atanf(tanf(horizontal*3.14159265358979323846f/360.f)/aspect)*180.f/3.14159265358979323846f;
}
static Matrix Multiply(const Matrix& a,const Matrix& b) {
    Matrix out{};
    for(int c=0;c<4;c++)for(int r=0;r<4;r++)for(int k=0;k<4;k++)out.m[c*4+r]+=a.m[k*4+r]*b.m[c*4+k];
    return out;
}
static bool RigidTransform(const Matrix& m) {
    if(!ValidView(m))return false;
    for(int a=0;a<3;a++) {float norm=0;for(int k=0;k<3;k++)norm+=m.m[a*4+k]*m.m[a*4+k];if(fabsf(norm-1)>.002f)return false;}
    for(int a=0;a<3;a++)for(int b=a+1;b<3;b++) {
        float dot=0;for(int k=0;k<3;k++)dot+=m.m[a*4+k]*m.m[b*4+k];
        if(fabsf(dot)>.01f)return false;
    }
    return true;
}
static Matrix RigidInverse(const Matrix& m) {
    Matrix out{};out.m[15]=1;
    for(int c=0;c<3;c++)for(int r=0;r<3;r++)out.m[c*4+r]=m.m[r*4+c];
    return EyeView(out,{m.m[12],m.m[13],m.m[14]});
}
struct Frame {
    Addr task{}, work{}, camera{}, node{}, scene{}, field{}, fieldTask{}, pc{}; uint64_t uid{}, fieldUid{};
    int sequence{-1},mode{},fieldStatus{}; uint32_t flags{},locks{},inputLocks{};
    Matrix view{}; Vec3 root{}; float fovy{},aspect{},nearClip{};
    bool scopedScene{};Addr cameraParent{};Matrix parentWorld{},worldView{};
};
static bool GetFrame(const Tasks& t,Frame& f) {
    f.task=t.camera;
    Addr seqWork=Get<Addr>(t.sequence+0x48);
    f.sequence=seqWork?Get<int>(seqWork+4):-1;
    lastSequence=t.sequence; lastSequenceId=f.sequence; lastField=t.field;
    if(!f.task || !t.field || !t.sequence) { reason="missing exploration tasks"; return false; }
    if(f.sequence!=4 || t.blocked) { reason="event or battle owns camera"; return false; }
    f.work=Get<Addr>(f.task+0x48); f.uid=Get<uint64_t>(f.task+0xb8);
    f.field=Get<Addr>(t.field+0x48); f.fieldTask=t.field; f.fieldUid=Get<uint64_t>(t.field+0xb8);
    // The engine uses the multi-character integer tag 'FMWK', not byte-string
    // memcmp("FMWK"): live PC value is 0x464D574B (bytes 4B 57 4D 46).
    if(!f.work || !f.field || Get<uint32_t>(f.field)!=0x464d574b) { reason="invalid FMWK/layout"; return false; }
    f.camera=Get<Addr>(f.work+0x50); f.node=Get<Addr>(f.work+0x58);
    f.scene=Get<Addr>(globalAddress+0x60);
    f.flags=Get<uint32_t>(f.work+0xc); f.mode=Get<int>(f.work+0x10);
    f.locks=Get<uint32_t>(f.work+0x268); f.inputLocks=Get<uint32_t>(f.work+0x26c);
    f.fieldStatus=Get<int>(f.field+0x1e8);
    // 0x17 is the live-verified ordinary field input state, including subway
    // stairs. Other field procedures must hand back to native cameras.
    if(f.fieldStatus!=23) { reason="native field procedure owns camera";return false; }
    if(!f.camera || !f.node || Get<uint32_t>(f.camera)!=5 || Get<uint32_t>(f.node)!=3) { reason="invalid camera/node types"; return false; }
    if(!f.scene || Get<Addr>(f.scene+0x10)!=f.camera) { reason="different active scene camera"; return false; }
    if((f.flags&4) || f.locks || f.inputLocks) { reason="native camera/input lock"; return false; }
    f.pc=Get<Addr>(f.field+0x2a8);
    if(!f.pc || Get<Addr>(f.pc+0x2e8)!=f.node) { reason="player root identity unconfirmed"; return false; }
    if(!Read(f.node+0x50,f.root) || !Finite(f.root) || !Read(f.camera+0x20,f.view) || !ValidView(f.view)) { reason="invalid position/view"; return false; }
    f.fovy=Get<float>(f.camera+0x1a8); f.aspect=Get<float>(f.camera+0x1ac); f.nearClip=Get<float>(f.camera+0x1a0);
    // Upstream's native Camera::set_fovy takes the same degrees displayed by
    // its 5..175 FOV slider (p5r-freecam/src/state/controls.rs).
    if(!std::isfinite(f.fovy)||f.fovy<5.f||f.fovy>175.f||!std::isfinite(f.aspect)||f.aspect<.5f||f.aspect>4||!std::isfinite(f.nearClip)||f.nearClip<=0||f.nearClip>200) { reason="invalid projection/layout"; return false; }
    if(Get<Addr>(f.field+0x100)!=f.task) { reason="field camera identity unconfirmed"; return false; }
    if(cameraFunction && (Get<Addr>(f.task+0x30)!=cameraFunction || Get<Addr>(f.task+0x40)!=endFunction)) {reason="new camera callbacks require review";return false;}
    reason="ready"; return true;
}
static bool SameExploration(const Frame& f) {
    return lookReady && f.fieldTask==lookFieldTask && f.fieldUid==lookFieldUid && f.field==lookFieldWork && f.pc==lookPc && f.node==lookRoot;
}
static bool GetRetainedFrame(const Tasks& t,Frame& f) {
    f.sequence=t.sequence?Get<int>(Get<Addr>(t.sequence+0x48)+4):-1;
    if(t.battle || f.sequence==5) {reason="mode2: native battle camera";return false;}
    if((f.sequence!=4 && f.sequence!=7) || !t.field) {reason="mode2: no live field player; native cinematic";return false;}
    f.fieldTask=t.field;f.fieldUid=Get<uint64_t>(t.field+0xb8);f.field=Get<Addr>(t.field+0x48);
    if(!f.field || Get<uint32_t>(f.field)!=0x464d574b) {reason="mode2: invalid field";return false;}
    f.fieldStatus=Get<int>(f.field+0x1e8);f.pc=Get<Addr>(f.field+0x2a8);f.node=Get<Addr>(f.pc+0x2e8);
    f.scene=Get<Addr>(globalAddress+0x60);f.camera=Get<Addr>(f.scene+0x10);
    if(!f.pc || !f.node || !f.scene || !f.camera || Get<uint32_t>(f.node)!=3 || Get<uint32_t>(f.camera)!=5) {reason="mode2: invalid player/scene";return false;}
    f.task=t.camera;f.work=Get<Addr>(t.camera+0x48);f.uid=Get<uint64_t>(t.camera+0xb8);
    bool positive=t.camera && Get<Addr>(f.field+0x100)==t.camera && Get<Addr>(f.work+0x50)==f.camera && Get<Addr>(f.work+0x58)==f.node;
    // An old field surviving a scene transition must not anchor the new scene.
    if(!positive && !(SameExploration(f) && f.scene==lookScene)) {reason="mode2: waiting for verified player anchor";return false;}
    if(!Read(f.node+0x50,f.root)||!Finite(f.root)||!Read(f.camera+0x20,f.view)||!ValidView(f.view)) {reason="mode2: invalid view/position";return false;}
    f.cameraParent=Get<Addr>(f.camera+8);f.worldView=f.view;
    if(f.cameraParent) {
        if(Get<uint32_t>(f.cameraParent)!=3 || !Read(f.cameraParent+0x20,f.parentWorld)||!RigidTransform(f.parentWorld)) {reason="mode2: unsupported parent transform";return false;}
        // Verified native setup: worldView = localView * rigidInverse(parent).
        f.worldView=Multiply(f.view,RigidInverse(f.parentWorld));
        if(!ValidView(f.worldView)){reason="mode2: invalid composite camera";return false;}
    }
    f.fovy=Get<float>(f.camera+0x1a8);f.aspect=Get<float>(f.camera+0x1ac);f.nearClip=Get<float>(f.camera+0x1a0);
    if(!std::isfinite(f.fovy)||f.fovy<5||f.fovy>175||!std::isfinite(f.aspect)||f.aspect<.5f||f.aspect>4||!std::isfinite(f.nearClip)||f.nearClip<=0||f.nearClip>200){reason="mode2: invalid projection";return false;}
    f.scopedScene=true;reason="mode2: retained field view";return true;
}
static void EnsureLook(const Frame& f) {
    if(SameExploration(f) && (!f.scopedScene || f.scene==lookScene))return;
    std::array<float,16> view;memcpy(view.data(),f.scopedScene?f.worldView.m:f.view.m,sizeof(f.view));look=p5fp::SeedLook(view);
    lookFieldTask=f.fieldTask;lookFieldUid=f.fieldUid;lookFieldWork=f.field;lookPc=f.pc;lookRoot=f.node;lookScene=f.scene;lookReady=true;
    rootReady=false;p5fp::ResetGait(gait);
    Log("New exploration: seed own yaw=%.1f pitch=%.1f fieldUID=%llu",look.yaw/p5fp::Degrees,look.pitch/p5fp::Degrees,f.fieldUid);
}
static void PauseControls() {ClearMouse();look.vYaw=look.vPitch=0;rootReady=false;p5fp::ResetGait(gait);}
static bool ReadLookInput() {
    lastPadX=lastPadY=0;
    if(!Focused())return false;
    uint16_t axes{};
    if(!Read(padRight,axes))return false;
    lastPadX=std::clamp((int(axes&255)-128)/127.f,-1.f,1.f);
    lastPadY=std::clamp((int(axes>>8)-128)/127.f,-1.f,1.f);
    return true;
}
struct MovementContext {bool eligible{};Frame frame{};float yaw{};};
static thread_local MovementContext movementContext;
static bool RedirectMovement(Vec3 original,float x,float y,float yaw,Vec3& out,Vec3& normalized) {
    if(!Finite(original)||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(yaw)||fabsf(original.y)>.001f)return false;
    float speed=sqrtf(original.x*original.x+original.z*original.z),axis=sqrtf(x*x+y*y);
    if(speed<.0001f || axis<.05f || axis>1.5f)return false;
    float sy=sinf(yaw),cy=cosf(yaw);
    normalized={(-cy*x+sy*y)/axis,0,(sy*x+cy*y)/axis};
    out={normalized.x*speed,original.y,normalized.z*speed};return true;
}
static float __fastcall InputVector(Addr field,Addr outAddress,float delta) {
    float nativeSpeed=inputVectorHook.call<float>(field,outAddress,delta);
    const auto& ctx=movementContext;
    if(!ctx.eligible || field!=ctx.frame.field || !wanted || faulted || !modernLook)return nativeSpeed;
    // Only the field player's current original move invocation is eligible.
    if(Get<uint64_t>(ctx.frame.fieldTask+0xb8)!=ctx.frame.fieldUid || Get<Addr>(ctx.frame.fieldTask+0x48)!=field ||
       Get<int>(field+0x1e8)!=23 || Get<Addr>(field+0x2a8)!=ctx.frame.pc || Get<Addr>(ctx.frame.pc+0x2e8)!=ctx.frame.node)return nativeSpeed;
    Vec3 original{},out{},normalized{};
    if(!Read(outAddress,original)||!RedirectMovement(original,Get<float>(field+0x34bc),Get<float>(field+0x34c0),ctx.yaw,out,normalized))return nativeSpeed;
    static_assert(sizeof(Vec3)==12);
    if(Write(outAddress,out)) {
        if(Write(field+0x3510,normalized))inputCorrections++;
        else {Write(outAddress,original);faulted=true;wanted=false;Log("Movement direction cache write failed; disabled");}
    }
    return nativeSpeed;
}
struct HiddenNode { Addr node{},parent{},name{}; float visibility{}; };
struct HiddenMesh {Addr mesh{},parent{};uint32_t flags{};};
struct RestoreState {
    Frame frame{}; Matrix modified{}; float fovy{},nearClip{}; bool applied{};
    HiddenNode nodes[512]{}; int count{};
    HiddenMesh meshes[512]{};int meshCount{};
};
static RestoreState restore;
static bool SameCameraOwner() {
    const Frame& f=restore.frame;
    if(f.scopedScene) {
        return renderScopeDepth>0 && Get<uint64_t>(f.fieldTask+0xb8)==f.fieldUid && Get<Addr>(f.fieldTask+0x48)==f.field &&
            Get<Addr>(globalAddress+0x60)==f.scene && Get<Addr>(f.scene+0x10)==f.camera && Get<uint32_t>(f.camera)==5 && Get<Addr>(f.camera+8)==f.cameraParent;
    }
    return Get<uint64_t>(f.task+0xb8)==f.uid && Get<Addr>(f.task+0x48)==f.work &&
        Get<Addr>(f.work+0x50)==f.camera && Get<uint32_t>(f.camera)==5;
}
static bool SamePlayerOwner() {
    const Frame& f=restore.frame;
    return Get<uint64_t>(f.fieldTask+0xb8)==f.fieldUid && Get<Addr>(f.fieldTask+0x48)==f.field &&
        Get<Addr>(f.field+0x2a8)==f.pc && Get<Addr>(f.pc+0x2e8)==f.node && Get<uint32_t>(f.node)==3;
}
static void Restore() {
    if(!restore.applied) return;
    restore.applied=false; active=false;
    const Frame& f=restore.frame;
    if(SameCameraOwner()) {
        Matrix now{};
        if(Read(f.camera+0x20,now) && memcmp(&now,&restore.modified,sizeof(now))==0) Write(f.camera+0x20,f.view);
        bool proj=false;
        if(Get<float>(f.camera+0x1a8)==restore.fovy) { Write(f.camera+0x1a8,f.fovy); proj=true; }
        if(Get<float>(f.camera+0x1a0)==restore.nearClip) { Write(f.camera+0x1a0,f.nearClip); proj=true; }
        if(proj) Write(f.camera+0x1b4,Get<uint32_t>(f.camera+0x1b4)|1u);
    } else { Log("Camera owner changed; old camera not written; user intent retained"); }
    // A changed native target must not prevent restoration of the still-live player.
    if(SamePlayerOwner()) {
        for(int i=0;i<restore.meshCount;i++) {
            auto& m=restore.meshes[i];
            if(!(m.flags&0x100000u) && Get<uint32_t>(m.mesh)==2 && Get<Addr>(m.mesh+8)==m.parent) {
                uint32_t now=Get<uint32_t>(m.mesh+0x20);
                if(now&0x100000u)Write(m.mesh+0x20,now&~0x100000u);
            }
        }
        for(int i=0;i<restore.count;i++) {
            auto& n=restore.nodes[i];
            if(Get<uint32_t>(n.node)==3 && Get<Addr>(n.node+8)==n.parent && Get<Addr>(n.node+0xf8)==n.name && Get<float>(n.node+0x108)==0.f)
                Write(n.node+0x108,n.visibility);
        }
    } else if(restore.count) {
        Log("Player resource owner changed; old nodes not written; user intent retained");
    }
    restore.count=0;restore.meshCount=0;
}
static void Hide(Addr root) {
    // Traverse only this validated player subtree: never the root's siblings.
    Addr pending[512]{},seen[1024]{}; int count=1,visited=0; pending[0]=root;
    while(count && visited++<512 && restore.count<512) {
        Addr n=pending[--count]; if(!n || Get<uint32_t>(n)!=3) continue;
        size_t slot=((n>>4)*11400714819323198485ull)&1023;
        while(seen[slot] && seen[slot]!=n)slot=(slot+1)&1023;
        if(seen[slot]==n)continue;
        seen[slot]=n;
        float v=Get<float>(n+0x108);
        if(!std::isfinite(v)||v<0||v>1) continue;
        restore.nodes[restore.count++]={n,Get<Addr>(n+8),Get<Addr>(n+0xf8),v};
        Write(n+0x108,0.f);
        if(restore.frame.scopedScene) {
            // Native post-update has already cached visibility into Mesh bit20.
            // Render submission tests this exact bit, so scope it to player
            // attachments instead of forcing a second animation/model update.
            Addr obj=Get<Addr>(n+0x110);
            for(int j=0;obj && j<128 && restore.meshCount<512;j++) {
                if(Get<Addr>(obj+8)!=n)break;
                if(Get<uint32_t>(obj)==2) {
                    uint32_t flags=Get<uint32_t>(obj+0x20);
                    restore.meshes[restore.meshCount++]={obj,n,flags};Write(obj+0x20,flags|0x100000u);
                }
                Addr next=Get<Addr>(obj+0x18);if(next==obj)break;obj=next;
            }
        }
        Addr child=Get<Addr>(n+0xd8);
        for(int j=0;child && j<512 && count<512;j++) {
            if(Get<Addr>(child+8)!=n) break;
            pending[count++]=child;
            Addr next=Get<Addr>(child+0xe0); if(next==child) break; child=next;
        }
    }
}
static void Status(const Frame& f) {
    DWORD now=GetTickCount(); if(now-lastStatus<500) return; lastStatus=now;
    FILE* fp=nullptr; _wfopen_s(&fp,statusPath,L"wb"); if(!fp)return;
    fprintf(fp,"P5RFirstPerson=1.0.0-rc1\nwanted=%d\nactive=%d\nreason=%s\nmain_thread=%lu\nsequence=%d\ncamera_task=%p\nplayer_node=%p\neye_height_units=%.1f\nhorizontal_fov=%.1f\nhide_player=%d\nhidden_nodes=%d\nmove_calls=%llu\ncamera_calls=%llu\napplied_frames=%llu\n",
        wanted,active,reason,mainThread,f.sequence,(void*)f.task,(void*)f.node,tuning.eye,tuning.fov,hidePlayer,restore.count,moves,cameraUpdates,applications);
    fprintf(fp,"process_id=%lu\nmodern_look=%d\nyaw_deg=%.2f\npitch_deg=%.2f\nright_x=%.3f\nright_y=%.3f\nyaw_speed=%.1f\ngait_amplitude=%.1f\ngait_offset=%.3f\ninput_corrections=%llu\n",GetCurrentProcessId(),modernLook,look.yaw/p5fp::Degrees,look.pitch/p5fp::Degrees,lastPadX,lastPadY,120.f,tuning.gait,gait.offset,inputCorrections);
    fprintf(fp,"interaction_mode=%d\nrender_calls=%llu\nretained_frames=%llu\nhidden_meshes=%d\n",interactionMode,renderCalls,retainedFrames,restore.meshCount);
    fprintf(fp,"mouse_reads=%llu\nmouse_packets=%llu\nmouse_allowed=%d\n",mouseReads.load(),mousePackets.load(),mouseAllowed.load());
    fclose(fp);
}
#include "camera_settings.inl"
static void HandleKeys(const Tasks&, const Frame&, bool) {
    bool next=requested.load();
    if(wanted!=next) { Restore();PauseControls();lookReady=false;wanted=next;reason=next?"first person requested":"disabled by F3"; }
    const auto nextTuning=CameraTuning::Decode(settingBits.load());
    if(nextTuning.gait!=tuning.gait)p5fp::ResetGait(gait);
    tuning=nextTuning;
}
static void UpdateControls(const Frame& f,bool controllable) {
    EnsureLook(f);
    if(!controllable) {PauseControls();lastPadX=lastPadY=0;return;}
    bool inputAvailable=ReadLookInput();lookSettings.yawRate=120.f;lookSettings.pitchRate=lookSettings.yawRate*.65f;
    if(modernLook && inputAvailable)p5fp::StepLook(look,lastPadX,lastPadY,frameSeconds,lookSettings);
    else look.vYaw=look.vPitch=0;
    if(MouseForeground()) {
        if(!mouseAllowed.exchange(true)){mouseX=0;mouseY=0;}
        const int dx=mouseX.exchange(0),dy=mouseY.exchange(0);
        p5fp::StepMouse(look,dx,dy);
    } else ClearMouse();
    if(rootReady) {
        float dx=f.root.x-previousRoot.x,dz=f.root.z-previousRoot.z;
        p5fp::StepGait(gait,sqrtf(dx*dx+dz*dz),f.root.y-previousRoot.y,frameSeconds,tuning.gait,Focused());
    }
    previousRoot=f.root;rootReady=true;
}
static void ApplyFrame(const Frame& f) {
    restore={}; restore.frame=f; restore.modified=f.view;
    Vec3 eye=f.root; eye.y+=tuning.eye+gait.offset;
    if(modernLook) {auto view=p5fp::AimView(look,{eye.x,eye.y,eye.z});memcpy(restore.modified.m,view.data(),sizeof(restore.modified));}
    else restore.modified=EyeView(f.scopedScene?f.worldView:f.view,eye);
    if(f.scopedScene && f.cameraParent)restore.modified=Multiply(restore.modified,f.parentWorld);
    restore.fovy=VerticalFov(tuning.fov,f.aspect);
    restore.nearClip=std::min(f.nearClip,2.f);
    restore.applied=true;
    bool ok=Write(f.camera+0x20,restore.modified) && Write(f.camera+0x1a8,restore.fovy) && Write(f.camera+0x1a0,restore.nearClip) && Write(f.camera+0x1b4,Get<uint32_t>(f.camera+0x1b4)|1u);
    if(!ok) {Restore();faulted=true;wanted=false;reason="write failed; disarmed";Log("%s",reason);}
    else {if(hidePlayer)Hide(f.node);active=true;applications++;reason=f.scopedScene?"mode2: first person during render":"first person active";}
}
static void Update(const Tasks& t, bool allowWrite) {
    Frame f{}; bool valid=GetFrame(t,f);HandleKeys(t,f,valid);
    if(static_cast<int32_t>(GetTickCount()-uiUntil)<0) { valid=false; reason="menu/event handoff"; }
    if(allowWrite && wanted && valid && !faulted) {
        UpdateControls(f,true);
        if(boundTask!=f.task || boundUid!=f.uid) {Log("Fresh camera uid=%llu resumed; same-field heading retained",f.uid);boundTask=f.task;boundUid=f.uid;}
        ApplyFrame(f);
    } else PauseControls();
    Status(f);
}
static uint64_t __fastcall CameraUpdate(Addr task,float dt) {
    Restore();
    uint64_t result=cameraHook.call<uint64_t>(task,dt); cameraUpdates++;
    if(result!=UINT64_MAX && GetCurrentThreadId()==mainThread) {
        Tasks t=FindTasks();
        if(t.camera==task && interactionMode==1) {frameSeconds=FrameSeconds();Update(t,true);}
    }
    return result;
}
static void __fastcall CameraEnd(Addr task) {
    if(task==boundTask || task==restore.frame.task) {Restore();boundTask=0;boundUid=0;PauseControls();reason="camera replacing: waiting for native exploration";Log("Camera shutdown restored; user intent and same-field heading retained");}
    endHook.call<void>(task);
}
static void InstallCamera(const Tasks& t) {
    if(!t.camera || cameraFunction) return;
    Addr fn=Get<Addr>(t.camera+0x30), end=Get<Addr>(t.camera+0x40);
    if(!IsExecutable(fn) || !IsExecutable(end)) { reason="invalid camera callbacks"; return; }
    // End hook is mandatory: no persistent writes without a lifetime exit path.
    endHook=safetyhook::create_inline(reinterpret_cast<void*>(end),reinterpret_cast<void*>(CameraEnd));
    if(!endHook) { Log("Cannot hook camera end; remaining read-only"); return; }
    endFunction=end;
    cameraHook=safetyhook::create_inline(reinterpret_cast<void*>(fn),reinterpret_cast<void*>(CameraUpdate));
    if(!cameraHook) { endHook.reset(); endFunction=0; Log("Cannot hook camera update; remaining read-only"); return; }
    cameraFunction=fn; Log("Camera callbacks update=RVA %llx end=RVA %llx",fn-imageBase,end-imageBase);
}
static void __fastcall MoveUpdate(Addr task,float dt) {
    if(!mainThread) mainThread=GetCurrentThreadId();
    if(GetCurrentThreadId()==mainThread) Restore();
    MovementContext previous=movementContext;movementContext={};
    if(GetCurrentThreadId()==mainThread && wanted && modernLook && !faulted && Focused() && static_cast<int32_t>(GetTickCount()-uiUntil)>=0) {
        Tasks tasks=FindTasks();Frame frame{};
        // A new field's heading is seeded only AFTER its first native camera
        // update. Same-field camera replacement already has a valid own heading.
        if(GetFrame(tasks,frame) && SameExploration(frame))movementContext={true,frame,look.yaw};
    }
    moveHook.call<void>(task,dt); moves++;
    movementContext=previous;
    if(GetCurrentThreadId()==mainThread) {
        Tasks t=FindTasks();
        InstallCamera(t);
        if(interactionMode==1 && (!cameraFunction || (t.camera && Get<Addr>(t.camera+0x30)!=cameraFunction)))Update(t,false);
    }
}
static uint64_t __fastcall EventUpdate(Addr task,float dt) {
    Restore();PauseControls();uiUntil=GetTickCount()+250;
    return eventHook.call<uint64_t>(task,dt);
}
template<int I> static bool __fastcall MenuUpdate(Addr main) {
    Restore(); menuCalls[I]++;
    // Live free exploration polls all four every frame, returning false.
    // Only an accepted request warrants the short transition guard; persistent
    // menu tasks and native field status/locks handle the actual menu lifetime.
    bool result=menuHooks[I].call<bool>(main);
    if(result) {menuTrue[I]++;uiUntil=GetTickCount()+250;} return result;
}

static void __fastcall RenderFrame() {
    // The native frame function prepares scene cameras BEFORE render-task
    // callbacks. Borrow only for this call; no unowned camera pointer survives
    // into the next native update.
    if(renderScopeDepth || !mainThread || GetCurrentThreadId()!=mainThread) {renderHook.call<void>();return;}
    renderCalls++;
    InitializeMouse();
    if(interactionMode==1) {renderHook.call<void>();Restore();return;}
    Restore();renderScopeDepth++;
    Tasks tasks=FindTasks();Frame frame{},controlFrame{};
    bool valid=GetRetainedFrame(tasks,frame);
    const char* retainedReason=reason;
    bool controllable=GetFrame(tasks,controlFrame) && static_cast<int32_t>(GetTickCount()-uiUntil)>=0;
    reason=retainedReason;
    frameSeconds=FrameSeconds();HandleKeys(tasks,frame,valid);
    if(wanted && valid && !faulted) {
        UpdateControls(frame,controllable);ApplyFrame(frame);
        if(active)retainedFrames++;
    } else PauseControls();
    Status(frame);
    renderHook.call<void>();
    Restore();renderScopeDepth--;

}
static Addr Scan(const char* pattern) {
    std::vector<int> bytes;
    for(const char* p=pattern;*p;) {
        if(*p==' ') {p++;continue;} if(*p=='?') {bytes.push_back(-1);p++;if(*p=='?')p++;continue;}
        char* end=nullptr; long v=strtol(p,&end,16); if(end==p) return 0; bytes.push_back(static_cast<int>(v));p=end;
    }
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(imageBase);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(imageBase+dos->e_lfanew);
    auto sec=IMAGE_FIRST_SECTION(nt); Addr found=0; int matches=0;
    for(int s=0;s<nt->FileHeader.NumberOfSections;s++) {
        if(!(sec[s].Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
        size_t n=std::min<size_t>(sec[s].Misc.VirtualSize,imageSize-sec[s].VirtualAddress);
        Addr start=imageBase+sec[s].VirtualAddress;
        for(size_t i=0;i+bytes.size()<=n;i++) {
            if(Get<unsigned char>(start+i)!=bytes[0])continue;
            bool match=true; for(size_t j=1;j<bytes.size();j++) if(bytes[j]>=0 && Get<unsigned char>(start+i+j)!=bytes[j]) {match=false;break;}
            if(match) {found=start+i;matches++;}
        }
    }
    Log("Signature matches=%d RVA=%llx pattern=%s",matches,found?found-imageBase:0,pattern);
    return matches==1?found:0;
}
static DWORD WINAPI Bootstrap(void*) {
    GetModuleFileNameW(selfModule,directory,MAX_PATH); wchar_t* slash=wcsrchr(directory,L'\\'); if(slash)*slash=0;
    swprintf_s(iniPath,L"%s\\P5RFirstPersonCamera.ini",directory); swprintf_s(logPath,L"%s\\P5RFirstPersonCamera.log",directory); swprintf_s(statusPath,L"%s\\P5RFirstPersonCamera.status.txt",directory);
    wchar_t mutexName[100];swprintf_s(mutexName,L"Local\\Osmium.P5RFirstPersonCamera.%lu",GetCurrentProcessId());
    HANDLE instance=CreateMutexW(nullptr,FALSE,mutexName);
    if(!instance || GetLastError()==ERROR_ALREADY_EXISTS) {if(instance)CloseHandle(instance);return 0;}
    Log("P5RFirstPerson 1.0.0-rc1 boot (camera-only: F3 toggle F4 eye F5 FOV F6 bob; Shift reverse, Ctrl coarse/reset)");
    Status(Frame{});
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr,exe,MAX_PATH);
    const wchar_t* name=wcsrchr(exe,L'\\'); if(!name || _wcsicmp(name+1,L"P5R.exe")) { Log("Wrong executable; disabled"); return 0; }
    imageBase=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(imageBase); auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(imageBase+dos->e_lfanew); imageSize=nt->OptionalHeader.SizeOfImage;
    // These layout offsets were reviewed for the Steam executable below.
    if(nt->FileHeader.TimeDateStamp!=0x66c8661a || imageSize!=0x18133000) {Log("Unsupported executable layout; no hooks installed");return 0;}
    if(GetModuleHandleW(L"P5RFirstPerson.asi")) {Log("Legacy all-in-one camera detected; disable it before enabling this module");return 0;}
    LoadSettings();
    QueryPerformanceFrequency(&clockFrequency);
    if(!clockFrequency.QuadPart){Log("No monotonic clock; disabled");return 0;}
    // Steam build15515071 has another unrelated F7/bit25 test at RVA11b50bc.
    // The GFD graphics-flag call sequence at RVA21cffc is uniquely distinguished
    // by its JE+5/CALL continuation (verified in this pinned executable).
    Addr globalSig=Scan("F7 05 ?? ?? ?? ?? 00 00 00 02 74 05 E8 ?? ?? ?? ??");
    Addr move=Scan("40 53 48 83 EC 50 48 8B 59 ?? 0F 29 74 24 ?? 0F 28 F1");
    Addr event=Scan("48 89 4C 24 ?? 53 56 57 41 54 48 81 EC A8 0C 00 00");
    Addr procSig=Scan("48 8D 2D ?? ?? ?? ?? 66 21 83 ?? ?? ?? ??");
    Addr inputVector=Scan("48 8B C4 48 89 58 18 48 89 70 20 55 57 41 57 48 8D 68 C8 48 81 EC 20 01 00 00");
    Addr buttonsSig=Scan("66 89 0D ?? ?? ?? ?? B8 FF 00 00 00 79 05 41 8B D7 EB 05");
    Addr rightSig=Scan("88 15 ?? ?? ?? ?? 79 05 45 8B C7 EB 07 44 3B C0 44 0F 4F C0");
    Addr leftSig=Scan("44 88 0D ?? ?? ?? ?? 79 05 45 8B D7 EB 07 44 3B D0 44 0F 4F D0");
    Addr render=Scan("48 89 5C 24 08 57 48 83 EC 60 48 83 3D ?? ?? ?? ?? 00 74 07 33 C9");
    if(!globalSig || !move || !event || !procSig || !inputVector || !buttonsSig || !rightSig || !leftSig || !render) { Log("Required unique signatures missing; no hooks installed"); return 0; }
    padButtons=buttonsSig+7+Get<int32_t>(buttonsSig+3);padRight=rightSig+6+Get<int32_t>(rightSig+2);padLeft=leftSig+7+Get<int32_t>(leftSig+3);
    if(padButtons<imageBase || padLeft+8>imageBase+imageSize || padRight!=padButtons+8 || padLeft!=padButtons+16) {Log("Invalid processed pad arrays; disabled");return 0;}
    Log("Processed native pad buttons/RX/LX RVAs=%llx/%llx/%llx",padButtons-imageBase,padRight-imageBase,padLeft-imageBase);
    globalAddress=globalSig+6+Get<int32_t>(globalSig+2)-44;
    if(globalAddress<imageBase || globalAddress+0x2ca0>imageBase+imageSize) { Log("Invalid global address");return 0; }
    Log("gfdGlobal RVA=%llx",globalAddress-imageBase);
    // Pin a shared near pool BEFORE installing any hook. Reloaded/CoreCLR and
    // the game's initial resource buffers can temporarily occupy this range.
    // Reuse this pool for all hooks and wait boundedly for startup allocations
    // to settle, instead of leaving a partially installed camera module.
    auto bootstrapAllocator=safetyhook::Allocator::global();
    safetyhook::Allocation bootstrapReserve;
    for(int attempt=0;attempt<30;attempt++) {
        auto reserve=bootstrapAllocator->allocate_near({reinterpret_cast<uint8_t*>(imageBase),
            reinterpret_cast<uint8_t*>(imageBase+imageSize-1)},128);
        if(reserve) {
            bootstrapReserve=std::move(*reserve);
            Log("Near hook pool ready address=%p attempt=%d",bootstrapReserve.data(),attempt+1);
            break;
        }
        if(attempt==0 || attempt%5==0)Log("Waiting for near hook pool: attempt=%d Win32=%lu",
            attempt+1,safetyhook::p5_virtual_alloc2_error);
        Sleep(1000);
    }
    if(!bootstrapReserve) {reason="near hook memory unavailable";faulted=true;Status(Frame{});return 0;}
    Addr table=procSig+7+Get<int32_t>(procSig+3);
    // xrd744-lib/src/fld/proc.rs: two x64 function pointers, then two u32s.
    // exec is the first field; +0x10 is ret_state, not a function pointer.
    struct ProcTableEntry { Addr exec, check; uint32_t returnState, flags; };
    static_assert(sizeof(ProcTableEntry)==0x18);
    constexpr int indices[]{0xf,0x10,0x11,0x16};
    void* detours[]{reinterpret_cast<void*>(MenuUpdate<0>),reinterpret_cast<void*>(MenuUpdate<1>),reinterpret_cast<void*>(MenuUpdate<2>),reinterpret_cast<void*>(MenuUpdate<3>)};
    for(int i=0;i<4;i++) {
        Addr fn=Get<Addr>(table+indices[i]*sizeof(ProcTableEntry));
        if(!IsExecutable(fn)) { Log("Invalid proc table entry %d; no camera edits",indices[i]); faulted=true; break; }
        auto hookResult=safetyhook::InlineHook::create(reinterpret_cast<void*>(fn),detours[i]);
        if(!hookResult) {
            Log("Near hook failure=%d allocator=%d VirtualAlloc=%lu VirtualAlloc2=%lu",
                safetyhook::p5_near_error,safetyhook::p5_near_allocator_error,
                safetyhook::p5_virtual_alloc_error,safetyhook::p5_virtual_alloc2_error);
            const auto& error=hookResult.error();
            if(error.type==safetyhook::InlineHook::Error::BAD_ALLOCATION)
                Log("Menu hook %d at RVA=%llx failed: error=%u allocator=%u",indices[i],fn-imageBase,
                    static_cast<unsigned>(error.type),static_cast<unsigned>(error.allocator_error));
            else
                Log("Menu hook %d at RVA=%llx failed: error=%u instruction=%p",indices[i],fn-imageBase,
                    static_cast<unsigned>(error.type),error.ip);
            reason="menu hook initialization failed";faulted=true;break;
        }
        menuHooks[i]=std::move(*hookResult);
    }
    if(faulted) { for(auto& h:menuHooks)h.reset();return 0; }
    inputVectorHook=safetyhook::create_inline(reinterpret_cast<void*>(inputVector),reinterpret_cast<void*>(InputVector));
    if(!inputVectorHook) {for(auto& h:menuHooks)h.reset();Log("Input vector hook failed");return 0;}
    eventHook=safetyhook::create_inline(reinterpret_cast<void*>(event),reinterpret_cast<void*>(EventUpdate));
    if(!eventHook) {inputVectorHook.reset();for(auto& h:menuHooks)h.reset();Log("Event handoff hook failed");return 0;}
    moveHook=safetyhook::create_inline(reinterpret_cast<void*>(move),reinterpret_cast<void*>(MoveUpdate));
    if(!moveHook) {inputVectorHook.reset();eventHook.reset();for(auto& h:menuHooks)h.reset();Log("Move observer failed");return 0;}
    renderHook=safetyhook::create_inline(reinterpret_cast<void*>(render),reinterpret_cast<void*>(RenderFrame));
    if(!renderHook) {moveHook.reset();inputVectorHook.reset();eventHook.reset();for(auto& h:menuHooks)h.reset();Log("Render borrow hook failed");return 0;}
    Log("Observer/handoff hooks installed; waiting for exploration. No freecam movement suppression.");
    bool down[4]{};
    for(int i=0;i<4;i++)down[i]=(GetAsyncKeyState(VK_F3+i)&0x8000)!=0;
    for(;;) { PollSettings(down);FlushSettings();Sleep(8); }
}
static volatile LONG started;
extern "C" __declspec(dllexport) void InitializeASI() {
    if(InterlockedCompareExchange(&started,1,0)!=0)return;
    HANDLE thread=CreateThread(nullptr,0,Bootstrap,nullptr,0,nullptr);
    if(thread)CloseHandle(thread);
}
extern "C" __declspec(dllexport) void ReloadedStart() { InitializeASI(); }
BOOL APIENTRY DllMain(HMODULE module,DWORD reasonCode,LPVOID) {
    if(reasonCode==DLL_PROCESS_ATTACH) {selfModule=module;DisableThreadLibraryCalls(module);InitializeASI();}
    return TRUE;
}

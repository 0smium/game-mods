// Production garden_visibility.inl is compiled verbatim. Adapters supply only
// the Unreal object table/context/property-discovery/native-dispatch boundary.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

namespace SDK {
struct UObject; struct UClass; struct UField; struct UFunction; struct UWorld;
struct FName { std::string value; std::string ToString() const { return value; } };
struct UObject { UClass* Class{}; UWorld* world{}; int index{}; std::uint64_t Name{}; std::string name; bool live{true}, unresolved{}; };
struct UField : UObject { UObject* Outer{}; UField* Next{}; };
struct FFieldClass { FName Name; };
struct FOwner { bool bIsUObject{true}; struct { UObject* Object{}; } Container; };
struct FProperty {
    FFieldClass* ClassPrivate{}; FName Name; FOwner Owner; FProperty* Next{};
    int ArrayDim{1}, ElementSize{1}, Offset{};
    std::uint64_t PropertyFlags{0x80};
};
struct FBoolProperty : FProperty { unsigned ByteOffset{}, FieldSize{1}, FieldMask{255}; };
struct FArrayProperty : FProperty { FProperty* InnerProperty{}; };
struct UFunction : UField { int Size{1}; unsigned FunctionFlags{0x08000000}; void* ExecFunction{reinterpret_cast<void*>(1)}; FProperty* ChildProperties{}; };
struct UClass : UObject { int Size{}; UField* Children{}; FProperty* ChildProperties{}; std::string kind; std::vector<std::string> bases; };
using UStruct=UClass;
template<class T> struct TArray { T* Data{};int count{},capacity{};int Num()const{return count;}T operator[](int i)const{return Data[i];} };
struct ULevel : UObject { UWorld* OwningWorld{}; TArray<UObject*> Actors; };
struct UWorld : UObject { UObject* AuthorityGameMode{}; TArray<ULevel*> Levels; };
struct AProjectPlayerCameraManager_C : UObject { bool contextValid{true}, bound{true}; const char* block{}; };
}
using ULONGLONG = unsigned long long;
ULONGLONG mockNow = 10000;
unsigned long mockThread = 1;
ULONGLONG GetTickCount64() { return mockNow; }
unsigned long GetCurrentThreadId() { return mockThread; }
std::vector<std::string> logs;
void Log(const char* format,...) { char text[1024];va_list args;va_start(args,format);std::vsnprintf(text,sizeof(text),format,args);va_end(args);logs.push_back(text); }
std::set<const void*> unreadable;
bool Readable(const void* p,std::size_t) { return p && !unreadable.count(p); }
bool Executable(const void* p) { return p == reinterpret_cast<void*>(1); }
bool IndexedObject(const SDK::UObject* p) { return p && p->live && !p->unresolved && Readable(p,sizeof(*p)); }
bool ValidObject(const SDK::UObject* p) { return IndexedObject(p)&&p->Class; }
std::string Name(const SDK::UObject* p) { return p ? p->name : "none"; }
std::string ClassName(const SDK::UObject* p) { return p && p->Class ? p->Class->kind : "none"; }
bool HasClass(const SDK::UObject* p,const char* kind) {
    return p && p->Class && (p->Class->kind==kind ||
        std::find(p->Class->bases.begin(),p->Class->bases.end(),kind)!=p->Class->bases.end());
}
SDK::UWorld* OwningWorld(const SDK::UObject* p) { return p ? p->world : nullptr; }
struct ObjectIdentity { SDK::UObject* pointer{}; int index{}; };
ObjectIdentity Identify(SDK::UObject* p) { return IndexedObject(p) ? ObjectIdentity{p,p->index} : ObjectIdentity{}; }
bool Matches(const ObjectIdentity& id,const SDK::UObject* p) { return id.pointer==p && id.pointer && IndexedObject(p) && id.index==p->index; }
enum class ObjectState { Unresolved, Live, Retired };
ObjectState Resolve(const ObjectIdentity& id,SDK::UObject*& out) {
    out=nullptr;
    if (!id.pointer) return ObjectState::Unresolved;
    if (!id.pointer->live || id.pointer->index!=id.index) return ObjectState::Retired;
    if (!IndexedObject(id.pointer)) return ObjectState::Unresolved;
    out=id.pointer; return ObjectState::Live;
}
struct CameraContext { SDK::AProjectPlayerCameraManager_C* manager{}; SDK::UWorld* world{}; };
bool Context(SDK::AProjectPlayerCameraManager_C* m,CameraContext& c) { c={m,m->world}; return m->contextValid; }
bool BoundTo(const CameraContext& c) { return c.manager->bound; }
const char* ViewBlock(const CameraContext& c) { return c.manager->block; }
struct { bool valid{}; ObjectIdentity manager; } binding;
std::atomic<bool> requested{true};
bool applied=true,restorationPending=false,insideProbe=true;
std::atomic<unsigned long> sampleThread{1};

struct Wrapper : SDK::UObject { bool permission{true}; bool actorHidden{true}; float materialOpacity{0.4f}; int collision{7}; };
struct PermissionProperty { int offset{-1},size{1}; unsigned byteOffset{},mask{255}; std::string kind{"BoolProperty"}; };
PermissionProperty permissionProperty;
std::map<std::pair<SDK::UObject*,std::string>,SDK::UObject*> objectProps;
std::map<std::pair<SDK::UObject*,std::string>,double> scalarProps;
namespace GardenDiagnostics {
PermissionProperty FindProperty(SDK::UObject*,const char*) { return permissionProperty; }
SDK::UObject* ObjectProperty(SDK::UObject* o,const char* p) { auto it=objectProps.find({o,p}); return it==objectProps.end()?nullptr:it->second; }
bool Scalar(SDK::UObject* o,const char* p,double& out) { auto it=scalarProps.find({o,p}); if(it==scalarProps.end())return false;out=it->second;return true; }
}
namespace MeshVisibility { template<class T> bool ArrayReadable(const T& a,int max) { return a.Num()>=0&&a.Num()<=max&&a.Num()<=a.capacity&&(!a.Num()||a.Data); } }
std::vector<std::pair<SDK::UObject*,bool>> writes;
std::function<void(Wrapper&,bool)> nativeEffect;
struct NativeDispatch {
    template<class T> void unsafe_call(SDK::UObject* object,SDK::UFunction*,void* args) {
        bool value=*static_cast<bool*>(args);
        writes.emplace_back(object,value);
        auto& wrapper=*static_cast<Wrapper*>(object);
        if(nativeEffect) nativeEffect(wrapper,value); else wrapper.permission=value;
    }
} processEventHook;

#include "../mods/smtvv/common/garden_visibility.inl"

void Require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int nextIndex=1;
void Register(SDK::UObject& o,SDK::UClass& c,SDK::UWorld& w,const char* name) { o.Class=&c;o.world=&w;o.index=nextIndex++;o.name=name; }
struct Scene {
    struct Manager:SDK::UObject{SDK::TArray<SDK::UObject*> wrappers;};
    SDK::UClass meta,ordinary,wrapperClass,devilClass,gardenClass,functionClass;
    SDK::UWorld world;
    SDK::ULevel level;
    SDK::UObject mode,sublevels,devil1,devil2;
    Manager gardenManager;
    SDK::AProjectPlayerCameraManager_C camera;
    Wrapper first,second;
    SDK::UFunction setter,tick;
    SDK::FFieldClass booleanClass,arrayClass,objectClass;
    SDK::FBoolProperty parameter;
    SDK::FArrayProperty arrayProperty;
    SDK::FProperty innerProperty;
    SDK::UObject* wrapperPointers[2]{};
    Scene() {
        for(auto c:{&meta,&ordinary,&wrapperClass,&devilClass,&gardenClass,&functionClass}) {c->Class=&meta;c->index=nextIndex++;c->Size=4096;}
        meta.kind="Class";ordinary.kind="Object";wrapperClass.kind="BP_GardenDevil_C";wrapperClass.bases={"GardenDevil"};
        devilClass.kind="DevilBase_C";devilClass.bases={"CharaBase_C"};gardenClass.kind="BP_GardenManager_C";functionClass.kind="Function";
        for(auto c:{&meta,&ordinary,&wrapperClass,&devilClass,&gardenClass,&functionClass})c->Name=std::hash<std::string>{}(c->kind);
        Register(world,ordinary,world,"world");Register(level,ordinary,world,"level");level.OwningWorld=&world;
        Register(mode,ordinary,world,"mode");Register(sublevels,ordinary,world,"sublevels");world.AuthorityGameMode=&mode;
        Register(gardenManager,gardenClass,world,"gardenManager");Register(camera,ordinary,world,"camera");
        Register(devil1,devilClass,world,"devil1");Register(devil2,devilClass,world,"devil2");
        Register(first,wrapperClass,world,"first");Register(second,wrapperClass,world,"second");
        wrapperPointers[0]=&first;wrapperPointers[1]=&second;gardenManager.wrappers={wrapperPointers,2,2};
        Register(tick,functionClass,world,"ReceiveTick");tick.Outer=&gardenClass;
        arrayClass.Name.value="ArrayProperty";objectClass.Name.value="ObjectProperty";
        arrayProperty.ClassPrivate=&arrayClass;arrayProperty.Name.value="GardenDevils";arrayProperty.Owner.Container.Object=&gardenClass;
        arrayProperty.ElementSize=sizeof(gardenManager.wrappers);arrayProperty.Offset=static_cast<int>(reinterpret_cast<char*>(&gardenManager.wrappers)-reinterpret_cast<char*>(&gardenManager));
        arrayProperty.InnerProperty=&innerProperty;innerProperty.ClassPrivate=&objectClass;innerProperty.ElementSize=sizeof(void*);
        gardenClass.ChildProperties=&arrayProperty;gardenClass.Size=sizeof(Manager);
        Register(setter,functionClass,world,"SetCanBeHiddenFlag");setter.Outer=&wrapperClass;wrapperClass.Children=&setter;
        wrapperClass.Size=sizeof(Wrapper);
        booleanClass.Name.value="BoolProperty";parameter.Name.value="CanBeHidden";parameter.Owner.Container.Object=&setter;
        parameter.ClassPrivate=&booleanClass;setter.ChildProperties=&parameter;
        permissionProperty.offset=static_cast<int>(reinterpret_cast<char*>(&first.permission)-reinterpret_cast<char*>(&first));
        objectProps[{&mode,"MapSubLevelManager"}]=&sublevels;
        scalarProps[{&sublevels,"IsGardenLoaded"}]=scalarProps[{&sublevels,"IsEndLoadGarden"}]=1;
        scalarProps[{&gardenManager,"GardenDevilsReady"}]=1;
        for(auto pair:{std::pair<Wrapper*,SDK::UObject*>{&first,&devil1},{&second,&devil2}}) {
            objectProps[{pair.first,"DevilBaseInstance"}]=pair.second;
            objectProps[{pair.first,"BP Garden Manager"}]=&gardenManager;
            scalarProps[{pair.first,"m_IsInitialiationDone"}]=1;
        }
        binding={true,Identify(&camera)};
    }
    bool Update(){ bool entered=GardenVisibilityBefore(&gardenManager,&tick);mockNow+=150;return entered; }
    void End(){GardenVisibilityAfter();}
};
void Reset(){
    GardenVisibility::entries.clear();GardenVisibility::scopeActive=false;GardenVisibility::lastFailure=0;GardenVisibility::lastApplyLog=0;
    GardenVisibility::restoring=false;mockNow=10000;mockThread=1;nextIndex=1;unreadable.clear();
    objectProps.clear();scalarProps.clear();writes.clear();logs.clear();nativeEffect={};permissionProperty={};
    requested=true;applied=true;restorationPending=false;insideProbe=true;sampleThread=1;binding={};
}
void ApplyOnlyPermission(){Scene s;Require(s.Update(),"exact garden tick enters");Require(!s.first.permission&&!s.second.permission,"linked wrappers must disable permission");Require(writes.size()==2,"exactly one native setter per initial true");Require(s.first.actorHidden&&s.first.materialOpacity==0.4f&&s.first.collision==7,"existing hidden/material/collision state untouched");Require(!s.Update(),"nested tick does not enter");Require(writes.size()==2,"nested tick makes no repeated setter calls");s.End();Require(s.first.permission&&s.second.permission&&!GardenVisibility::scopeActive&&GardenVisibility::entries.empty(),"all permission ownership released before callback returns");}
void NativeFalseUnowned(){Scene s;s.first.permission=false;s.Update();Require(GardenVisibility::entries.size()==1,"original false never claimed");Require(GardenVisibilityRestore("test"),"restore completes");Require(!s.first.permission&&s.second.permission,"original false preserved");}
void RestoreUnresolvedIndependently(){Scene s;s.Update();s.second.unresolved=true;s.End();Require(GardenVisibility::restoring&&!GardenVisibility::scopeActive,"unresolved snapshot stays pending after scope ends");Require(s.first.permission&&!s.second.permission,"other live snapshot restores independently");const auto before=writes.size();Require(!s.Update(),"pending prior restore blocks a new scope");Require(writes.size()==before,"pending restore prevents fresh applications");s.second.unresolved=false;Require(GardenVisibilityRestore("test"),"retry restores old unresolved snapshot");Require(s.second.permission,"second original restored");}
void RetiredNoWrites(){Scene s;s.Update();s.second.live=false;const auto before=writes.size();Require(GardenVisibilityRestore("test"),"retired snapshot released");Require(writes.size()==before+1&&s.first.permission&&!s.second.permission,"retired object not written");}
void NativeNewTrueRetained(){Scene s;s.Update();s.first.permission=true;const auto before=writes.size();Require(GardenVisibilityRestore("test"),"native true release completes");Require(writes.size()==before+1,"already native true needs no restoring write");}
void InactiveGuardsRestore(){
    for(int guard=0;guard<8;++guard){Reset();Scene s;s.Update();switch(guard){case 0:requested=false;break;case 1:applied=false;break;case 2:restorationPending=true;break;case 3:s.camera.contextValid=false;break;case 4:s.camera.bound=false;break;case 5:s.camera.block="event-camera";break;case 6:scalarProps[{&s.sublevels,"IsGardenLoaded"}]=0;break;case 7:scalarProps[{&s.sublevels,"IsEndLoadGarden"}]=0;break;}s.End();Require(s.first.permission&&s.second.permission,"same callback end restores despite changed eligibility");Require(!s.Update(),"inactive guard prevents next scope");}
}
void UnrelatedCameraPreservesOwnership(){Scene s;s.Update();SDK::AProjectPlayerCameraManager_C other;Register(other,s.ordinary,s.world,"other");Require(!GardenVisibilityBefore(&other,&s.tick),"unrelated callback never enters");Require(!s.first.permission&&!s.second.permission&&writes.size()==2,"unrelated callback cannot release current overrides");s.End();}
void InvalidLinksNeverApply(){Scene s;objectProps[{&s.first,"DevilBaseInstance"}]=nullptr;scalarProps[{&s.second,"m_IsInitialiationDone"}]=0;s.Update();Require(writes.empty(),"invalid or uninitialized wrapper rejected");}
void LinkedIdentityChangeRestores(){Scene s;s.Update();SDK::UObject replacement;Register(replacement,s.devilClass,s.world,"replacement");objectProps[{&s.first,"DevilBaseInstance"}]=&replacement;s.End();Require(!s.first.permission&&s.second.permission&&GardenVisibility::entries.empty(),"changed linked identity retires old ownership without writing the new binding");}
void SetterSignatureRejections(){
    for(int fault=0;fault<13;++fault){Reset();Scene s;switch(fault){case 0:s.setter.Size=2;break;case 1:s.setter.FunctionFlags=0;break;case 2:s.setter.ExecFunction=nullptr;break;case 3:s.setter.Outer=&s.world;break;case 4:s.parameter.Name.value="wrong";break;case 5:s.parameter.Owner.Container.Object=&s.world;break;case 6:s.parameter.Offset=1;break;case 7:s.parameter.FieldMask=1;break;case 8:s.parameter.Next=&s.parameter;break;case 9:s.parameter.PropertyFlags|=0x100;break;case 10:s.parameter.Owner.bIsUObject=false;break;case 11:s.parameter.ElementSize=2;break;case 12:s.booleanClass.Name.value="ByteProperty";break;}s.Update();Require(writes.empty()&&GardenVisibility::entries.empty(),"mismatched original setter signature rejected without snapshot/write");}
}
void WrongThreadNoDispatch(){Scene s;mockThread=2;s.Update();Require(writes.empty(),"wrong thread never dispatches native setter");}
void PartialApplyFailureBlocksRetry(){Scene s;nativeEffect=[&](Wrapper&w,bool v){w.permission=v;if(&w==&s.second&&!v)w.unresolved=true;};Require(s.Update(),"partially entered scope still requests finally cleanup");Require(s.first.permission&&!s.second.permission&&GardenVisibility::restoring,"partial native failure restores readable first and retains second");s.End();Require(!GardenVisibility::scopeActive,"finally closes failed scope");const auto before=writes.size();Require(!s.Update(),"unresolved partial failure blocks next scope");Require(writes.size()==before,"unresolved partial failure blocks new permissions");s.second.unresolved=false;nativeEffect={};Require(GardenVisibilityRestore("recovered"),"partial failure eventually restores");Require(s.first.permission&&s.second.permission,"both permissions recovered");}
void DialogueBetweenTicksPreserved(){Scene s;s.Update();s.End();s.first.permission=false;Require(s.Update(),"next tick enters after dialogue changed native flag");Require(GardenVisibility::entries.size()==1,"dialogue false not claimed on next tick");s.End();Require(!s.first.permission&&s.second.permission,"same-value dialogue protection between ticks survives");s.first.permission=true;s.Update();s.End();Require(s.first.permission&&s.second.permission,"dialogue release restores normal tick behavior");}
void ExactTickAndArrayGuards(){for(int fault=0;fault<10;++fault){Reset();Scene s;switch(fault){case 0:s.tick.name="Tick Hit";break;case 1:s.tick.Outer=&s.world;break;case 2:s.tick.live=false;break;case 3:s.arrayProperty.Owner.Container.Object=&s.world;break;case 4:s.arrayProperty.ElementSize=1;break;case 5:s.innerProperty.ElementSize=4;break;case 6:s.objectClass.Name.value="FloatProperty";break;case 7:s.gardenManager.wrappers.count=129;s.gardenManager.wrappers.capacity=129;break;case 8:s.arrayProperty.Offset=s.gardenClass.Size;break;case 9:scalarProps[{&s.gardenManager,"GardenDevilsReady"}]=0;break;}Require(!s.Update()&&writes.empty()&&!GardenVisibility::scopeActive,"wrong tick or malformed garden array rejected before scope");}}
void PermissionLayoutRejections(){for(int fault=0;fault<5;++fault){Reset();Scene s;switch(fault){case 0:permissionProperty.kind="ByteProperty";break;case 1:permissionProperty.size=2;break;case 2:permissionProperty.mask=1;break;case 3:permissionProperty.byteOffset=1;break;case 4:permissionProperty.offset=s.wrapperClass.Size;break;}s.Update();Require(writes.empty(),"invalid permission storage rejected");}}
int main(){
    const std::vector<std::pair<const char*,void(*)()>> cases{
        {"apply only permission; native hidden timeline untouched",ApplyOnlyPermission},
        {"original false is unowned",NativeFalseUnowned},
        {"unresolved restore independent and blocks reapply",RestoreUnresolvedIndependently},
        {"retired wrapper never written",RetiredNoWrites},
        {"new native true retained without write",NativeNewTrueRetained},
        {"eight context and state exits restore",InactiveGuardsRestore},
        {"unrelated manager cannot release binding",UnrelatedCameraPreservesOwnership},
        {"invalid links and uninitialized wrapper never apply",InvalidLinksNeverApply},
        {"changed linked devil retires old ownership",LinkedIdentityChangeRestores},
        {"thirteen setter signature failures rejected",SetterSignatureRejections},
        {"wrong callback thread never dispatches",WrongThreadNoDispatch},
        {"partial native apply failure retains and recovers",PartialApplyFailureBlocksRetry},
        {"five permission layout failures rejected",PermissionLayoutRejections},
        {"dialogue same-value false between ticks preserved",DialogueBetweenTicksPreserved},
        {"ten exact tick and array guards reject",ExactTickAndArrayGuards}};
    unsigned failures=0;for(auto [name,test]:cases){Reset();try{test();std::printf("PASS %s\n",name);}catch(const std::exception&e){++failures;std::printf("FAIL %s: %s\n",name,e.what());for(const auto&line:logs)std::printf("  %s\n",line.c_str());}}
    std::printf("%zu scenarios, %u failures\n",cases.size(),failures);return failures?1:0;
}

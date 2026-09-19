// Compile the complete production module. The adapters model engine reflection,
// object identity, local-player links, and the observed pure overlap delegate.
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDK {
struct UClass; struct UWorld; struct UFunction; struct ULocalPlayer; struct APlayerController;
struct FName { std::string value; std::string ToString() const { return value; } };
struct UObject { UClass* Class{}; UObject* Outer{}; UWorld* world{}; int index{}; bool live{true}; std::string name; };
struct FFieldClass { FName Name; };
struct FProperty {
    FFieldClass* ClassPrivate{}; FName Name;
    struct { bool bIsUObject{true}; struct { UObject* Object{}; } Container; } Owner;
    FProperty* Next{}; int ArrayDim{1}, ElementSize{}, Offset{}; std::uint64_t PropertyFlags{0x80};
};
struct UFunction : UObject { int Size{0xa8}; unsigned FunctionFlags{0x8400000}; void* ExecFunction{reinterpret_cast<void*>(1)}; FProperty* ChildProperties{}; };
struct UClass : UObject { std::uint64_t Name{}; std::string kind; std::vector<std::string> bases; };
template<class T> struct TArray { T* Data{}; int count{},capacity{}; int Num() const{return count;} T operator[](int i)const{return Data[i];} };
struct UGameInstance : UObject { TArray<ULocalPlayer*> LocalPlayers; };
struct UWorld : UObject { UObject* AuthorityGameMode{}; UGameInstance* OwningGameInstance{}; };
struct UGameViewportClient : UObject { UWorld* World{}; };
struct ULocalPlayer : UObject { UGameViewportClient* ViewportClient{}; APlayerController* PlayerController{}; };
struct APlayerController : UObject { UObject* Player{}; UObject* Pawn{}; };
}
using ULONGLONG=unsigned long long;
unsigned long threadId=1;
ULONGLONG GetTickCount64(){return 10000;}
unsigned long GetCurrentThreadId(){return threadId;}
void Log(const char*,...){}
bool insideProbe=true,applied=false;
std::atomic<bool> requested{false};
std::atomic<unsigned long> sampleThread{1};
std::set<const void*> unreadable;
bool Readable(const void* p,std::size_t){return p&&!unreadable.count(p);}
bool Executable(const void* p){return p==reinterpret_cast<void*>(1);}
bool IndexedObject(const SDK::UObject* p){return p&&p->live&&Readable(p,sizeof(*p));}
std::string Name(const SDK::UObject* p){return p?p->name:"none";}
std::string ClassName(const SDK::UObject* p){return p&&p->Class?p->Class->kind:"none";}
bool HasClass(const SDK::UObject* p,const char* name){return p&&p->Class&&(p->Class->kind==name||std::find(p->Class->bases.begin(),p->Class->bases.end(),name)!=p->Class->bases.end());}
SDK::UWorld* OwningWorld(const SDK::UObject* p){return p?p->world:nullptr;}
std::map<std::pair<SDK::UObject*,std::string>,SDK::UObject*> refs;
std::map<std::pair<SDK::UObject*,std::string>,double> values;
namespace GardenDiagnostics {
SDK::UObject* ObjectProperty(SDK::UObject* p,const char* name){auto i=refs.find({p,name});return i!=refs.end()&&IndexedObject(i->second)?i->second:nullptr;}
bool Scalar(SDK::UObject* p,const char* name,double& out){auto i=values.find({p,name});if(i==values.end()||!IndexedObject(p))return false;out=i->second;return true;}
}
namespace MeshVisibility { template<class T> bool ArrayReadable(const T& a,int max){return a.Num()>=0&&a.Num()<=max&&a.Num()<=a.capacity&&(!a.Num()||a.Data);} }

#include "../mods/smtvv/common/garden_free_camera.inl"

void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int nextIndex=1;
struct Scene {
    SDK::UClass meta,ordinary,wrapperClass,devilClass,ditherClass,gardenClass,freeManagerClass,controllerClass,pawnClass,gameClass,collisionClass,functionClass;
    SDK::UWorld world,otherWorld;
    SDK::UObject wrapper,devil,dither,garden,freeManager,pawn,collision,ordinaryCollision,cameraCollision,mode,sublevels;
    SDK::UGameInstance game;
    SDK::ULocalPlayer local,otherLocal;
    SDK::UGameViewportClient viewport;
    SDK::APlayerController controller;
    SDK::ULocalPlayer* locals[1]{};
    SDK::UFunction begin;
    std::array<SDK::FFieldClass,6> kinds;
    std::array<SDK::FProperty,6> properties;
    std::array<std::uint8_t,0xa8> parameters{};
    int nativeCalls{},ditherActivations{};
    bool actorHidden{},tickEnabled{true};
    float currentFade{},materialOpacity{1};
    int collisionProfile=77;
    Scene(){
        auto type=[&](SDK::UClass& c,const char* name,std::vector<std::string> bases={}){c.Class=&meta;c.kind=name;c.name=name;c.bases=bases;c.index=nextIndex++;c.Name=std::hash<std::string>{}(name);};
        type(meta,"Class");type(ordinary,"Object");type(wrapperClass,"BP_GardenDevil_C",{"GardenDevil"});type(devilClass,"Dev798_C",{"CharaBase_C"});
        type(ditherClass,"BP_GardenDevilDitherComponent_C");type(gardenClass,"BP_GardenManager_C");type(freeManagerClass,"BP_GardenFreeCameraManager_C");
        type(controllerClass,"BP_FreeCameraController_C",{"PlayerController"});type(pawnClass,"GardenFreeCameraPawn",{"Pawn"});type(gameClass,"ProjectGameInstance_C");
        type(collisionClass,"CapsuleComponent");type(functionClass,"Function");
        auto object=[&](SDK::UObject& o,SDK::UClass& c,const char* name){o.Class=&c;o.name=name;o.world=&world;o.index=nextIndex++;};
        object(world,ordinary,"world");object(otherWorld,ordinary,"otherWorld");otherWorld.world=&otherWorld;
        object(wrapper,wrapperClass,"wrapper");object(devil,devilClass,"devil");object(dither,ditherClass,"dither");dither.Outer=&wrapper;
        object(garden,gardenClass,"garden");object(freeManager,freeManagerClass,"freeManager");freeManager.Outer=&garden;
        object(pawn,pawnClass,"pawn");object(controller,controllerClass,"controller");object(game,gameClass,"game");
        object(collision,collisionClass,"freeCollision");collision.Outer=&wrapper;
        object(ordinaryCollision,collisionClass,"interactionCollision");object(cameraCollision,collisionClass,"ordinaryCameraCollision");
        object(mode,ordinary,"mode");object(sublevels,ordinary,"sublevels");object(local,ordinary,"local");object(otherLocal,ordinary,"otherLocal");object(viewport,ordinary,"viewport");
        world.AuthorityGameMode=&mode;world.OwningGameInstance=&game;locals[0]=&local;game.LocalPlayers={locals,1,1};
        local.ViewportClient=&viewport;local.PlayerController=&controller;viewport.World=&world;controller.Player=&local;
        refs[{&wrapper,"GardenDevilFreeCameraCollision"}]=&collision;refs[{&wrapper,"GardenDevilCameraCollision"}]=&cameraCollision;refs[{&wrapper,"GardenDevilCollision"}]=&ordinaryCollision;
        refs[{&wrapper,"DevilBaseInstance"}]=&devil;refs[{&wrapper,"BP Garden Manager"}]=&garden;refs[{&wrapper,"GardenDevilDitherComponent"}]=&dither;
        refs[{&dither,"GardenDevilRef"}]=&wrapper;refs[{&mode,"MapSubLevelManager"}]=&sublevels;
        refs[{&controller,"SpectatorPawn"}]=&pawn;refs[{&controller,"GardenFreeCameraPawnRef"}]=&pawn;refs[{&pawn,"Controller"}]=&controller;
        refs[{&garden,"BP_GardenFreeCameraManager"}]=&freeManager;refs[{&freeManager,"GardenFreeCameraControllerRef"}]=&controller;refs[{&controller,"BP Garden Free Camera Manager"}]=&freeManager;
        values[{&wrapper,"m_IsInitialiationDone"}]=values[{&garden,"GardenDevilsReady"}]=values[{&sublevels,"IsGardenLoaded"}]=values[{&sublevels,"IsEndLoadGarden"}]=1;
        object(begin,functionClass,GardenFreeCamera::BeginName);begin.Outer=&wrapperClass;begin.ChildProperties=properties.data();
        const char* names[]={"OverlappedComponent","OtherActor","OtherComp","OtherBodyIndex","bFromSweep","SweepResult"};
        const char* types[]={"ObjectProperty","ObjectProperty","ObjectProperty","IntProperty","BoolProperty","StructProperty"};
        int offsets[]={0,8,16,24,28,32},sizes[]={8,8,8,4,1,0x88};
        for(int i=0;i<6;++i){kinds[i].Name.value=types[i];auto& p=properties[i];p.ClassPrivate=&kinds[i];p.Name.value=names[i];p.Offset=offsets[i];p.ElementSize=sizes[i];p.Owner.Container.Object=&begin;p.Next=i<5?&properties[i+1]:nullptr;}
        parameters.fill(0xa5);Put(0,&collision);Put(8,&pawn);Put(16,&ordinaryCollision);
    }
    void Put(int offset,SDK::UObject* p){std::memcpy(parameters.data()+offset,&p,sizeof(p));}
    SDK::UObject* Get(int offset){SDK::UObject* p{};std::memcpy(&p,parameters.data()+offset,sizeof(p));return p;}
    bool Before(GardenFreeCamera::CallScope& scope){return GardenFreeCameraBefore(&wrapper,&begin,parameters.data(),scope);}
    void Native(){++nativeCalls;if(Get(8)&&HasClass(Get(8),"GardenFreeCameraPawn"))++ditherActivations;}
    bool Invoke(bool throws=false){GardenFreeCamera::CallScope scope{};const auto changed=Before(scope);try{Native();if(throws)throw std::runtime_error("native event exception");}catch(...){if(scope.active)GardenFreeCameraAfter(scope);throw;}if(scope.active)GardenFreeCameraAfter(scope);Require(!scope.active&&!scope.slot&&!scope.original,"no retained restore state");return changed;}
};
void Reset(){refs.clear();values.clear();unreadable.clear();threadId=1;sampleThread=1;insideProbe=true;requested=false;applied=false;}
void SuccessfulCall(){Scene s;auto original=s.parameters;Require(s.Invoke(),"ordinary overlap suppressed");Require(s.nativeCalls==1&&s.ditherActivations==0,"original event once, dither not enabled");Require(original==s.parameters,"all caller input bytes restored");Require(!s.actorHidden&&s.tickEnabled&&s.materialOpacity==1&&s.collisionProfile==77,"render/collision/tick state untouched");}
void F3Off(){Scene s;Require(!requested&&!applied&&s.Invoke(),"native freecam eligible with F3 off");}
void WrongCollision(){Scene s;s.Put(0,&s.ordinaryCollision);Require(!s.Invoke()&&s.ditherActivations==1,"interaction collision is native");}
void EndEvent(){Scene s;s.begin.name="BndEvt_EndOverlap";Require(!s.Invoke()&&s.ditherActivations==1,"other events run unchanged");}
void WrongWorld(){Scene s;s.pawn.world=&s.otherWorld;Require(!s.Invoke(),"cross-world pawn rejected");}
void WrongLocalController(){Scene s;s.controller.Player=&s.otherLocal;Require(!s.Invoke(),"non-local controller rejected");}
void SpectatorRequired(){Scene s;s.controller.Pawn=&s.pawn;refs[{&s.controller,"SpectatorPawn"}]=nullptr;Require(!s.Invoke(),"ordinary Pawn does not substitute for SpectatorPawn");}
void WrongManager(){Scene s;refs[{&s.freeManager,"GardenFreeCameraControllerRef"}]=nullptr;Require(!s.Invoke(),"manager/controller backlink required");}
void WrongDemonBinding(){Scene s;refs[{&s.dither,"GardenDevilRef"}]=&s.devil;Require(!s.Invoke(),"component wrapper binding required");}
void Loading(){Scene s;values[{&s.sublevels,"IsEndLoadGarden"}]=0;Require(!s.Invoke(),"incomplete garden loading rejected");}
void SignatureRejections(){Scene s;s.properties[1].Offset=16;Require(!s.Invoke(),"wrong OtherActor offset rejected");s.properties[1].Offset=8;s.properties[1].PropertyFlags|=0x100;Require(!s.Invoke(),"out OtherActor rejected");s.properties[1].PropertyFlags=0x80;s.begin.Size=0xa0;Require(!s.Invoke(),"unexpected parameter structure rejected");}
void ThreadBoundary(){Scene s;threadId=2;Require(!s.Invoke(),"other thread rejected");threadId=1;insideProbe=false;Require(!s.Invoke(),"outside guarded context rejected");}
void FinallyRestores(){Scene s;auto original=s.parameters;bool caught=false;try{s.Invoke(true);}catch(const std::runtime_error&){caught=true;}Require(caught&&s.parameters==original&&s.nativeCalls==1,"native exception propagates after input restore");}
void NestedScopes(){Scene a,b;GardenFreeCamera::CallScope first{},second{};auto original=a.parameters;Require(a.Before(first)&&b.Before(second),"nested events have separate stack scopes");GardenFreeCameraAfter(second);Require(a.Get(8)==nullptr&&b.Get(8)==&b.pawn,"nested restore does not restore outer early");GardenFreeCameraAfter(first);Require(a.parameters==original&&!first.active&&!second.active,"both nested input frames restored");}
void PawnRetirement(){Scene s;GardenFreeCamera::CallScope scope{};Require(s.Before(scope),"scope begins");s.pawn.live=false;GardenFreeCameraAfter(scope);Require(s.Get(8)==&s.pawn&&!scope.active,"input pointer restored without dereferencing retired pawn");}
void ColdStartOnly(){Scene s;s.currentFade=1;s.materialOpacity=0;Require(s.Invoke(),"new overlap suppressed");Require(s.currentFade==1&&s.materialOpacity==0,"does not pretend to repair pre-existing material fade");}

int main(){const std::pair<const char*,void(*)()> tests[]={{"event once and inputs restored",SuccessfulCall},{"F3 off native freecam",F3Off},{"wrong collision",WrongCollision},{"End event untouched",EndEvent},{"wrong world",WrongWorld},{"wrong local controller",WrongLocalController},{"SpectatorPawn binding",SpectatorRequired},{"wrong manager",WrongManager},{"wrong demon binding",WrongDemonBinding},{"garden loading",Loading},{"signature rejection",SignatureRejections},{"thread guard",ThreadBoundary},{"exception finally",FinallyRestores},{"nested scopes",NestedScopes},{"retired pawn input restore",PawnRetirement},{"cold start boundary",ColdStartOnly}};try{for(auto test:tests){Reset();test.second();std::cout<<"PASS "<<test.first<<'\n';}std::cout<<"PASS "<<std::size(tests)<<" production free-camera tests\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

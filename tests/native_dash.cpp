// Compile complete production dash_effect_visibility.inl with controlled
// identity/context/reflection/native-dispatch adapters; never copy its policy.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace SDK {
struct UWorld;
struct FName { std::string value; std::string GetRawString()const{return value;} std::string ToString()const{auto p=value.rfind('/');return p==std::string::npos?value:value.substr(p+1);} };
struct UObject { FName Name; std::string type; UObject* Outer{};UWorld* world{};int index{};bool retired{},unresolved{}; };
struct UWorld:UObject{};
struct USceneComponent:UObject { USceneComponent* AttachParent{};FName AttachSocketName; };
struct UParticleSystem:UObject{};
struct UParticleSystemComponent:USceneComponent { UParticleSystem* Template{};bool bOwnerNoSee{},bIsActive{true};float testParticleSimulation{7}; };
struct APlayerBase_C:UObject { USceneComponent* Mesh{};UObject* dash2{};bool purple{true},formReadable{true}; };
struct AProjectPlayerCameraManager_C:UObject { APlayerBase_C* pawn{};bool contextValid{true},bound{true};const char* block{}; };
struct UFunction;
struct FBoolProperty { struct { bool bIsUObject{true};struct{UObject* Object{};}Container; }Owner; };
struct UFunction:UObject { int Size{1};FBoolProperty* ChildProperties{}; };
}
using ULONGLONG=unsigned long long;
std::atomic<bool> requested{true};bool applied=true,restorationPending=false,insideProbe=true;
std::atomic<unsigned long> sampleThread{1};unsigned long currentThread=1;ULONGLONG now=10000;
unsigned long GetCurrentThreadId(){return currentThread;}ULONGLONG GetTickCount64(){return now;}
void Log(const char*,...){}
std::set<const void*> unreadable;
bool Readable(const void*p,std::size_t){return p&&!unreadable.count(p);}
bool IndexedObject(const SDK::UObject*p){return p&&!p->retired&&!p->unresolved&&Readable(p,sizeof(*p));}
std::string Name(const SDK::UObject*p){return p?p->Name.ToString():"none";}
std::string ClassName(const SDK::UObject*p){return p?p->type:"none";}
bool HasClass(const SDK::UObject*p,const char*kind){return p&&(p->type==kind||(p->type=="Pla603_C"&&std::string(kind)=="PlayerBase_C"));}
struct ObjectIdentity{SDK::UObject*pointer{};int index{};};
ObjectIdentity Identify(SDK::UObject*p){return IndexedObject(p)?ObjectIdentity{p,p->index}:ObjectIdentity{};}
bool Matches(const ObjectIdentity&i,const SDK::UObject*p){return i.pointer&&i.pointer==p&&IndexedObject(p)&&i.index==p->index;}
enum class ObjectState{Unresolved,Live,Retired};
ObjectState Resolve(const ObjectIdentity&i,SDK::UObject*&p){p=nullptr;if(!i.pointer)return ObjectState::Unresolved;if(i.pointer->retired||i.pointer->index!=i.index)return ObjectState::Retired;if(!IndexedObject(i.pointer))return ObjectState::Unresolved;p=i.pointer;return ObjectState::Live;}
struct CameraContext{SDK::AProjectPlayerCameraManager_C*manager{};SDK::APlayerBase_C*pawn{};SDK::UWorld*world{};};
bool Context(SDK::AProjectPlayerCameraManager_C*m,CameraContext&c){if(!m||!m->contextValid)return false;c={m,m->pawn,m->world};return true;}
bool BoundTo(const CameraContext&c){return c.manager->bound;}
const char*ViewBlock(const CameraContext&c){return c.manager->block;}
namespace GardenDiagnostics{
bool Scalar(SDK::APlayerBase_C*p,const char*key,double&out){if(std::string(key)!="bTsukuyomiForm"||!p->formReadable)return false;out=p->purple?1:0;return true;}
SDK::UObject*ObjectProperty(SDK::APlayerBase_C*p,const char*key){return std::string(key)=="DashEffect2"&&IndexedObject(p->dash2)?p->dash2:nullptr;}
}
SDK::UFunction nativeSetter;SDK::FBoolProperty nativeParameter;bool setterAvailable=true;
namespace MeshVisibility{
bool BelongsTo(SDK::UObject*p,SDK::APlayerBase_C*pawn,SDK::UWorld*world){return IndexedObject(p)&&p->Outer==pawn&&p->world==world;}
SDK::UFunction*NativeBoolFunction(SDK::UObject*,const char*fn,const char*param){return setterAvailable&&std::string(fn)=="SetOwnerNoSee"&&std::string(param)=="bNewOwnerNoSee"?&nativeSetter:nullptr;}
}
std::vector<std::pair<SDK::UParticleSystemComponent*,bool>> writes;
std::function<void(SDK::UParticleSystemComponent*,bool)> effect;
struct Dispatcher{template<class T>void unsafe_call(SDK::UParticleSystemComponent*p,SDK::UFunction*,void*args){bool hidden=*static_cast<bool*>(args);writes.emplace_back(p,hidden);if(effect)effect(p,hidden);else p->bOwnerNoSee=hidden;}}processEventHook;

#include "../mods/smtvv/common/dash_effect_visibility.inl"

void Require(bool ok,const char*text){if(!ok)throw std::runtime_error(text);}
int nextIndex=1;
void Register(SDK::UObject&o,const char*type,const char*name,SDK::UWorld&w,SDK::UObject*owner=nullptr){o.type=type;o.Name.value=name;o.index=nextIndex++;o.world=&w;o.Outer=owner;}
struct Scene{
    SDK::UWorld world;SDK::APlayerBase_C pawn;SDK::AProjectPlayerCameraManager_C manager;
    SDK::USceneComponent mesh,otherMesh;SDK::UObject package;SDK::UParticleSystem purple;
    SDK::UParticleSystemComponent dash,common,attack;
    Scene(){Register(world,"World","world",world);Register(pawn,"Pla603_C","player",world);Register(manager,"CameraManager","manager",world);manager.pawn=&pawn;
        Register(mesh,"SkeletalMeshComponent","mesh",world,&pawn);Register(otherMesh,"SkeletalMeshComponent","otherMesh",world);pawn.Mesh=&mesh;
        Register(package,"Package","/Game/Design/Effect/Particles/chara/PL/pl_603_11",world);Register(purple,"ParticleSystem","pl_603_11",world,&package);
        Register(dash,"ParticleSystemComponent","DashEffect2",world,&pawn);dash.Template=&purple;dash.AttachParent=&mesh;dash.AttachSocketName.value="Eff_102";pawn.dash2=&dash;
        Register(common,"ParticleSystemComponent","DashEffect",world,&pawn);Register(attack,"ParticleSystemComponent","AttackTrail",world,&pawn);
    }
    void Update(){CameraContext c{&manager,&pawn,&world};DashEffectUpdate(c);now+=100;}
};
void Reset(){DashEffectVisibility::entry={};DashEffectVisibility::restoring=false;DashEffectVisibility::lastStatus=0;DashEffectVisibility::previousStatus.clear();requested=true;applied=true;restorationPending=false;insideProbe=true;sampleThread=1;currentThread=1;now=10000;unreadable.clear();writes.clear();effect={};setterAvailable=true;nativeSetter={};nativeParameter={};nativeSetter.ChildProperties=&nativeParameter;nativeParameter.Owner.Container.Object=&nativeSetter;nextIndex=1;}
void PurpleOnly(){Scene s;s.Update();Require(s.dash.bOwnerNoSee&&writes.size()==1,"purple dash owner view hidden");Require(s.dash.bIsActive&&s.dash.testParticleSimulation==7,"native particle simulation and active state retained");Require(!s.common.bOwnerNoSee&&!s.attack.bOwnerNoSee,"common dash and attack effects unaffected");s.Update();Require(writes.size()==1,"no repeated write while already ours");Require(DashEffectRestore("test")&&!s.dash.bOwnerNoSee,"original owner flag restored");}
void BluePreserved(){Scene s;s.pawn.purple=false;s.Update();Require(writes.empty()&&!s.dash.bOwnerNoSee,"blue form untouched even if template temporarily purple");}
void ExactTemplateAttachment(){for(int n=0;n<7;++n){Reset();Scene s;switch(n){case 0:s.purple.Name.value="pl_603_10";break;case 1:s.package.Name.value="/Other/pl_603_11";break;case 2:s.dash.AttachSocketName.value="EFF_001";break;case 3:s.dash.AttachParent=&s.otherMesh;break;case 4:s.dash.Outer=&s.manager;break;case 5:s.purple.type="NiagaraSystem";break;case 6:s.pawn.type="Pla602_C";break;}s.Update();Require(writes.empty(),"different template/package/socket/owner/form never hidden");}}
void InactiveNeverClaimed(){Scene s;s.dash.bIsActive=false;s.Update();Require(writes.empty(),"inactive dash untouched");s.dash.bIsActive=true;s.Update();s.dash.bIsActive=false;s.Update();Require(!s.dash.bOwnerNoSee&&!DashEffectVisibility::entry.owned,"ending native dash restores owner flag");}
void NativeOwnerTrueUnowned(){Scene s;s.dash.bOwnerNoSee=true;s.Update();Require(writes.empty()&&!DashEffectVisibility::entry.owned,"native true not claimed");DashEffectRestore("test");Require(s.dash.bOwnerNoSee,"native true survives restore");}
void GuardExitRestores(){for(int n=0;n<7;++n){Reset();Scene s;s.Update();switch(n){case 0:requested=false;break;case 1:applied=false;break;case 2:restorationPending=true;break;case 3:s.manager.contextValid=false;break;case 4:s.manager.bound=false;break;case 5:s.manager.block="event";break;case 6:s.pawn.purple=false;break;}s.Update();Require(!s.dash.bOwnerNoSee&&!DashEffectVisibility::entry.owned,"F3/guard/blue change restores");}}
void TemplateChangedRestoresSameComponent(){Scene s;s.Update();s.purple.Name.value="pl_603_10";s.Update();Require(!s.dash.bOwnerNoSee,"template replacement restores inherited owner flag");}
void ReplacedReferenceOldRestoredFirst(){Scene s;s.Update();SDK::UParticleSystemComponent next=s.dash;next.index=nextIndex++;next.bOwnerNoSee=false;s.pawn.dash2=&next;effect=[&](SDK::UParticleSystemComponent*p,bool hidden){if(p==&next&&hidden)Require(!s.dash.bOwnerNoSee,"old component restored before new component write");p->bOwnerNoSee=hidden;};s.Update();Require(!s.dash.bOwnerNoSee&&next.bOwnerNoSee,"replaced reference transitions safely");}
void PendingBlocksNewComponent(){Scene s;s.Update();SDK::UParticleSystemComponent next=s.dash;next.index=nextIndex++;next.bOwnerNoSee=false;s.pawn.dash2=&next;s.dash.unresolved=true;s.Update();Require(DashEffectVisibility::restoring&&!next.bOwnerNoSee,"unresolved old snapshot blocks new component");s.dash.unresolved=false;s.Update();Require(!s.dash.bOwnerNoSee&&next.bOwnerNoSee,"retry restores then applies");}
void RetiredNoWrite(){Scene s;s.Update();s.dash.retired=true;auto n=writes.size();Require(DashEffectRestore("test")&&writes.size()==n,"destroyed effect retires without writes");}
void NativeFalseRetained(){Scene s;s.Update();s.dash.bOwnerNoSee=false;auto n=writes.size();Require(DashEffectRestore("test")&&writes.size()==n,"native newer false not overwritten");}
void WrongThreadNoDispatch(){Scene s;currentThread=2;s.Update();Require(writes.empty()&&!s.dash.bOwnerNoSee,"wrong thread cannot dispatch");}
void SetterSignatureRejected(){for(int n=0;n<3;++n){Reset();Scene s;switch(n){case 0:setterAvailable=false;break;case 1:nativeSetter.Size=2;break;case 2:nativeParameter.Owner.Container.Object=&s.world;break;}s.Update();Require(writes.empty()&&!s.dash.bOwnerNoSee,"native setter wrong signature rejected");}}
void PartialSetterRetainsSnapshot(){Scene s;effect=[](SDK::UParticleSystemComponent*p,bool hidden){p->bOwnerNoSee=hidden;if(hidden)p->unresolved=true;};s.Update();Require(DashEffectVisibility::restoring&&DashEffectVisibility::entry.owned,"partial native write retains recovery snapshot");s.dash.unresolved=false;effect={};Require(DashEffectRestore("test")&&!s.dash.bOwnerNoSee,"partial write restored on retry");}
int main(){const std::vector<std::pair<const char*,void(*)()>>cases{
    {"purple exact dash hidden only from owner",PurpleOnly},{"blue accepted form preserved",BluePreserved},{"seven template/attachment/form mismatches rejected",ExactTemplateAttachment},
    {"inactive and end of dash restore",InactiveNeverClaimed},{"native owner-hidden flag never claimed",NativeOwnerTrueUnowned},{"seven F3/context/form exits restore",GuardExitRestores},
    {"template replacement restores flag",TemplateChangedRestoresSameComponent},{"reference replacement restores old before new",ReplacedReferenceOldRestoredFirst},
    {"pending snapshot blocks new effect",PendingBlocksNewComponent},{"retired component never written",RetiredNoWrite},{"native newer false retained",NativeFalseRetained},
    {"wrong thread never dispatches",WrongThreadNoDispatch},{"setter metadata failures rejected",SetterSignatureRejected},{"partial native failure remains recoverable",PartialSetterRetainsSnapshot}};
    unsigned failures=0;for(auto[name,test]:cases){Reset();try{test();std::printf("PASS %s\n",name);}catch(const std::exception&e){++failures;std::printf("FAIL %s: %s\n",name,e.what());}}
    std::printf("%zu scenarios, %u failures\n",cases.size(),failures);return failures?1:0;}

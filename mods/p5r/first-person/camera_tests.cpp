// In-process synthetic fixtures; never opens a game process or touches saves.
#include "camera.cpp"
#include <stdexcept>
static void Require(bool v,const char* why) { if(!v) throw std::runtime_error(why); }
struct Fixture {
    alignas(16) unsigned char task[0xc0]{},work[0x380]{},camera[0x1e0]{},node[0x140]{},fieldTask[0xc0]{},field[0x300]{},pc[0x300]{};
    Fixture() {
        restore={}; auto& f=restore.frame;
        f.task=(Addr)task; f.work=(Addr)work; f.camera=(Addr)camera; f.node=(Addr)node; f.fieldTask=(Addr)fieldTask; f.field=(Addr)field; f.pc=(Addr)pc; f.uid=17;f.fieldUid=19;
        Write(f.task+0xb8,f.uid);Write(f.task+0x48,f.work);Write(f.work+0x50,f.camera);Write(f.work+0x58,f.node);Write(f.camera,5u);Write(f.node,3u);
        Write(f.fieldTask+0xb8,f.fieldUid);Write(f.fieldTask+0x48,f.field);Write(f.field+0x2a8,f.pc);Write(f.pc+0x2e8,f.node);
        f.view.m[0]=f.view.m[5]=f.view.m[10]=f.view.m[15]=1;
        f.view.m[12]=7;f.fovy=50;f.nearClip=10;
        restore.modified=EyeView(f.view,{1,2,3});restore.fovy=70;restore.nearClip=2;
        Write(f.camera+0x20,restore.modified);Write(f.camera+0x1a8,restore.fovy);Write(f.camera+0x1a0,restore.nearClip);
        restore.nodes[0]={f.node,0,0,.7f};restore.count=1;Write(f.node+0x108,0.f);restore.applied=true;
    }
};
int main() {
    try {
        Matrix v{};v.m[0]=v.m[5]=v.m[10]=v.m[15]=1;
        auto placed=EyeView(v,{12,165,-23});
        Require(placed.m[12]==-12 && placed.m[13]==-165 && placed.m[14]==23,"eye maps to view origin");
        Matrix rotated{};rotated.m[2]=-1;rotated.m[5]=1;rotated.m[8]=1;rotated.m[15]=1;
        auto turned=EyeView(rotated,{12,165,-23});
        for(int r=0;r<3;r++)Require(fabsf(turned.m[r]*12+turned.m[4+r]*165+turned.m[8+r]*-23+turned.m[12+r])<.001f,"rotated eye maps to origin");
        Require(memcmp(turned.m,rotated.m,12*sizeof(float))==0,"native orientation preserved");
        Require(fabsf(VerticalFov(90,16.f/9.f)-58.7155f)<.001f,"FOV converts degrees and aspect");
        // Recorded 23:02 PC scene layout: FMWK integer tag, sequence4,
        // mode0, fieldStatus23, flags1088, FOV50 and target==pc0Root.
        { Fixture f; alignas(16) unsigned char global[0x100]{},scene[0x40]{},seqTask[0xc0]{},seq[0x20]{};
          globalAddress=(Addr)global;Write(globalAddress+0x60,(Addr)scene);Write((Addr)scene+0x10,(Addr)f.camera);
          Write((Addr)f.field,0x464d574bu);Write((Addr)f.field+0x100,(Addr)f.task);Write((Addr)f.field+0x1e8,23);
          Write((Addr)seqTask+0x48,(Addr)seq);Write((Addr)seq+4,4);Write((Addr)f.work+0xc,1088u);
          Write((Addr)f.node+0x50,Vec3{-3450.2466f,140.43889f,3124.9883f});Write((Addr)f.camera+0x1ac,16.f/9.f);
          Tasks tasks{(Addr)f.task,(Addr)f.fieldTask,(Addr)seqTask};Frame frame{};
          Require(GetFrame(tasks,frame),"recorded native exploration passes validation");
          Write((Addr)f.work+0x268,1u);Require(!GetFrame(tasks,frame),"native locked camera remains excluded");
          globalAddress=0;
        }
        { Fixture f; Restore();Require(Get<float>((Addr)f.camera+0x1a8)==50,"FOV restored");Require(Get<float>((Addr)f.node+0x108)==.7f,"visibility restored to original value"); }
        { Fixture f;Write((Addr)f.work+0x58,(Addr)0x12345);Restore();Require(Get<float>((Addr)f.node+0x108)==.7f,"target switch does not strand hidden player"); }
        { Fixture f;Write((Addr)f.fieldTask+0xb8,uint64_t{20});Restore();Require(Get<float>((Addr)f.node+0x108)==0,"old player generation is never written"); }
        { Fixture f;auto newer=restore.modified;newer.m[12]=999;Write((Addr)f.camera+0x20,newer);Restore();Require(Get<Matrix>((Addr)f.camera+0x20).m[12]==999,"native newer camera wins"); }
        { Fixture f;Write((Addr)f.node+0x108,1.f);Restore();Require(Get<float>((Addr)f.node+0x108)==1,"native visibility change wins"); }
        { Fixture f;wanted=true;Write((Addr)f.task+0xb8,uint64_t{99});Restore();Require(wanted,"camera generation change retains user intent");wanted=false; }
        { Fixture f;Frame newCamera=restore.frame;lookReady=false;EnsureLook(newCamera);look.yaw=.83f;look.pitch=-.3f;
          newCamera.task+=0x1000;newCamera.uid++;EnsureLook(newCamera);
          Require(look.yaw==.83f && look.pitch==-.3f,"same field camera replacement retains heading");
          newCamera.fieldUid++;EnsureLook(newCamera);Require(fabsf(look.yaw-.83f)>.01f,"new field seeds native heading"); }
        { Vec3 output{},direction{};
          Require(RedirectMovement({3,0,4},0,1,0,output,direction),"forward input redirects");
          Require(fabsf(output.x)<.001f && fabsf(output.z-5)<.001f,"forward follows own look and preserves speed");
          Require(RedirectMovement({3,0,4},1,0,0,output,direction)&&fabsf(output.x+5)<.001f,"right follows own view right");
          Require(RedirectMovement({3,0,4},0,1,p5fp::Pi/2,output,direction)&&fabsf(output.x-5)<.001f,"turned heading follows yaw");
          Require(!RedirectMovement({3,2,4},0,1,0,output,direction),"special vertical input stays native");
          Require(!RedirectMovement({0,0,0},0,1,0,output,direction),"zero native movement is not manufactured");
          Require(!RedirectMovement({3,0,4},0,0,0,output,direction),"zero stick does not manufacture direction"); }
        { Fixture f;Restore();alignas(16) unsigned char global[0x100]{},scene[0x40]{},otherScene[0x40]{},seqTask[0xc0]{},seq[0x20]{},mesh[0x40]{};
          globalAddress=(Addr)global;Write(globalAddress+0x60,(Addr)scene);Write((Addr)scene+0x10,(Addr)f.camera);
          Write((Addr)f.field,0x464d574bu);Write((Addr)f.field+0x100,(Addr)f.task);Write((Addr)f.field+0x1e8,24);
          Write((Addr)seqTask+0x48,(Addr)seq);Write((Addr)seq+4,4);Write((Addr)f.work+0x26c,1u);
          Write((Addr)f.node+0x50,Vec3{10,20,30});Write((Addr)f.camera+0x1ac,16.f/9.f);
          Tasks tasks{(Addr)f.task,(Addr)f.fieldTask,(Addr)seqTask,true};Frame frame{},strict{};lookReady=false;
          Require(!GetFrame(tasks,strict),"mode1 retains phone/input-lock handoff");
          Require(GetRetainedFrame(tasks,frame),"mode2 accepts identity-verified phone state24");EnsureLook(frame);
          tasks.camera=0;Require(GetRetainedFrame(tasks,frame),"retained anchor survives exploration-camera removal");
          Write((Addr)otherScene+0x10,(Addr)f.camera);Write(globalAddress+0x60,(Addr)otherScene);
          Require(!GetRetainedFrame(tasks,frame),"same camera address in a new scene is not an anchor");Write(globalAddress+0x60,(Addr)scene);
          tasks.battle=true;Require(!GetRetainedFrame(tasks,frame),"battle task blocks mode2 even during field transition");tasks.battle=false;
          Write((Addr)seq+4,5);Require(!GetRetainedFrame(tasks,frame),"battle sequence always native");
          Write((Addr)seq+4,7);Require(GetRetainedFrame(tasks,frame),"event with retained live field anchor can retain FP");
          auto noField=tasks;noField.field=0;Require(!GetRetainedFrame(noField,frame),"cinematic without live player cannot reuse stale root");
          Write((Addr)seq+4,4);Require(GetRetainedFrame(tasks,frame),"phone returns to valid retained frame");
          Write((Addr)mesh,2u);Write((Addr)mesh+8,(Addr)f.node);Write((Addr)mesh+0x20,0x200u);Write((Addr)f.node+0x110,(Addr)mesh);
          float oldVis=Get<float>((Addr)f.node+0x108);Matrix oldView=Get<Matrix>((Addr)f.camera+0x20);
          renderScopeDepth=1;ApplyFrame(frame);
          Require(restore.meshCount==1 && (Get<uint32_t>((Addr)mesh+0x20)&0x100000u),"render scope hides only owned player mesh cache");
          Write((Addr)mesh+0x20,Get<uint32_t>((Addr)mesh+0x20)|0x400u);
          Restore();renderScopeDepth=0;
          Require(Get<uint32_t>((Addr)mesh+0x20)==0x600u && Get<float>((Addr)f.node+0x108)==oldVis,"render scope restores only owned visibility bit and retains native flag changes");
          Matrix restored=Get<Matrix>((Addr)f.camera+0x20);Require(memcmp(&oldView,&restored,sizeof(oldView))==0,"borrowed camera restored before scope ends");
          Write((Addr)mesh+0x20,0x100200u);renderScopeDepth=1;ApplyFrame(frame);Restore();renderScopeDepth=0;
          Require(Get<uint32_t>((Addr)mesh+0x20)==0x100200u,"originally hidden meshes remain hidden");
          renderScopeDepth=1;ApplyFrame(frame);renderScopeDepth=0;
          Require(!SameCameraOwner(),"scoped camera cannot be restored outside render lifetime");
          renderScopeDepth=1;Restore();renderScopeDepth=0;
          globalAddress=0;lookReady=false;wanted=false;
        }
        { Matrix parent{};parent.m[2]=-1;parent.m[5]=1;parent.m[8]=1;parent.m[15]=1;parent.m[12]=12;parent.m[13]=3;parent.m[14]=-8;
          Require(RigidTransform(parent),"camera parent is rigid");
          Matrix desired{};auto own=p5fp::AimView(p5fp::Look{.4f,-.3f,0,0},{10,165,20});memcpy(desired.m,own.data(),sizeof(desired));
          Matrix composite=Multiply(Multiply(desired,parent),RigidInverse(parent));
          for(int i=0;i<16;i++)Require(fabsf(composite.m[i]-desired.m[i])<.001f,"parent-local camera resolves to desired world view");
          parent.m[5]=2;Require(!RigidTransform(parent),"scaled parent rejected instead of guessed inverse");
        }
        puts("PASS: field/camera ownership, interaction guards and camera restoration");return 0;
    } catch(const std::exception& e) {fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}

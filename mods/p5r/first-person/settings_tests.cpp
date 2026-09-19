#include "camera.cpp"
#include <stdexcept>
static void Check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
int main() {
    try {
        { p5fp::Look a{},b{};
        p5fp::StepMouse(a,300,-100);
        for(int i=0;i<10;i++)p5fp::StepMouse(b,30,-10);
        Check(std::abs(a.yaw-b.yaw)<.00001f && std::abs(a.pitch-b.pitch)<.00001f,"mouse distance independent of event/frame splitting");
        Check(a.yaw<0 && a.pitch>0,"mouse right/up direction");
        p5fp::StepMouse(a,0,100000);Check(a.pitch==-p5fp::PitchLimit,"mouse pitch clamp");
        mouseX=40;mouseY=-90;mouseAllowed=true;ClearMouse();
        Check(mouseX==0 && mouseY==0 && !mouseAllowed,"handoff discards pending mouse motion"); }
        using namespace CameraTuning;
        auto d=Decode(Defaults);Check(d.eye==165 && d.fov==110 && d.gait==5,"P5R defaults");
        Check(Chord(0,false,false,false)==Action::Toggle && Chord(0,false,true,false)==Action::Reset,"F3 chords");
        Check(Chord(0,true,false,false)==Action::None && Chord(2,false,false,true)==Action::None,"ignore unintended chords");
        auto b=Defaults;
        for(int i=0;i<1000;i++)b=Adjust(b,0,false,true);
        Check(Decode(b).eye==220,"eye saturates, never wraps");
        for(int i=0;i<1000;i++)b=Adjust(b,0,true,true);
        Check(Decode(b).eye==100,"eye lower bound");
        Check(Decode(Adjust(Defaults,2,true,false)).gait==4.75f,"quarter cm decrement");
        Check(Decode(Adjust(Defaults,1,true,true)).fov==105,"coarse reverse FOV");
        Check(Decode(Encode(NAN,INFINITY,-1)).eye==165 && Decode(Encode(NAN,INFINITY,-1)).gait==0,"invalid values sanitized");
        wchar_t temp[MAX_PATH],folder[MAX_PATH];Check(GetTempPathW(MAX_PATH,temp)>0,"temp path");
        swprintf_s(folder,L"%sP5R-public-settings-%lu-%llu",temp,GetCurrentProcessId(),GetTickCount64());
        Check(CreateDirectoryW(folder,nullptr)!=0,"exclusive temp folder");
        swprintf_s(iniPath,L"%s\\test.ini",folder);
        LoadSettings();Check(settingsDirty,"missing config created");
        settingBits=Encode(173,103,2.75f);settingsChangedAt=GetTickCount64()-501;FlushSettings();
        settingBits=Defaults;LoadSettings();auto r=Decode(settingBits.load());
        Check(r.eye==173 && r.fov==103 && r.gait==2.75f && !settingsDirty,"saved settings survive reload");
        WritePrivateProfileStringW(L"Camera",L"FovDegrees",L"garbage",iniPath);
        LoadSettings();Check(Decode(settingBits.load()).fov==110,"bad config falls back");
        DeleteFileW(iniPath);RemoveDirectoryW(folder);
        puts("PASS: tuning bounds/chords, defaults, production INI roundtrip");return 0;
    } catch(const std::exception& e) {fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}

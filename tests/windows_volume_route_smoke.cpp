// SPDX-License-Identifier: GPL-3.0-only
// Opt-in independent-clone test. Adjusts only guest endpoint defaults/volume.
#include "windows_managed_route.h"
#include <windows.h>
#include <tlhelp32.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wrl/client.h>
#include <cmath>
#include <cstdio>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void check(HRESULT hr,const char* message){require(SUCCEEDED(hr),message);}
struct Volume {
 ComPtr<IAudioEndpointVolume> endpoint;float original=0;BOOL muted=FALSE;bool restore=true;
 explicit Volume(const std::wstring& id){
  ComPtr<IMMDeviceEnumerator> devices;ComPtr<IMMDevice> device;
  check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices)),"Enumerator failed");
  check(devices->GetDevice(id.c_str(),&device),"Endpoint failed");
  check(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(endpoint.GetAddressOf())),"Volume activation failed");
  check(endpoint->GetMasterVolumeLevelScalar(&original),"Read original volume failed");
  check(endpoint->GetMute(&muted),"Read original mute failed");
 }
 ~Volume(){if(!restore)return;endpoint->SetMasterVolumeLevelScalar(original,nullptr);endpoint->SetMute(muted,nullptr);}
 void set(float level,BOOL mute){check(endpoint->SetMasterVolumeLevelScalar(level,nullptr),"Set volume failed");check(endpoint->SetMute(mute,nullptr),"Set mute failed");}
 bool matches(float expected,BOOL mute){float level=0;BOOL actual=FALSE;check(endpoint->GetMasterVolumeLevelScalar(&level),"Read volume failed");check(endpoint->GetMute(&actual),"Read mute failed");return std::abs(level-expected)<0.005f&&actual==mute;}
 void expect(float expected,BOOL mute){require(matches(expected,mute),"Endpoint volume/mute mismatch");}
};
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 const bool read=app.arguments().size()==3&&app.arguments()[1]=="--read-volume";
 const bool set=app.arguments().size()==5&&app.arguments()[1]=="--set-volume";
 const bool hold=app.arguments().size()==5&&app.arguments()[1]=="--hold-volume";
 if(!hold&&!read&&!set&&(app.arguments().size()!=2||app.arguments()[1]!="--run")){
  std::cout<<"Use --run only on an independent Windows clone; changes guest routes/volume.\n";return 0;
 }
 HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 try{
  check(initialized,"COM failed");
  if(read||set){
   {
    Volume endpoint(app.arguments()[2].toStdWString());
    if(set){
     bool valid=false;const float level=app.arguments()[3].toFloat(&valid);
     const auto mute=app.arguments()[4];
     require(valid&&level>=0&&level<=1&&(mute=="0"||mute=="1"),"Invalid fixture volume/mute");
     endpoint.set(level,mute=="1"?TRUE:FALSE);
    }else std::cout<<endpoint.original<<" "<<(endpoint.muted?1:0)<<std::endl;
    endpoint.restore=false;
   }
   CoUninitialize();return 0;
  }
  if(hold){
   {
    soundcurrent::ManagedWindowsRoute route(false,app.arguments()[2].toStdWString(),app.arguments()[4].toStdWString(),true);
    soundcurrent::WindowsBridge bridge;
    require(bridge.start(app.arguments()[3].toStdWString(),app.arguments()[4].toStdWString(),false,false),"Child bridge did not initialize");
    std::cout<<"ready"<<std::endl;
    while(std::getchar()!=EOF){}
   }
   CoUninitialize();return 0;
  }
  {
  std::wstring render,capture,physical;
  for(auto& d:soundcurrent::windowsAudioEndpoints(false)){
   if(d.name.find(L"CABLE Input")!=std::wstring::npos&&render.empty())render=d.id;
   if(!d.virtualCable&&physical.empty())physical=d.id;
  }
  for(auto& d:soundcurrent::windowsAudioEndpoints(true))
   if(d.name.find(L"CABLE Output")!=std::wstring::npos&&capture.empty())capture=d.id;
  require(!render.empty()&&!capture.empty()&&!physical.empty(),"Need existing cable and physical fixture endpoints");
  Volume speakers(physical),source(render);
  speakers.set(0.71f,FALSE);source.set(0.35f,TRUE);
  auto route=std::make_unique<soundcurrent::ManagedWindowsRoute>(false,render,physical,true);
  source.expect(0.71f,FALSE);
  soundcurrent::WindowsBridge bridge;
  require(bridge.start(capture,physical,false,false),"Bridge did not initialize");
  speakers.expect(1.0f,FALSE);
  // Change system volume/mute on the owned endpoint while the bridge is active.
  source.set(0.53f,TRUE);
  bridge.stop();route.reset();
  speakers.expect(0.53f,TRUE);
  std::cout<<"PASS: 71% copied to owned endpoint, physical output unity while active, changed volume/mute restored on stop\n";
  std::array<std::wstring,3> expected;
  for(int role=0;role<3;++role){expected[role]=soundcurrent::windowsDefaultEndpointId(false,role);if(expected[role]==render)expected[role]=physical;}
  struct RouteCleanup {
   std::wstring render,physical;std::array<std::wstring,3> defaults;
   ~RouteCleanup(){soundcurrent::windowsRestoreOwnedRoute(false,render,physical,defaults,true);}
  } cleanup{render,physical,expected};
  speakers.set(0.71f,FALSE);source.set(0.35f,TRUE);
  QProcess child;
  child.start(QCoreApplication::applicationFilePath(),{"--hold-volume",QString::fromStdWString(render),QString::fromStdWString(capture),QString::fromStdWString(physical)});
  require(child.waitForStarted(5000)&&child.waitForReadyRead(10000)&&child.readAllStandardOutput().trimmed()=="ready","Volume owner handshake failed");
  source.expect(0.71f,FALSE);speakers.expect(1.0f,FALSE);
  source.set(0.42f,FALSE);
  child.kill();require(child.waitForFinished(10000),"Volume owner did not terminate");
  QElapsedTimer timer;timer.start();bool recovered=false;
  do{
   QCoreApplication::processEvents();
   bool defaultsMatch=true;
   for(int role=0;role<3;++role)if(soundcurrent::windowsDefaultEndpointId(false,role)!=expected[role])defaultsMatch=false;
   if(defaultsMatch&&speakers.matches(0.42f,FALSE)){recovered=true;break;}
   QThread::msleep(50);
  }while(timer.elapsed()<10000);
  require(recovered,"Guardian did not recover changed volume and all default roles after crash");
  std::cout<<"PASS: forced owner termination recovered changed volume/mute and all three output roles\n";
  for(const auto desired : {std::pair<float,BOOL>{0.46f,TRUE}, {1.0f,FALSE}}){
   speakers.set(0.71f,TRUE);source.set(0.35f,FALSE);
   bool stopped=false;
   auto guarded=std::make_unique<soundcurrent::ManagedWindowsRoute>(false,render,physical,true,[&]{bridge.stop();stopped=true;});
   require(bridge.start(capture,physical,false,false),"Guardian-fault bridge did not initialize");
   speakers.expect(1.0f,FALSE);
   source.set(desired.first,desired.second);
   HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
   require(snapshot!=INVALID_HANDLE_VALUE,"Cannot enumerate own guardian");
   DWORD pid=0;PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
   if(Process32FirstW(snapshot,&entry))do{
    if(entry.th32ParentProcessID==GetCurrentProcessId()&&_wcsicmp(entry.szExeFile,L"soundcurrent-route-guardian.exe")==0){pid=entry.th32ProcessID;break;}
   }while(Process32NextW(snapshot,&entry));
   CloseHandle(snapshot);require(pid!=0,"Own guardian missing");
   HANDLE process=OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,pid);
   require(process!=nullptr,"Cannot open own guardian");
   const bool terminated=TerminateProcess(process,99)!=FALSE;
   if(terminated)WaitForSingleObject(process,5000);
   CloseHandle(process);require(terminated,"Cannot terminate own guardian");
   timer.restart();recovered=false;
   do{
    QCoreApplication::processEvents();
    bool defaultsMatch=true;
    for(int role=0;role<3;++role)if(soundcurrent::windowsDefaultEndpointId(false,role)!=expected[role])defaultsMatch=false;
    if(stopped&&!bridge.running()&&!guarded->healthy()&&defaultsMatch&&speakers.matches(desired.first,desired.second)){recovered=true;break;}
    QThread::msleep(50);
   }while(timer.elapsed()<10000);
   require(recovered,"Guardian failure did not stop bridge and preserve latest volume/mute");
   guarded.reset();speakers.expect(desired.first,desired.second);
  }
  std::cout<<"PASS: guardian failure stopped live bridge and recovered muted and unity/unmuted volume changes while owner stayed alive\n";


  }
  CoUninitialize();return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';if(SUCCEEDED(initialized))CoUninitialize();return 1;}
}

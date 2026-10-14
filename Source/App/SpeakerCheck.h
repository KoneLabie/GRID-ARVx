#pragma once
#if JUCE_WINDOWS
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
namespace kone::grid {
inline juce::String speakerSignalCheck() {
  using Microsoft::WRL::ComPtr;
  juce::String report;
  const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  {
    auto processor = std::make_unique<Processor>();
    SystemInput capture(*processor);
    auto result = capture.open();
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> endpoint;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioRenderClient> render;
    HRESULT status = CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
    if(SUCCEEDED(status)) status=enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&endpoint);
    if(SUCCEEDED(status)) status=endpoint->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)client.GetAddressOf());
    WAVEFORMATEX format{};
    format.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;format.nChannels=2;format.nSamplesPerSec=48000;
    format.wBitsPerSample=32;format.nBlockAlign=8;format.nAvgBytesPerSec=48000*8;
    if(SUCCEEDED(status)) status=client->Initialize(AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,1000000,0,&format,nullptr);
    if(SUCCEEDED(status)) status=client->GetService(IID_PPV_ARGS(&render));
    UINT32 capacity=0;
    if(SUCCEEDED(status)) status=client->GetBufferSize(&capacity);
    if(SUCCEEDED(status)) status=client->Start();
    float peakRms=0,maximumWave=0;
    int generated=0;
    const auto start=juce::Time::getMillisecondCounter();
    if(result.wasOk() && SUCCEEDED(status)) while(juce::Time::getMillisecondCounter()-start<1100) {
      UINT32 padding=0;
      if(FAILED(client->GetCurrentPadding(&padding))) break;
      const auto frames=capacity-padding;
      BYTE* bytes=nullptr;
      if(frames && SUCCEEDED(render->GetBuffer(frames,&bytes))) {
        auto* output=reinterpret_cast<float*>(bytes);
        for(UINT32 i=0;i<frames;++i,++generated) {
          const float ramp=juce::jlimit(0.0f,1.0f,generated/2400.0f)*
            juce::jlimit(0.0f,1.0f,(48000-generated)/2400.0f);
          output[i*2]=ramp*.025f*std::sin(generated*juce::MathConstants<float>::twoPi*443/48000);
          output[i*2+1]=ramp*.025f*std::sin(generated*juce::MathConstants<float>::twoPi*659/48000);
        }
        render->ReleaseBuffer(frames,0);
      }
      juce::Thread::sleep(10);
      const auto audio=processor->analysis.consume(.01f);
      peakRms=juce::jmax(peakRms,audio.rms);
      for(auto value:audio.left)maximumWave=juce::jmax(maximumWave,std::abs(value));
      for(auto value:audio.right)maximumWave=juce::jmax(maximumWave,std::abs(value));
    }
    if(client && SUCCEEDED(status)) client->Stop();
    capture.close();
    const bool passed=result.wasOk() && SUCCEEDED(status) && capture.capturedFrames>1000 && peakRms>.0001f && maximumWave>.0001f;
    report=(passed?"PASS ":"FAIL ")+juce::String("Speaker playback reaches analyzer and waveform / ")+
      processor->getAssetProperty("audioCaptureDevice")+" / frames "+juce::String(capture.capturedFrames.load())+
      " / RMS "+juce::String(peakRms,6)+" / waveform "+juce::String(maximumWave,6)+
      " / "+result.getErrorMessage()+" / HRESULT "+juce::String::toHexString((int)status)+"\n";
  }
  if(SUCCEEDED(com))CoUninitialize();
  return report;
}
}
#endif

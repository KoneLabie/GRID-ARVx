#include "SystemInput.h"
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <audioclient.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <windows.h>
#include <wrl/client.h>
#endif
namespace kone::grid {
juce::Array<SystemInput::PlaybackDevice> SystemInput::playbackDevices() {
    juce::Array<PlaybackDevice> result;
#if JUCE_WINDOWS
    using Microsoft::WRL::ComPtr;
    const auto initializedCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initializedCom) && initializedCom != RPC_E_CHANGED_MODE) return result;
    {
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDeviceCollection> collection;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       IID_PPV_ARGS(&enumerator))) &&
            SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
            UINT count = 0; collection->GetCount(&count);
            for (UINT i = 0; i < count; ++i) {
                ComPtr<IMMDevice> endpoint; ComPtr<IPropertyStore> properties;
                LPWSTR id = nullptr;
                if (FAILED(collection->Item(i, &endpoint)) || FAILED(endpoint->GetId(&id))) continue;
                juce::String name;
                if (SUCCEEDED(endpoint->OpenPropertyStore(STGM_READ, &properties))) {
                    PROPVARIANT value; PropVariantInit(&value);
                    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR)
                        name = juce::String(value.pwszVal);
                    PropVariantClear(&value);
                }
                if (name.isEmpty()) name = "Playback device " + juce::String((int)i + 1);
                result.add({juce::String(id), name}); CoTaskMemFree(id);
            }
        }
    }
    if (SUCCEEDED(initializedCom)) CoUninitialize();
#endif
    return result;
}
juce::Result SystemInput::open(const juce::String &deviceId) {
#if JUCE_WINDOWS
    close();
    initialized.reset();
    capturedFrames = 0;
    processor.analysis.reset();
    {
        juce::ScopedLock l(statusLock);
        failure.clear();
        endpointId = deviceId;
    }
    startThread();
    if (!initialized.wait(3000)) {
        close();
        return juce::Result::fail("System audio initialization timed out.");
    }
    juce::ScopedLock l(statusLock);
    return failure.isEmpty() ? juce::Result::ok() : juce::Result::fail(failure);
#else
    return juce::Result::fail(
        "Select a system-audio monitor or loopback device in Audio/MIDI Settings on this OS.");
#endif
}
void SystemInput::run() {
#if JUCE_WINDOWS
    while (!threadShouldExit()) {
        if (!captureEndpoint()) break;
        wait(200);
    }
#endif
}
bool SystemInput::captureEndpoint() {
#if JUCE_WINDOWS
    using Microsoft::WRL::ComPtr;
    HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool started = false;
    auto fail = [&](const char *text, HRESULT code) {
        juce::ScopedLock l(statusLock);
        failure = juce::String(text) + " (0x" + juce::String::toHexString((int)code) + ")";
        processor.setAssetProperty("audioCaptureStatus", failure);
        initialized.signal();
    };
    {
        ComPtr<IMMDeviceEnumerator> devices;
        ComPtr<IMMDevice> endpoint;
        ComPtr<IAudioClient> client;
        ComPtr<IAudioCaptureClient> capture;
        HRESULT result =
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&devices));
        auto findEndpoint = [&] {
            return endpointId.isEmpty() ? devices->GetDefaultAudioEndpoint(eRender, eConsole, &endpoint)
                                       : devices->GetDevice(endpointId.toWideCharPointer(), &endpoint);
        };
        if (FAILED(result)) {
            fail("Cannot enumerate audio devices", result);
        } else if (FAILED(result = findEndpoint())) {
            fail("The selected speaker/headphone device is unavailable", result);
        } else if (FAILED(result = endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                                      (void **)client.GetAddressOf()))) {
            fail("Cannot open playback device", result);
        } else {
            WAVEFORMATEX *format = nullptr;
            result = client->GetMixFormat(&format);
            if (FAILED(result) || !format) {
                fail("Unsupported mix format", result);
            } else {
                // Ask the Windows shared mixer for stereo float output. This
                // also downmixes surround endpoints instead of discarding all
                // channels beyond the first pair.
                WAVEFORMATEX stereo{};
                stereo.wFormatTag=WAVE_FORMAT_IEEE_FLOAT; stereo.nChannels=2;
                stereo.nSamplesPerSec=format->nSamplesPerSec;
                stereo.wBitsPerSample=32; stereo.nBlockAlign=8;
                stereo.nAvgBytesPerSec=stereo.nSamplesPerSec*8;
                if (FAILED(result = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                    AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                    AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 1000000, 0, &stereo, nullptr))) {
                    fail("Cannot start shared speaker capture", result);
                } else if (FAILED(result = client->GetService(IID_PPV_ARGS(&capture)))) {
                    fail("Cannot access capture service", result);
                } else if (FAILED(result = client->Start())) {
                    fail("Cannot start system audio", result);
                } else {
                    processor.setInputSampleRate(format->nSamplesPerSec);
                    started = true;
                    ComPtr<IPropertyStore> properties;
                    if(SUCCEEDED(endpoint->OpenPropertyStore(STGM_READ,&properties))) {
                      PROPVARIANT value; PropVariantInit(&value);
                      if(SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName,&value)) && value.vt==VT_LPWSTR)
                        processor.setAssetProperty("audioCaptureDevice", juce::String(value.pwszVal));
                      PropVariantClear(&value);
                    }
                    processor.setAssetProperty("audioCaptureStatus", "Listening");
                    initialized.signal();
                    LPWSTR actualId=nullptr; endpoint->GetId(&actualId);
                    const juce::String currentId(actualId); CoTaskMemFree(actualId);
                    const auto nextCheckStart=juce::Time::getMillisecondCounter();
                    auto nextCheck=nextCheckStart;
                    std::vector<float> left(65536), right(65536);
                    while (!threadShouldExit()) {
                        if (midiPoll)
                            midiPoll(480);
                        if(endpointId.isEmpty() && juce::Time::getMillisecondCounter()-nextCheck >= 1000) {
                          nextCheck=juce::Time::getMillisecondCounter();
                          ComPtr<IMMDevice> currentDefault; LPWSTR id=nullptr;
                          if(SUCCEEDED(devices->GetDefaultAudioEndpoint(eRender,eConsole,&currentDefault)) &&
                             SUCCEEDED(currentDefault->GetId(&id))) {
                            const bool changed=juce::String(id)!=currentId;CoTaskMemFree(id);
                            if(changed) break;
                          }
                        }
                        UINT32 available = 0;
                        bool packetFailed = false;
                        if (FAILED(capture->GetNextPacketSize(&available)))
                            break;
                        while (available && !threadShouldExit()) {
                            BYTE *data = nullptr;
                            UINT32 samples = 0;
                            DWORD flags = 0;
                            if (FAILED(capture->GetBuffer(&data, &samples, &flags, nullptr, nullptr))) {
                                packetFailed = true; break;
                            }
                            auto count = std::min<UINT32>(samples, 65536);
                            for (UINT32 i = 0; i < count; ++i) {
                                float pair[2]{};
                                if(data && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) std::memcpy(pair,data+i*8,8);
                                left[i]=pair[0];right[i]=pair[1];
                            }
                            if (processor.value("muteInput") < 64)
                                processor.analyzeInput(left.data(), right.data(), (int)count);
                            capturedFrames.fetch_add(count, std::memory_order_relaxed);
                            capture->ReleaseBuffer(samples);
                            if (FAILED(capture->GetNextPacketSize(&available))) {
                                packetFailed = true; break;
                            }
                        }
                        if(packetFailed) break;
                        wait(10);
                    }
                    client->Stop();
                    if(!threadShouldExit()) processor.setAssetProperty("audioCaptureStatus", "Reconnecting speaker capture...");
                }
                CoTaskMemFree(format);
            }
        }
    }
    if (SUCCEEDED(init))
        CoUninitialize();
    return started && !threadShouldExit();
#else
    return false;
#endif
}
} // namespace kone::grid

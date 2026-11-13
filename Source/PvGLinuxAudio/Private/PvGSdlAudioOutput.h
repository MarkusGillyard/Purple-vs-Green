#pragma once

#include "AudioMixer.h"

THIRD_PARTY_INCLUDES_START
#include <SDL3/SDL.h>
THIRD_PARTY_INCLUDES_END

namespace PvGAudio
{
// An SDL request owns the handoff. Unreal also submits from the mixer
// thread during priming, when there is no SDL request to fill.
class FSdlAudioOutput final : public Audio::IAudioMixerPlatformInterface
{
public:
    FSdlAudioOutput();
    virtual ~FSdlAudioOutput() override;

    virtual FString GetPlatformApi() const override { return TEXT("SDL3"); }
    virtual bool InitializeHardware() override;
    virtual bool TeardownHardware() override;
    virtual bool IsInitialized() const override { return bInitialized; }
    virtual bool GetNumOutputDevices(uint32& OutNumOutputDevices) override;
    virtual bool GetOutputDeviceInfo(const uint32 InDeviceIndex, Audio::FAudioPlatformDeviceInfo& OutInfo) override;
    virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override;
    virtual bool OpenAudioStream(const Audio::FAudioMixerOpenStreamParams& Params) override;
    virtual bool CloseAudioStream() override;
    virtual bool StartAudioStream() override;
    virtual bool StopAudioStream() override;
    virtual Audio::FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override { return AudioStreamInfo.DeviceInfo; }
    virtual void SubmitBuffer(const uint8* Buffer) override;
    virtual FString GetDefaultDeviceName() override { return TEXT("Default SDL Audio Device"); }
    virtual FAudioPlatformSettings GetPlatformSettings() const override;
    virtual void SuspendContext() override;
    virtual void ResumeContext() override;

private:
    static void SDLCALL OnStreamNeedsData(void* UserData, SDL_AudioStream* InStream, int AdditionalAmount, int TotalAmount);
    void RenderRequest(SDL_AudioStream* InStream, int AdditionalAmount);
    static SDL_AudioDeviceID ResolveDevice(uint32 DeviceIndex);

    SDL_AudioStream* Stream = nullptr;
    // Written by the SDL device thread for one request at a time; sized when the stream opens.
    TArray<uint8> RequestBuffer;
    FCriticalSection SubmitLock;
    // The thread currently inside RenderRequest, 0 otherwise. Guarded by SubmitLock.
    uint32 RequestThreadId = 0;
    bool bInitialized = false;
    bool bSuspended = false;

};
}

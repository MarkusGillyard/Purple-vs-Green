#include "PvGSdlAudioOutput.h"

#include "AudioMixerTypes.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTLS.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogPvGAudio, Log, All);

namespace PvGAudio
{
namespace
{
// The mixer renders 5.1 float; SDL converts to the device's own layout and rate.
constexpr int32 OutputChannels = 6;
}

FSdlAudioOutput::FSdlAudioOutput() = default;

FSdlAudioOutput::~FSdlAudioOutput()
{
    if (bInitialized)
    {
        TeardownHardware();
    }
}

bool FSdlAudioOutput::InitializeHardware()
{
    if (bInitialized)
    {
        UE_LOG(LogPvGAudio, Error, TEXT("SDL audio is already initialized."));
        return false;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen")) && !SDL_getenv("SDL_AUDIODRIVER"))
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        UE_LOG(LogPvGAudio, Error, TEXT("SDL audio failed to start: %s"), UTF8_TO_TCHAR(SDL_GetError()));
        return false;
    }
    const char* Driver = SDL_GetCurrentAudioDriver();
    UE_LOG(LogPvGAudio, Display, TEXT("PvG SDL3 output, %s backend."), Driver ? UTF8_TO_TCHAR(Driver) : TEXT("no"));
    if (Audio::IAudioMixer::ShouldRecycleThreads())
    {
        // Pre-create the paused null-device thread, as the engine's own outputs do.
        CreateNullDeviceThread([] {}, 1.0f, true);
    }
    bInitialized = true;
    return true;
}

bool FSdlAudioOutput::TeardownHardware()
{
    if (!bInitialized)
    {
        return true;
    }
    StopAudioStream();
    CloseAudioStream();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    bInitialized = false;
    return true;
}

SDL_AudioDeviceID FSdlAudioOutput::ResolveDevice(uint32 DeviceIndex)
{
    if (DeviceIndex == static_cast<uint32>(AUDIO_MIXER_DEFAULT_DEVICE_INDEX))
    {
        return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    }
    int Count = 0;
    SDL_AudioDeviceID* Devices = SDL_GetAudioPlaybackDevices(&Count);
    const SDL_AudioDeviceID Id = (Devices && DeviceIndex < static_cast<uint32>(Count)) ? Devices[DeviceIndex] : 0;
    SDL_free(Devices);
    return Id;
}

bool FSdlAudioOutput::GetNumOutputDevices(uint32& OutNumOutputDevices)
{
    if (!bInitialized)
    {
        return false;
    }
    int Count = 0;
    SDL_free(SDL_GetAudioPlaybackDevices(&Count));
    OutNumOutputDevices = static_cast<uint32>(FMath::Max(Count, 0));
    return true;
}

bool FSdlAudioOutput::GetOutputDeviceInfo(const uint32 InDeviceIndex, Audio::FAudioPlatformDeviceInfo& OutInfo)
{
    if (!bInitialized)
        return false;
    const SDL_AudioDeviceID Device = ResolveDevice(InDeviceIndex);
    if (Device == 0)
        return false;
    const char* Name = Device ? SDL_GetAudioDeviceName(Device) : nullptr;
    const FString DeviceName = Name ? FString(UTF8_TO_TCHAR(Name)) : GetDefaultDeviceName();
    OutInfo.Reset();
    OutInfo.Name = DeviceName;
    OutInfo.DeviceId = DeviceName;
    OutInfo.SampleRate = GetPlatformSettings().SampleRate;
    OutInfo.Format = Audio::EAudioMixerStreamDataFormat::Float;
    OutInfo.NumChannels = FMath::Min(OutputChannels, AUDIO_MIXER_MAX_OUTPUT_CHANNELS);
    for (int32 i = 0; i < OutInfo.NumChannels; ++i)
    {
        OutInfo.OutputChannelArray.Add(static_cast<EAudioMixerChannel::Type>(i));
    }
    OutInfo.bIsSystemDefault = InDeviceIndex == static_cast<uint32>(AUDIO_MIXER_DEFAULT_DEVICE_INDEX);
    return true;
}

bool FSdlAudioOutput::GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const
{
    OutDefaultDeviceIndex = AUDIO_MIXER_DEFAULT_DEVICE_INDEX;
    return true;
}

bool FSdlAudioOutput::OpenAudioStream(const Audio::FAudioMixerOpenStreamParams& Params)
{
    if (!bInitialized || AudioStreamInfo.StreamState != Audio::EAudioOutputStreamState::Closed)
    {
        return false;
    }
    uint32 NumDevices = 0;
    if (!GetNumOutputDevices(NumDevices) || NumDevices == 0)
    {
        UE_LOG(LogPvGAudio, Warning, TEXT("No audio output devices."));
        return false;
    }
    OpenStreamParams = Params;
    AudioStreamInfo.Reset();
    AudioStreamInfo.OutputDeviceIndex = Params.OutputDeviceIndex;
    AudioStreamInfo.AudioMixer = Params.AudioMixer;
    AudioStreamInfo.NumOutputFrames = Params.NumFrames;
    AudioStreamInfo.NumBuffers = Params.NumBuffers;
    if (!GetOutputDeviceInfo(Params.OutputDeviceIndex, AudioStreamInfo.DeviceInfo))
    {
        return false;
    }

    const SDL_AudioDeviceID Device = ResolveDevice(Params.OutputDeviceIndex);
    if (Device == 0)
    {
        UE_LOG(LogPvGAudio, Error, TEXT("Audio device index %d is not available."), static_cast<int32>(Params.OutputDeviceIndex));
        return false;
    }
    SDL_AudioSpec Spec;
    Spec.format = SDL_AUDIO_F32LE;
    Spec.channels = AudioStreamInfo.DeviceInfo.NumChannels;
    Spec.freq = static_cast<int>(Params.SampleRate);
    RequestBuffer.SetNumZeroed(static_cast<int32>(Params.NumFrames) * Spec.channels * static_cast<int32>(sizeof(float)));

    UE_LOG(LogPvGAudio, Display, TEXT("Opening %s (device index %d), %d Hz, %d channels, %d frames per buffer."), *AudioStreamInfo.DeviceInfo.Name,
        static_cast<int32>(Params.OutputDeviceIndex), Spec.freq, Spec.channels, static_cast<int32>(Params.NumFrames));
    Stream = SDL_OpenAudioDeviceStream(Device, &Spec, &FSdlAudioOutput::OnStreamNeedsData, this);
    if (Stream == nullptr)
    {
        UE_LOG(LogPvGAudio, Error, TEXT("SDL could not open the audio device: %s"), UTF8_TO_TCHAR(SDL_GetError()));
        return false;
    }
    AudioStreamInfo.StreamState = Audio::EAudioOutputStreamState::Open;
    return true;
}

bool FSdlAudioOutput::CloseAudioStream()
{
    if (AudioStreamInfo.StreamState == Audio::EAudioOutputStreamState::Closed)
    {
        return false;
    }
    if (!StopAudioStream())
    {
        return false;
    }
    if (Stream != nullptr)
    {
        // Returns once SDL has stopped calling back for this stream.
        SDL_DestroyAudioStream(Stream);
        Stream = nullptr;
    }
    bSuspended = false;
    AudioStreamInfo.StreamState = Audio::EAudioOutputStreamState::Closed;
    return true;
}

bool FSdlAudioOutput::StartAudioStream()
{
    if (!bInitialized || (AudioStreamInfo.StreamState != Audio::EAudioOutputStreamState::Open && AudioStreamInfo.StreamState != Audio::EAudioOutputStreamState::Stopped))
    {
        return false;
    }
    if (!Stream)
        return false;
    BeginGeneratingAudio();
    if (!SDL_ResumeAudioStreamDevice(Stream))
    {
        StopGeneratingAudio();
        UE_LOG(LogPvGAudio, Error, TEXT("SDL could not resume the audio device: %s"), UTF8_TO_TCHAR(SDL_GetError()));
        return false;
    }
    AudioStreamInfo.StreamState = Audio::EAudioOutputStreamState::Running;
    return true;
}

bool FSdlAudioOutput::StopAudioStream()
{
    if (!bInitialized)
    {
        return false;
    }
    if (AudioStreamInfo.StreamState != Audio::EAudioOutputStreamState::Stopped && AudioStreamInfo.StreamState != Audio::EAudioOutputStreamState::Closed)
    {
        if (Stream != nullptr)
        {
            SDL_PauseAudioStreamDevice(Stream);
        }
        if (AudioStreamInfo.StreamState == Audio::EAudioOutputStreamState::Running)
        {
            StopGeneratingAudio();
        }
        if (Stream && !SDL_ClearAudioStream(Stream))
            UE_LOG(LogPvGAudio, Warning, TEXT("SDL could not clear the stopped stream: %s"), UTF8_TO_TCHAR(SDL_GetError()));
        bSuspended = false;
        AudioStreamInfo.StreamState = Audio::EAudioOutputStreamState::Stopped;
    }
    return true;
}

void SDLCALL FSdlAudioOutput::OnStreamNeedsData(void* UserData, SDL_AudioStream* InStream, int AdditionalAmount, int)
{
    if (AdditionalAmount > 0)
    {
        static_cast<FSdlAudioOutput*>(UserData)->RenderRequest(InStream, AdditionalAmount);
    }
}

void FSdlAudioOutput::RenderRequest(SDL_AudioStream* InStream, int AdditionalAmount)
{
    const int BlockBytes = RequestBuffer.Num();
    if (BlockBytes <= 0)
        return;
    const int Blocks = FMath::Min(16, FMath::DivideAndRoundUp(AdditionalAmount, BlockBytes));
    for (int Block = 0; Block < Blocks; ++Block)
    {
        // Silence unless the mixer submits from this request (it skips while stopping or swapping devices).
        FMemory::Memzero(RequestBuffer.GetData(), RequestBuffer.Num());
        if (bIsDeviceInitialized)
        {
            {
                FScopeLock Lock(&SubmitLock);
                RequestThreadId = FPlatformTLS::GetCurrentThreadId();
            }
            ReadNextBuffer();
            {
                FScopeLock Lock(&SubmitLock);
                RequestThreadId = 0;
            }
        }
        if (!SDL_PutAudioStreamData(InStream, RequestBuffer.GetData(), BlockBytes))
            break;
    }
}

void FSdlAudioOutput::SubmitBuffer(const uint8* Buffer)
{
    FScopeLock Lock(&SubmitLock);
    if (RequestThreadId != 0 && RequestThreadId == FPlatformTLS::GetCurrentThreadId())
    {
        FMemory::Memcpy(RequestBuffer.GetData(), Buffer, RequestBuffer.Num());
    }
}

FAudioPlatformSettings FSdlAudioOutput::GetPlatformSettings() const
{
    return FAudioPlatformSettings::GetPlatformSettings(FPlatformProperties::GetRuntimeSettingsClassName());
}

void FSdlAudioOutput::SuspendContext()
{
    if (!bSuspended && Stream != nullptr)
    {
        if (SDL_PauseAudioStreamDevice(Stream))
            bSuspended = true;
        else
            UE_LOG(LogPvGAudio, Warning, TEXT("SDL could not suspend audio: %s"), UTF8_TO_TCHAR(SDL_GetError()));
    }
}

void FSdlAudioOutput::ResumeContext()
{
    if (bSuspended && Stream != nullptr)
    {
        if (SDL_ResumeAudioStreamDevice(Stream))
            bSuspended = false;
        else
            UE_LOG(LogPvGAudio, Warning, TEXT("SDL could not resume audio: %s"), UTF8_TO_TCHAR(SDL_GetError()));
    }
}
}

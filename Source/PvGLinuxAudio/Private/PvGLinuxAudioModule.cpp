#include "AudioDevice.h"
#include "PvGSdlAudioOutput.h"

class FPvGLinuxAudioModule final : public IAudioDeviceModule
{
public:
    Audio::IAudioMixerPlatformInterface* CreateAudioMixerPlatformInterface() override
    {
        return new PvGAudio::FSdlAudioOutput();
    }
};

IMPLEMENT_MODULE(FPvGLinuxAudioModule, PvGLinuxAudio)

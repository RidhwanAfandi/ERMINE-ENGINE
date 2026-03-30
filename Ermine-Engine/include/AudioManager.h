/* Start Header ************************************************************************/
/*!
\file       AudioManager.h
\author     Hurng Kai Rui, h.kairui, 2301278, h.kairui\@digipen.edu
\date       15/9/2025
\brief      AudioManager is where the FMOD audio engine is wrapped.
			Provides a simple interface for loading, playing, and managing sounds.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "PreCompile.h"
#include "MathVector.h"
#include "fmod_studio.hpp"
#include "fmod.hpp"

using namespace std;

namespace Ermine {
    // Remove the separate Vector3 struct since we're using Vector3D from Math.h

    struct FadeOutInfo
    {
        FMOD::Channel* channel;
        int channelId;          // Track the channel ID for lookup
        float startVolume;
        float fadeDuration;
        float elapsedTime;
    };

    struct Implementation {
        Implementation();
        ~Implementation();
        void Update();
        FMOD::Studio::System* mpStudioSystem;
        FMOD::System* mpSystem;
        int mnNextChannelId;
        typedef map<string, FMOD::Sound*> SoundMap;
        typedef map<int, FMOD::Channel*> ChannelMap;
        typedef map<string, FMOD::Studio::EventInstance*> EventMap;
        typedef map<string, FMOD::Studio::Bank*> BankMap;
        BankMap mBanks;
        EventMap mEvents;
        SoundMap mSounds;
        ChannelMap mChannels;
        vector<FadeOutInfo> mFadeOutChannels; // Channels currently fading out
    };

    class CAudioEngine {
    public:
        // Core engine functions
        static void Init();
        static void Update();
        static void Shutdown();
        static int ErrorCheck(FMOD_RESULT result);
        // Sound loading and management
        static void LoadSound(const string& strSoundName, bool b3d = true, bool bLooping = false, bool bStream = false);
        static void UnLoadSound(const string& strSoundName);
        // Sound playback and control - now using Vector3D instead of Vector3
        static int PlaySounds(const string& strSoundName, const Vector3D& vPos = Vector3D{ 0, 0, 0 }, float fVolumedB = 0.0f);
        static void SetChannel3dPosition(int nChannelId, const Vector3D& vPosition);
        static void SetChannelVolume(int nChannelId, float fVolumedB);
        static void SetChannelPaused(int nChannelId, bool paused);
        static bool IsPlaying(int nChannelId);
        static void StopChannel(int nChannelId, bool fadeOut = false, float fadeDuration = 0.3f);
        static void StopAllChannels(bool fadeOut = false, float fadeDuration = 0.3f);
        static void PauseAllChannels();
        static void ResumeAllChannels();

        // FMOD Studio functions
        static void LoadBank(const std::string& strBankName, FMOD_STUDIO_LOAD_BANK_FLAGS flags);
        static void LoadEvent(const std::string& strEventName);
        static void PlayEvent(const string& strEventName);
        static void StopEvent(const string& strEventName, bool bImmediate = false);
        static bool IsEventPlaying(const string& strEventName);
        static void GetEventParameter(const string& strEventName, const string& strEventParameter, float* parameter);
        static void SetEventParameter(const string& strEventName, const string& strParameterName, float fValue);
        // 3D Audio listener - now using Vector3D
        static void Set3dListenerAndOrientation(const Vector3D& vPosition, const Vector3D& vLook, const Vector3D& vUp);

        static void SetListenerPosition(const Vector3D& position);
        static void SetListenerOrientation(const Vector3D& forward, const Vector3D& up);
        static void SetListenerAttributes(const Vector3D& position,
            const Vector3D& velocity,
            const Vector3D& forward,
            const Vector3D& up);

        static FMOD::System* GetCoreSystem();

        // Reverb DSP functions
        static FMOD::DSP* CreateReverbDSP();
        static void AddReverbToChannel(int nChannelId, FMOD::DSP* reverbDSP);
        static void RemoveReverbFromChannel(int nChannelId);
        static void SetReverbParameter(FMOD::DSP* reverbDSP, int index, float value);
        static void RemoveReverbDSP(FMOD::DSP* reverbDSP);

        // Utility functions
        static float dbToVolume(float dB);
        static float VolumeTodB(float volume);
        static FMOD_VECTOR VectorToFmod(const Vector3D& vPosition); // Updated parameter type
    };

    // Reverb presets for easy use
    struct ReverbPreset
    {
        float decayTime;      // 0.1f to 20.0f
        float earlyDelay;     // 0.001f to 0.3f
        float lateDelay;      // 0.001f to 0.1f
        float hfReference;    // 20.0f to 20000.0f
        float hfDecayRatio;   // 0.1f to 0.99f
        float diffusion;      // 0.0f to 100.0f
        float density;        // 0.0f to 100.0f
        float lowShelfFreq;   // 20.0f to 1000.0f
        float lowShelfGain;   // -18.0f to 18.0f
        float highCut;        // 20.0f to 20000.0f
        float earlyLateMix;   // 0.0f to 100.0f
        float wetLevel;       // -80.0f to 20.0f (dB)
        float dryLevel;       // -80.0f to 20.0f (dB)
    };

    // Common reverb presets
    namespace ReverbPresets {
        const ReverbPreset None = { 1.0f, 0.02f, 0.02f, 5000.0f, 0.5f, 100.0f, 100.0f, 250.0f, 0.0f, 20000.0f, 50.0f, -80.0f, 0.0f };
        const ReverbPreset SmallRoom = { 1.0f, 0.005f, 0.02f, 5000.0f, 0.7f, 90.0f, 80.0f, 250.0f, 0.0f, 4000.0f, 40.0f, -12.0f, 0.0f };
        const ReverbPreset MediumRoom = { 1.5f, 0.01f, 0.03f, 5000.0f, 0.65f, 85.0f, 75.0f, 250.0f, 0.0f, 3500.0f, 45.0f, -10.0f, 0.0f };
        const ReverbPreset LargeRoom = { 2.5f, 0.02f, 0.04f, 5000.0f, 0.6f, 80.0f, 70.0f, 250.0f, 0.0f, 3000.0f, 50.0f, -8.0f, 0.0f };
        const ReverbPreset SmallHall = { 3.0f, 0.03f, 0.05f, 5000.0f, 0.55f, 75.0f, 65.0f, 250.0f, 0.0f, 2500.0f, 55.0f, -6.0f, 0.0f };
        const ReverbPreset LargeHall = { 4.0f, 0.04f, 0.06f, 5000.0f, 0.5f, 70.0f, 60.0f, 250.0f, 0.0f, 2000.0f, 60.0f, -4.0f, 0.0f };
        const ReverbPreset Cathedral = { 6.0f, 0.05f, 0.08f, 5000.0f, 0.45f, 65.0f, 55.0f, 250.0f, 0.0f, 1500.0f, 70.0f, -2.0f, 0.0f };
        const ReverbPreset Cave = { 5.0f, 0.06f, 0.1f, 5000.0f, 0.4f, 60.0f, 50.0f, 250.0f, 0.0f, 1000.0f, 75.0f, 0.0f, 0.0f };
        const ReverbPreset Outdoor = { 0.5f, 0.001f, 0.01f, 5000.0f, 0.8f, 95.0f, 90.0f, 250.0f, 0.0f, 5000.0f, 20.0f, -20.0f, 0.0f };
    }
}
/* Start Header ************************************************************************/
/*!
\file       VideoManager.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       01/29/2026
\brief      Video playback system using pl_mpeg with a dedicated decode thread
            and audio-synced streaming playback.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#define PL_MPEG_IMPLEMENTATION
#include "VideoManager.h"

#include <glad/glad.h>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cmath>

#include "AssetManager.h"
#include "AudioManager.h"
#include "SettingsManager.h"
#include "fmod.h"
#include "fmod.hpp"

namespace Ermine
{
    /**
     * @brief Clear queued frame data and pending seek/skip flags.
     */
    void VideoManager::ResetStreamingState(VideoData& video)
    {
        video.currentFrame = {};
        video.hasCurrentFrame = false;
        video.nextFrame = {};
        video.hasNextFrame = false;
        video.pendingSkips = 0;
        video.pendingSeekToStart = false;
    }

    /**
     * @brief Initialize video rendering resources and spawn the decode thread.
     */
    void VideoManager::Init(int screenWidth, int screenHeight)
    {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        m_videoShader = AssetManager::GetInstance().LoadShader(
            "../Resources/Shaders/video_vertex.glsl",
            "../Resources/Shaders/video_fragment.glsl"
        );

        if (!m_videoShader || !m_videoShader->IsValid())
            m_videoShader = nullptr;

        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

        float vertices[] = {
            -1.0f, -1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 1.0f,
             1.0f,  1.0f, 1.0f, 0.0f,

            -1.0f, -1.0f, 0.0f, 1.0f,
             1.0f,  1.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f, 0.0f
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);

        m_decodeStop = false;
        m_decodeThread = std::thread(&VideoManager::DecodeThreadLoop, this);
    }

    /**
     * @brief Shutdown video playback and decoding resources.
     */
    void VideoManager::Shutdown()
    {
        m_decodeStop = true;
        m_decodeCv.notify_all();
        if (m_decodeThread.joinable())
            m_decodeThread.join();

        CleanupAllVideos();

        if (m_VBO)
        {
            glDeleteBuffers(1, &m_VBO);
            m_VBO = 0;
        }

        if (m_VAO)
        {
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
    }

    /**
     * @brief Update cached screen size for aspect-fit calculations.
     */
    void VideoManager::OnScreenResize(int width, int height)
    {
        m_screenWidth = width;
        m_screenHeight = height;
        m_quadDirty = true;
    }

    /**
     * @brief Open a video stream and initialize GPU textures for playback.
     */
    bool VideoManager::LoadVideo(const std::string& name, const std::string& filepath, bool loop)
    {
        if (name.empty() || filepath.empty())
            return false;

        if (VideoExists(name))
        {
            FreeVideo(name);
        }

        auto video = std::make_shared<VideoData>();
        video->filepath = filepath;
        video->loop = loop;

        video->plm = plm_create_with_filename(filepath.c_str());
        if (!video->plm)
            return false;

        if (!plm_probe(video->plm, 5000 * 1024))
        {
            plm_destroy(video->plm);
            video->plm = nullptr;
            return false;
        }

        const int frameRate = static_cast<int>(plm_get_framerate(video->plm));
        const double duration = plm_get_duration(video->plm);

        video->width = plm_get_width(video->plm);
        video->height = plm_get_height(video->plm);
        video->totalFrames = (frameRate > 0) ? static_cast<int>(duration * frameRate + 0.5) : 0;
        if (video->totalFrames <= 0)
            video->totalFrames = 1;

        video->frameDuration = (frameRate > 0) ? (1.0 / static_cast<double>(frameRate)) : (duration / video->totalFrames);
        video->videoClockSeconds = 0.0;

        if (!m_audioOnly)
        {
            if (!FinishLoadingVideo(*video))
            {
                ReleaseVideoResources(*video);
                return false;
            }
        }
        else
        {
            plm_set_video_enabled(video->plm, 0);
            video->loaded = true;
        }

        // Initialize audio playback (predecode to memory if present)
        video->hasAudioStream = (plm_get_num_audio_streams(video->plm) > 0);
        if (video->hasAudioStream)
        {
            plm_t* audioPlm = plm_create_with_filename(filepath.c_str());
            if (audioPlm && plm_probe(audioPlm, 5000 * 1024))
            {
                plm_set_video_enabled(audioPlm, 0);
                plm_set_audio_enabled(audioPlm, 1);

                video->audioSampleRate = plm_get_samplerate(audioPlm);
                if (video->audioSampleRate <= 0)
                    video->audioSampleRate = 44100;

                video->audioChannels = 2;
                if (audioPlm->audio_decoder && audioPlm->audio_decoder->mode == PLM_AUDIO_MODE_MONO)
                    video->audioChannels = 1;

                double audioDuration = plm_get_duration(audioPlm);
                if (audioDuration <= 0.0)
                    audioDuration = plm_get_duration(video->plm);
                if (audioDuration <= 0.0 && video->frameDuration > 0.0 && video->totalFrames > 0)
                    audioDuration = video->frameDuration * static_cast<double>(video->totalFrames);
                if (audioDuration <= 0.0)
                    audioDuration = 1.0;

                const uint64_t expectedSamples = static_cast<uint64_t>(audioDuration * static_cast<double>(video->audioSampleRate) * static_cast<double>(video->audioChannels));
                video->audioPcm.clear();
                video->audioPcm.reserve(static_cast<size_t>(expectedSamples));

                while (true)
                {
                    plm_samples_t* samples = plm_decode_audio(audioPlm);
                    if (!samples)
                        break;

                    if (video->audioChannels == 1)
                    {
                        const size_t count = static_cast<size_t>(samples->count);
                        video->audioPcm.reserve(video->audioPcm.size() + count);
                        for (size_t i = 0; i < count; ++i)
                            video->audioPcm.push_back(samples->interleaved[i * 2]);
                    }
                    else
                    {
                        const size_t count = static_cast<size_t>(samples->count) * 2;
                        video->audioPcm.insert(video->audioPcm.end(), samples->interleaved, samples->interleaved + count);
                    }
                }

                plm_destroy(audioPlm);
                audioPlm = nullptr;

                if (!video->audioPcm.empty())
                {
                    if (auto* core = CAudioEngine::GetCoreSystem())
                    {
                        FMOD_CREATESOUNDEXINFO exinfo{};
                        exinfo.cbsize = sizeof(exinfo);
                        exinfo.numchannels = video->audioChannels;
                        exinfo.defaultfrequency = video->audioSampleRate;
                        exinfo.format = FMOD_SOUND_FORMAT_PCMFLOAT;
                        exinfo.length = static_cast<unsigned int>(video->audioPcm.size() * sizeof(float));

                        const FMOD_MODE mode = static_cast<FMOD_MODE>(FMOD_OPENRAW | FMOD_OPENMEMORY | FMOD_CREATESAMPLE | (video->loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF));
                        FMOD::Sound* sound = nullptr;
                        if (core->createSound(reinterpret_cast<const char*>(video->audioPcm.data()), mode, &exinfo, &sound) == FMOD_OK && sound)
                        {
                            sound->setDefaults(video->audioSampleRate, 0);
                            if (video->loop && video->audioChannels > 0)
                            {
                                const unsigned int pcmSamples = static_cast<unsigned int>(video->audioPcm.size() / static_cast<size_t>(video->audioChannels));
                                sound->setLoopPoints(0, FMOD_TIMEUNIT_PCM, pcmSamples, FMOD_TIMEUNIT_PCM);
                            }
                            video->audioSound = sound;
                            video->audioEnabled = true;
                        }
                    }
                }
            }
            else if (audioPlm)
            {
                plm_destroy(audioPlm);
                audioPlm = nullptr;
            }
        }

        if (!video->audioEnabled)
        {
            video->hasAudioStream = false;
        }

        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            video->hasCurrentFrame = false;
            video->hasNextFrame = false;
            video->pendingSkips = 0;
            video->pendingSeekToStart = false;
            m_videos[name] = video;
        }


        m_decodeCv.notify_one();
        return true;
    }

    /**
     * @brief Set the active video by name for playback/rendering.
     */
    void VideoManager::SetCurrentVideo(const std::string& name)
    {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            auto it = m_videos.find(name);
            if (it == m_videos.end())
                return;

            if (!m_currentVideo.empty() && m_currentVideo != name)
            {
                auto prev = m_videos.find(m_currentVideo);
                if (prev != m_videos.end() && prev->second)
                {
                    prev->second->audioClockValid = false;
                    if (prev->second->audioChannel)
                    {
                        prev->second->audioChannel->stop();
                        prev->second->audioChannel = nullptr;
                    }
                }
            }

            m_currentVideo = name;
            m_quadDirty = true;
        }

        m_decodeCv.notify_one();
    }

    /**
     * @brief Begin playback for the current video.
     */
    void VideoManager::Play()
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_currentVideo.empty())
            return;

        auto it = m_videos.find(m_currentVideo);
        if (it == m_videos.end())
            return;

        if (!it->second->loaded)
            return;
        if (!it->second->hasCurrentFrame && !it->second->hasNextFrame)
            m_decodeCv.notify_one();

        if (it->second->audioEnabled && it->second->audioSound)
        {
            m_isPlaying = true;
            m_decodeCv.notify_one();

            if (auto* core = CAudioEngine::GetCoreSystem())
            {
                if (!it->second->audioChannel)
                {
                    core->playSound(it->second->audioSound, nullptr, false, &it->second->audioChannel);
                    if (it->second->audioChannel && it->second->audioSampleRate > 0)
                    {
                        it->second->audioChannel->setFrequency(static_cast<float>(it->second->audioSampleRate));
                    }
                    if (it->second->audioChannel)
                        it->second->audioChannel->setMode(it->second->loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
                    // Apply master volume from user settings on playback start
                    if (it->second->audioChannel)
                    {
                        float masterVol = SettingsManager::GetInstance().masterVolume;
                        it->second->audioChannel->setVolume(std::max(0.0f, std::min(1.0f, masterVol)));
                    }
                    if (it->second->audioChannel)
                    {
                        unsigned long long channelClock = 0;
                        unsigned long long parentClock = 0;
                        if (it->second->audioChannel->getDSPClock(&channelClock, &parentClock) == FMOD_OK)
                        {
                            it->second->audioStartClock = parentClock;
                            it->second->audioClockValid = true;
                        }
                    }
                }
                else
                {
                    bool paused = false;
                    it->second->audioChannel->getPaused(&paused);
                    if (paused)
                        it->second->audioChannel->setPaused(false);
                    unsigned long long dspClock = 0;
                    unsigned long long parentClock = 0;
                    if (it->second->audioChannel->getDSPClock(&dspClock, &parentClock) == FMOD_OK && it->second->audioSampleRate > 0)
                    {
                        unsigned int posPcm = 0;
                        if (it->second->audioChannel->getPosition(&posPcm, FMOD_TIMEUNIT_PCM) == FMOD_OK)
                        {
                            it->second->audioStartClock = parentClock - posPcm;
                            it->second->audioClockValid = true;
                        }
                    }
                }
            }
        }
        else
        {
            m_isPlaying = true;
            m_decodeCv.notify_one();
        }
    }

    /**
     * @brief Pause playback without resetting state.
     */
    void VideoManager::Pause()
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_isPlaying = false;

        if (!m_currentVideo.empty())
        {
            auto it = m_videos.find(m_currentVideo);
            if (it != m_videos.end() && it->second->audioChannel)
                it->second->audioChannel->setPaused(true);
        }
    }

    /**
     * @brief Stop playback and reset the playhead to the beginning.
     */
    void VideoManager::Stop()
    {
        std::shared_ptr<VideoData> video;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (m_currentVideo.empty())
                return;

            auto it = m_videos.find(m_currentVideo);
            if (it == m_videos.end())
                return;

            video = it->second;
            video->currentFrameIndex = -1;
            video->videoClockSeconds = 0.0;
            video->done = false;
            m_isPlaying = false;
        }

        if (video)
        {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                ResetStreamingState(*video);
                video->pendingSeekToStart = true;
            }

            {
                std::lock_guard<std::mutex> decodeLock(video->decodeMutex);
                if (video->plm)
                    plm_seek(video->plm, 0.0, 1);
            }

            if (video->audioChannel)
            {
                video->audioChannel->stop();
                video->audioChannel = nullptr;
            }

            video->audioClockValid = false;
        }
    }

    /**
     * @brief Advance playback timing and consume decoded frames.
     */
    void VideoManager::Update(float deltaTime)
    {
        if (m_audioOnly)
            return;
        std::shared_ptr<VideoData> current;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (m_currentVideo.empty())
                return;

            auto it = m_videos.find(m_currentVideo);
            if (it == m_videos.end())
                return;

            current = it->second;
        }

        if (!current || !current->loaded)
            return;

        UpdateAndAdvance(*current, deltaTime);
        m_decodeCv.notify_one();
    }

    /**
     * @brief Render the current decoded frame to screen.
     */
    void VideoManager::Render()
    {
        if (!m_renderEnabled || m_audioOnly)
            return;
        std::shared_ptr<VideoData> video;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (m_currentVideo.empty())
                return;

            auto it = m_videos.find(m_currentVideo);
            if (it == m_videos.end())
                return;

            video = it->second;
        }

        if (!video || !video->loaded)
            return;

        RenderVideoFrame(*video);
    }

    /**
     * @brief Check if the current video is playing.
     */
    bool VideoManager::IsVideoPlaying() const
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_currentVideo.empty())
            return false;

        auto it = m_videos.find(m_currentVideo);
        if (it == m_videos.end())
            return false;

        const auto& video = it->second;
        return m_isPlaying && !video->done && video->currentFrameIndex < video->totalFrames;
    }

    /**
     * @brief Check if a named video has completed playback.
     */
    bool VideoManager::IsVideoDonePlaying(const std::string& videoName) const
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto it = m_videos.find(videoName);
        if (it == m_videos.end())
            return false;
        return it->second->done;
    }

    /**
     * @brief Check if a named video exists.
     */
    bool VideoManager::VideoExists(const std::string& videoName) const
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        return m_videos.find(videoName) != m_videos.end();
    }

    /**
     * @brief Remove a single video and release its resources.
     */
    void VideoManager::FreeVideo(const std::string& name)
    {
        std::shared_ptr<VideoData> video;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            auto it = m_videos.find(name);
            if (it == m_videos.end())
                return;

            video = it->second;
            video->stopDecoding = true;
            m_videos.erase(it);

            if (m_currentVideo == name)
            {
                m_currentVideo.clear();
                m_isPlaying = false;
            }
        }

        m_decodeCv.notify_all();

        if (video)
        {
            std::lock_guard<std::mutex> decodeLock(video->decodeMutex);
            ReleaseVideoResources(*video);
        }
    }

    /**
     * @brief Release all videos and reset playback state.
     */
    void VideoManager::CleanupAllVideos()
    {
        std::vector<std::shared_ptr<VideoData>> videos;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            for (auto& entry : m_videos)
            {
                entry.second->stopDecoding = true;
                videos.push_back(entry.second);
            }
            m_videos.clear();
            m_currentVideo.clear();
            m_isPlaying = false;
            m_renderEnabled = true;
            m_fitMode = VideoFitMode::AspectFit;
            m_quadDirty = true;
        }

        m_decodeCv.notify_all();

        for (auto& video : videos)
        {
            if (!video)
                continue;
            std::lock_guard<std::mutex> decodeLock(video->decodeMutex);
            ReleaseVideoResources(*video);
        }
    }

    void VideoManager::SetAudioVolume(const std::string& name, float volume)
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto it = m_videos.find(name);
        if (it == m_videos.end() || !it->second)
            return;
        auto& video = it->second;
        if (video->audioChannel)
        {
            // Apply master volume from user settings so cutscene audio respects the player's volume preference
            float masterVol = SettingsManager::GetInstance().masterVolume;
            float finalVolume = std::max(0.0f, std::min(1.0f, volume * masterVol));
            video->audioChannel->setVolume(finalVolume);
        }
    }

#pragma region Private Methods

    /**
     * @brief Create GPU textures required for Y/Cb/Cr planes.
     */
    bool VideoManager::FinishLoadingVideo(VideoData& video)
    {
        if (video.loaded)
            return true;

        if (video.width == 0 || video.height == 0)
            return false;

        glGenTextures(1, &video.tex_y);
        glBindTexture(GL_TEXTURE_2D, video.tex_y);
        video.tex_y_width = video.width;
        video.tex_y_height = video.height;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(video.tex_y_width), static_cast<int>(video.tex_y_height), 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenTextures(1, &video.tex_cb);
        glBindTexture(GL_TEXTURE_2D, video.tex_cb);
        video.tex_cb_width = (video.width + 1) / 2;
        video.tex_cb_height = (video.height + 1) / 2;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(video.tex_cb_width), static_cast<int>(video.tex_cb_height), 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenTextures(1, &video.tex_cr);
        glBindTexture(GL_TEXTURE_2D, video.tex_cr);
        video.tex_cr_width = video.tex_cb_width;
        video.tex_cr_height = video.tex_cb_height;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(video.tex_cr_width), static_cast<int>(video.tex_cr_height), 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenBuffers(kPboCount, video.pbo_y);
        glGenBuffers(kPboCount, video.pbo_cb);
        glGenBuffers(kPboCount, video.pbo_cr);

        const size_t ySize = static_cast<size_t>(video.tex_y_width) * static_cast<size_t>(video.tex_y_height);
        const size_t cbSize = static_cast<size_t>(video.tex_cb_width) * static_cast<size_t>(video.tex_cb_height);
        const size_t crSize = static_cast<size_t>(video.tex_cr_width) * static_cast<size_t>(video.tex_cr_height);

        for (int i = 0; i < kPboCount; ++i)
        {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, video.pbo_y[i]);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, ySize, nullptr, GL_STREAM_DRAW);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, video.pbo_cb[i]);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, cbSize, nullptr, GL_STREAM_DRAW);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, video.pbo_cr[i]);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, crSize, nullptr, GL_STREAM_DRAW);
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        video.pboSizeY = ySize;
        video.pboSizeCb = cbSize;
        video.pboSizeCr = crSize;
        video.pboIndex = 0;

        video.loaded = true;
        video.done = false;
        video.currentFrameIndex = -1;
        video.videoClockSeconds = 0.0;
        ResetStreamingState(video);
        video.hasCurrentFrame = false;
        video.stopDecoding = false;
        m_quadDirty = true;

        return true;
    }

    /**
     * @brief Pop the next decoded frame when the frame duration elapses.
     */
    void VideoManager::UpdateAndAdvance(VideoData& video, float deltaTime)
    {
        if (!m_isPlaying || video.done)
            return;

        (void)deltaTime;
        int targetFrame = video.currentFrameIndex;
        bool audioWrapped = false;
        bool videoWrapped = false;

        if (video.audioEnabled && video.audioChannel && video.frameDuration > 0.0 && video.audioClockValid && video.audioSampleRate > 0)
        {
            unsigned long long dspClock = 0;
            unsigned long long parentClock = 0;
            if (video.audioChannel->getDSPClock(&dspClock, &parentClock) == FMOD_OK)
            {
                unsigned long long elapsed = 0;
                if (parentClock >= video.audioStartClock)
                    elapsed = parentClock - video.audioStartClock;

                double audioSeconds = static_cast<double>(elapsed) / static_cast<double>(video.audioSampleRate);
                const double durationSeconds = (video.totalFrames > 0) ? (video.frameDuration * static_cast<double>(video.totalFrames)) : 0.0;

                if (video.loop && durationSeconds > 0.0)
                {
                    const double wrapped = std::fmod(audioSeconds, durationSeconds);
                    if (wrapped + 1e-6 < video.videoClockSeconds)
                        audioWrapped = true;
                    audioSeconds = wrapped;
                }

                video.videoClockSeconds = audioSeconds;
                targetFrame = static_cast<int>(audioSeconds / video.frameDuration);
                if (video.totalFrames > 0 && targetFrame >= video.totalFrames)
                    targetFrame = video.totalFrames - 1;
            }
        }
        else if (!video.hasAudioStream || !video.audioEnabled)
        {
            video.videoClockSeconds += deltaTime;
            targetFrame = static_cast<int>(video.videoClockSeconds / video.frameDuration);
            if (video.loop && video.totalFrames > 0)
            {
                const int wrapped = targetFrame % video.totalFrames;
                if (wrapped < video.currentFrameIndex)
                    videoWrapped = true;
                targetFrame = wrapped;
            }
        }
        else
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (audioWrapped || videoWrapped)
            {
                video.pendingSeekToStart = true;
                video.pendingSkips = 0;
                video.nextFrame = {};
                video.hasNextFrame = false;
            }

            if (targetFrame > video.currentFrameIndex)
            {
                int desiredAdvance = targetFrame - video.currentFrameIndex;
                if (desiredAdvance > 1)
                    video.pendingSkips = desiredAdvance - 1;
            }
        }

        bool hasNextFrame = false;
        bool shouldPresent = false;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            hasNextFrame = video.hasNextFrame;
            shouldPresent = (targetFrame > video.currentFrameIndex);
        }

        if (hasNextFrame && shouldPresent)
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            video.currentFrame = std::move(video.nextFrame);
            video.hasCurrentFrame = true;
            video.hasNextFrame = false;
            if (video.currentFrameIndex < 0)
                video.currentFrameIndex = 0;
            else
                video.currentFrameIndex += 1;
        }

        m_decodeCv.notify_one();
    }

    /**
     * @brief Upload the current frame planes to GPU and draw the quad.
     */
    void VideoManager::RenderVideoFrame(VideoData& video)
    {
        if (!m_videoShader || !m_videoShader->IsValid())
            return;

        unsigned int frameWidth = 0;
        unsigned int frameHeight = 0;
        unsigned int cbWidth = 0;
        unsigned int cbHeight = 0;
        unsigned int crWidth = 0;
        unsigned int crHeight = 0;
        const uint8_t* yBuffer = nullptr;
        const uint8_t* cbBuffer = nullptr;
        const uint8_t* crBuffer = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (!video.hasCurrentFrame)
                return;
            frameWidth = video.currentFrame.width;
            frameHeight = video.currentFrame.height;
            cbWidth = video.currentFrame.cb_width;
            cbHeight = video.currentFrame.cb_height;
            crWidth = video.currentFrame.cr_width;
            crHeight = video.currentFrame.cr_height;
            yBuffer = video.currentFrame.y_buffer.get();
            cbBuffer = video.currentFrame.cb_buffer.get();
            crBuffer = video.currentFrame.cr_buffer.get();
        }
        if (!yBuffer || !cbBuffer || !crBuffer)
            return;

        UpdateQuadForVideo(video);

        glDisable(GL_DEPTH_TEST);

        m_videoShader->Bind();
        m_videoShader->SetUniform1i("tex_y", 0);
        m_videoShader->SetUniform1i("tex_cb", 1);
        m_videoShader->SetUniform1i("tex_cr", 2);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        auto EnsureTexture = [](GLuint texture, unsigned int& texWidth, unsigned int& texHeight, unsigned int desiredWidth, unsigned int desiredHeight)
        {
            if (desiredWidth == 0 || desiredHeight == 0)
                return;
            if (texWidth == desiredWidth && texHeight == desiredHeight)
                return;

            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<int>(desiredWidth), static_cast<int>(desiredHeight), 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            texWidth = desiredWidth;
            texHeight = desiredHeight;
        };

        auto UploadPlane = [](GLuint texture, GLuint pbo, size_t& pboSize, unsigned int width, unsigned int height, const uint8_t* src)
        {
            const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            if (pboSize < size)
            {
                glBufferData(GL_PIXEL_UNPACK_BUFFER, size, nullptr, GL_STREAM_DRAW);
                pboSize = size;
            }

            void* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            if (ptr)
            {
                memcpy(ptr, src, size);
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            }
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, nullptr);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        };

        const int pboIndex = video.pboIndex;

        glActiveTexture(GL_TEXTURE0);
        EnsureTexture(video.tex_y, video.tex_y_width, video.tex_y_height, frameWidth, frameHeight);
        UploadPlane(video.tex_y, video.pbo_y[pboIndex], video.pboSizeY, frameWidth, frameHeight, yBuffer);

        glActiveTexture(GL_TEXTURE1);
        EnsureTexture(video.tex_cb, video.tex_cb_width, video.tex_cb_height, cbWidth, cbHeight);
        UploadPlane(video.tex_cb, video.pbo_cb[pboIndex], video.pboSizeCb, cbWidth, cbHeight, cbBuffer);

        glActiveTexture(GL_TEXTURE2);
        EnsureTexture(video.tex_cr, video.tex_cr_width, video.tex_cr_height, crWidth, crHeight);
        UploadPlane(video.tex_cr, video.pbo_cr[pboIndex], video.pboSizeCr, crWidth, crHeight, crBuffer);

        video.pboIndex = (video.pboIndex + 1) % kPboCount;

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        m_videoShader->Unbind();

        glEnable(GL_DEPTH_TEST);
    }

    /**
     * @brief Update the full-screen quad based on fit mode and aspect ratio.
     */
    void VideoManager::UpdateQuadForVideo(const VideoData& video)
    {
        if (!m_quadDirty || m_screenWidth <= 0 || m_screenHeight <= 0)
            return;

        if (video.width == 0 || video.height == 0)
            return;

        const float screenAspect = static_cast<float>(m_screenWidth) / static_cast<float>(m_screenHeight);
        const float videoAspect = static_cast<float>(video.width) / static_cast<float>(video.height);

        float scaleX = 1.0f;
        float scaleY = 1.0f;

        if (m_fitMode == VideoFitMode::StretchToFill)
        {
            scaleX = 1.0f;
            scaleY = 1.0f;
        }
        else if (videoAspect > screenAspect)
        {
            scaleY = screenAspect / videoAspect;
        }
        else
        {
            scaleX = videoAspect / screenAspect;
        }

        const float left = -scaleX;
        const float right = scaleX;
        const float bottom = -scaleY;
        const float top = scaleY;

        float vertices[] = {
            left,  bottom, 0.0f, 1.0f,
            right, bottom, 1.0f, 1.0f,
            right, top,    1.0f, 0.0f,

            left,  bottom, 0.0f, 1.0f,
            right, top,    1.0f, 0.0f,
            left,  top,    0.0f, 0.0f
        };

        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        m_quadDirty = false;
    }

    /**
     * @brief Background decode loop that fills the frame queue.
     */
    void VideoManager::DecodeThreadLoop()
    {
        while (!m_decodeStop)
        {
            if (m_audioOnly)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            std::shared_ptr<VideoData> video;

            {
                std::unique_lock<std::mutex> lock(m_stateMutex);
                video.reset();
                m_decodeCv.wait(lock, [this, &video]()
                {
                    if (m_decodeStop)
                        return true;
                    if (!m_isPlaying || m_currentVideo.empty())
                        return false;
                    auto it = m_videos.find(m_currentVideo);
                    if (it == m_videos.end() || !it->second)
                        return false;
                    auto candidate = it->second;
                    if (!candidate->loaded || candidate->stopDecoding || candidate->done)
                        return false;
                    if (candidate->hasNextFrame && candidate->pendingSkips <= 0 && !candidate->pendingSeekToStart)
                        return false;
                    video = candidate;
                    return true;
                });
            }

            if (m_decodeStop)
                break;

            if (!video)
                continue;

            std::lock_guard<std::mutex> decodeLock(video->decodeMutex);
            if (video->stopDecoding || !video->plm)
                continue;

            bool seekToStart = false;
            int skipsToProcess = 0;
            {
                std::lock_guard<std::mutex> stateLock(m_stateMutex);
                seekToStart = video->pendingSeekToStart;
                skipsToProcess = video->pendingSkips;
            }

            if (seekToStart)
            {
                plm_seek(video->plm, 0.0, 1);
                std::lock_guard<std::mutex> stateLock(m_stateMutex);
                video->pendingSeekToStart = false;
                video->pendingSkips = 0;
                video->currentFrameIndex = -1;
            }

            while (skipsToProcess > 0)
            {
                plm_frame_t* skipped = plm_decode_video(video->plm);
                if (!skipped)
                    break;
                skipsToProcess--;
                std::lock_guard<std::mutex> stateLock(m_stateMutex);
                video->pendingSkips = skipsToProcess;
                video->currentFrameIndex += 1;
            }

            bool needVideoFrame = false;
            {
                std::lock_guard<std::mutex> stateLock(m_stateMutex);
                needVideoFrame = !video->hasNextFrame && !video->stopDecoding;
            }

            if (needVideoFrame)
            {
                plm_frame_t* frame = plm_decode_video(video->plm);
                if (!frame)
                {
                    if (video->loop)
                    {
                        std::lock_guard<std::mutex> stateLock(m_stateMutex);
                        video->pendingSeekToStart = true;
                    }
                    else
                    {
                        std::lock_guard<std::mutex> stateLock(m_stateMutex);
                        video->done = true;
                        m_isPlaying = false;
                    }
                }
                else
                {
                    VideoFrame decoded;
                    decoded.width = frame->y.width;
                    decoded.height = frame->y.height;
                    decoded.cb_width = frame->cb.width;
                    decoded.cb_height = frame->cb.height;
                    decoded.cr_width = frame->cr.width;
                    decoded.cr_height = frame->cr.height;
                    const size_t y_size = static_cast<size_t>(frame->y.width) * static_cast<size_t>(frame->y.height);
                    const size_t cr_size = static_cast<size_t>(frame->cr.width) * static_cast<size_t>(frame->cr.height);
                    const size_t cb_size = static_cast<size_t>(frame->cb.width) * static_cast<size_t>(frame->cb.height);

                    decoded.y_buffer = std::make_unique<uint8_t[]>(y_size);
                    decoded.cr_buffer = std::make_unique<uint8_t[]>(cr_size);
                    decoded.cb_buffer = std::make_unique<uint8_t[]>(cb_size);

                    memcpy(decoded.y_buffer.get(), frame->y.data, y_size);
                    memcpy(decoded.cr_buffer.get(), frame->cr.data, cr_size);
                    memcpy(decoded.cb_buffer.get(), frame->cb.data, cb_size);

                    std::lock_guard<std::mutex> stateLock(m_stateMutex);
            video->nextFrame = std::move(decoded);
            video->hasNextFrame = true;
        }
    }

        }
    }

    /**
     * @brief Release all GL and CPU resources for a video.
     */
    void VideoManager::ReleaseVideoResources(VideoData& video)
    {
        if (video.plm)
        {
            plm_destroy(video.plm);
            video.plm = nullptr;
        }

        if (video.audioChannel)
        {
            video.audioChannel->stop();
            video.audioChannel = nullptr;
        }

        if (video.audioSound)
        {
            video.audioSound->release();
            video.audioSound = nullptr;
        }
        video.audioPcm.clear();
        video.audioPcm.shrink_to_fit();

        if (video.tex_y)
        {
            glDeleteTextures(1, &video.tex_y);
            video.tex_y = 0;
        }
        video.tex_y_width = 0;
        video.tex_y_height = 0;

        if (video.tex_cb)
        {
            glDeleteTextures(1, &video.tex_cb);
            video.tex_cb = 0;
        }
        video.tex_cb_width = 0;
        video.tex_cb_height = 0;

        if (video.tex_cr)
        {
            glDeleteTextures(1, &video.tex_cr);
            video.tex_cr = 0;
        }
        video.tex_cr_width = 0;
        video.tex_cr_height = 0;

        if (video.pbo_y[0] || video.pbo_y[1] || video.pbo_y[2])
        {
            glDeleteBuffers(kPboCount, video.pbo_y);
            glDeleteBuffers(kPboCount, video.pbo_cb);
            glDeleteBuffers(kPboCount, video.pbo_cr);
            memset(video.pbo_y, 0, sizeof(video.pbo_y));
            memset(video.pbo_cb, 0, sizeof(video.pbo_cb));
            memset(video.pbo_cr, 0, sizeof(video.pbo_cr));
        }
        video.pboSizeY = 0;
        video.pboSizeCb = 0;
        video.pboSizeCr = 0;

        ResetStreamingState(video);
        video.stopDecoding = true;
        video.audioClockValid = false;
        video.audioEnabled = false;
        video.loaded = false;
        video.done = false;
    }
}

#pragma endregion

/* Start Header ************************************************************************/
/*!
\file       VideoManager.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       01/29/2026
\brief      Video playback system using pl_mpeg with a dedicated decode thread
            and audio-synced streaming playback.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "PreCompile.h"
#include "Systems.h"
#include "Shader.h"

#include "pl_mpeg/pl_mpeg.h"
#include "fmod.hpp"
#include <thread>
#include <cstring>
#include <vector>

namespace Ermine
{
    class VideoManager : public System
    {
    public:
        static constexpr int kPboCount = 3;

        enum class VideoFitMode
        {
            AspectFit = 0,
            StretchToFill = 1
        };
        struct VideoFrame
        {
            unsigned int width = 0;
            unsigned int height = 0;
            unsigned int cb_width = 0;
            unsigned int cb_height = 0;
            unsigned int cr_width = 0;
            unsigned int cr_height = 0;
            std::unique_ptr<uint8_t[]> y_buffer;
            std::unique_ptr<uint8_t[]> cr_buffer;
            std::unique_ptr<uint8_t[]> cb_buffer;
        };

        struct VideoData
        {
            plm_t* plm = nullptr;
            std::string filepath;

            GLuint tex_y = 0;
            GLuint tex_cb = 0;
            GLuint tex_cr = 0;
            unsigned int tex_y_width = 0;
            unsigned int tex_y_height = 0;
            unsigned int tex_cb_width = 0;
            unsigned int tex_cb_height = 0;
            unsigned int tex_cr_width = 0;
            unsigned int tex_cr_height = 0;
            GLuint pbo_y[kPboCount]{};
            GLuint pbo_cb[kPboCount]{};
            GLuint pbo_cr[kPboCount]{};
            size_t pboSizeY = 0;
            size_t pboSizeCb = 0;
            size_t pboSizeCr = 0;
            int pboIndex = 0;

            int currentFrameIndex = 0;
            int totalFrames = 0;
            unsigned int width = 0;
            unsigned int height = 0;

            double frameDuration = 0.0;
            double videoClockSeconds = 0.0;

            VideoFrame currentFrame;
            bool hasCurrentFrame = false;
            VideoFrame nextFrame;
            bool hasNextFrame = false;
            int pendingSkips = 0;
            bool pendingSeekToStart = false;

            bool done = false;
            bool loaded = false;
            bool loop = false;
            bool stopDecoding = false;

            std::mutex decodeMutex;

            // Audio playback (predecoded PCM in memory)
            FMOD::Sound* audioSound = nullptr;
            FMOD::Channel* audioChannel = nullptr;
            int audioSampleRate = 0;
            int audioChannels = 2;
            bool audioEnabled = false;
            bool hasAudioStream = false;
            unsigned long long audioStartClock = 0;
            bool audioClockValid = false;
            std::vector<float> audioPcm;

            VideoData() = default;
            VideoData(const VideoData&) = delete;
            VideoData& operator=(const VideoData&) = delete;

            VideoData(VideoData&& other) noexcept
                : plm(other.plm),
                  filepath(std::move(other.filepath)),
                  tex_y(other.tex_y),
                  tex_cb(other.tex_cb),
                  tex_cr(other.tex_cr),
                  tex_y_width(other.tex_y_width),
                  tex_y_height(other.tex_y_height),
                  tex_cb_width(other.tex_cb_width),
                  tex_cb_height(other.tex_cb_height),
                  tex_cr_width(other.tex_cr_width),
                  tex_cr_height(other.tex_cr_height),
                  pboSizeY(other.pboSizeY),
                  pboSizeCb(other.pboSizeCb),
                  pboSizeCr(other.pboSizeCr),
                  pboIndex(other.pboIndex),
                  currentFrameIndex(other.currentFrameIndex),
                  totalFrames(other.totalFrames),
                  width(other.width),
                  height(other.height),
                  frameDuration(other.frameDuration),
                  videoClockSeconds(other.videoClockSeconds),
                  currentFrame(std::move(other.currentFrame)),
                  hasCurrentFrame(other.hasCurrentFrame),
                  nextFrame(std::move(other.nextFrame)),
                  hasNextFrame(other.hasNextFrame),
                  pendingSkips(other.pendingSkips),
                  pendingSeekToStart(other.pendingSeekToStart),
                  done(other.done),
                  loaded(other.loaded),
                  loop(other.loop),
                  stopDecoding(other.stopDecoding),
                  audioSound(other.audioSound),
                  audioChannel(other.audioChannel),
                  audioSampleRate(other.audioSampleRate),
                  audioChannels(other.audioChannels),
                  audioEnabled(other.audioEnabled),
                  hasAudioStream(other.hasAudioStream),
                  audioStartClock(other.audioStartClock),
                  audioClockValid(other.audioClockValid),
                  audioPcm(std::move(other.audioPcm))
            {
                memcpy(pbo_y, other.pbo_y, sizeof(pbo_y));
                memcpy(pbo_cb, other.pbo_cb, sizeof(pbo_cb));
                memcpy(pbo_cr, other.pbo_cr, sizeof(pbo_cr));
                other.plm = nullptr;
                other.tex_y = 0;
                other.tex_cb = 0;
                other.tex_cr = 0;
                other.tex_y_width = 0;
                other.tex_y_height = 0;
                other.tex_cb_width = 0;
                other.tex_cb_height = 0;
                other.tex_cr_width = 0;
                other.tex_cr_height = 0;
                memset(other.pbo_y, 0, sizeof(other.pbo_y));
                memset(other.pbo_cb, 0, sizeof(other.pbo_cb));
                memset(other.pbo_cr, 0, sizeof(other.pbo_cr));
                other.pboSizeY = 0;
                other.pboSizeCb = 0;
                other.pboSizeCr = 0;
                other.pboIndex = 0;
                other.loaded = false;
                other.done = false;
                other.stopDecoding = true;
                other.audioSound = nullptr;
                other.audioChannel = nullptr;
                other.audioEnabled = false;
            }

            VideoData& operator=(VideoData&& other) noexcept
            {
                if (this != &other)
                {
                    plm = other.plm;
                    filepath = std::move(other.filepath);
                    tex_y = other.tex_y;
                    tex_cb = other.tex_cb;
                    tex_cr = other.tex_cr;
                    tex_y_width = other.tex_y_width;
                    tex_y_height = other.tex_y_height;
                    tex_cb_width = other.tex_cb_width;
                    tex_cb_height = other.tex_cb_height;
                    tex_cr_width = other.tex_cr_width;
                    tex_cr_height = other.tex_cr_height;
                    memcpy(pbo_y, other.pbo_y, sizeof(pbo_y));
                    memcpy(pbo_cb, other.pbo_cb, sizeof(pbo_cb));
                    memcpy(pbo_cr, other.pbo_cr, sizeof(pbo_cr));
                    pboSizeY = other.pboSizeY;
                    pboSizeCb = other.pboSizeCb;
                    pboSizeCr = other.pboSizeCr;
                    pboIndex = other.pboIndex;
                    currentFrameIndex = other.currentFrameIndex;
                    totalFrames = other.totalFrames;
                    width = other.width;
                    height = other.height;
                    frameDuration = other.frameDuration;
                    videoClockSeconds = other.videoClockSeconds;
                    currentFrame = std::move(other.currentFrame);
                    hasCurrentFrame = other.hasCurrentFrame;
                    nextFrame = std::move(other.nextFrame);
                    hasNextFrame = other.hasNextFrame;
                    pendingSkips = other.pendingSkips;
                    pendingSeekToStart = other.pendingSeekToStart;
                    done = other.done;
                    loaded = other.loaded;
                    loop = other.loop;
                    stopDecoding = other.stopDecoding;
                    audioSound = other.audioSound;
                    audioChannel = other.audioChannel;
                    audioSampleRate = other.audioSampleRate;
                    audioChannels = other.audioChannels;
                    audioEnabled = other.audioEnabled;
                    hasAudioStream = other.hasAudioStream;
                    audioStartClock = other.audioStartClock;
                    audioClockValid = other.audioClockValid;
                    audioPcm = std::move(other.audioPcm);

                    other.plm = nullptr;
                    other.tex_y = 0;
                    other.tex_cb = 0;
                    other.tex_cr = 0;
                    other.tex_y_width = 0;
                    other.tex_y_height = 0;
                    other.tex_cb_width = 0;
                    other.tex_cb_height = 0;
                    other.tex_cr_width = 0;
                    other.tex_cr_height = 0;
                    memset(other.pbo_y, 0, sizeof(other.pbo_y));
                    memset(other.pbo_cb, 0, sizeof(other.pbo_cb));
                    memset(other.pbo_cr, 0, sizeof(other.pbo_cr));
                    other.pboSizeY = 0;
                    other.pboSizeCb = 0;
                    other.pboSizeCr = 0;
                    other.pboIndex = 0;
                    other.loaded = false;
                    other.done = false;
                    other.stopDecoding = true;
                    other.audioSound = nullptr;
                    other.audioChannel = nullptr;
                    other.audioEnabled = false;
                }
                return *this;
            }
        };

        void Init(int screenWidth, int screenHeight);
        void Shutdown();
        void OnScreenResize(int width, int height);

        bool LoadVideo(const std::string& name, const std::string& filepath, bool loop);
        void SetCurrentVideo(const std::string& name);

        void Play();
        void Pause();
        void Stop();

        void Update(float deltaTime);
        void Render();

        bool IsVideoPlaying() const;
        bool IsVideoDonePlaying(const std::string& videoName) const;
        bool VideoExists(const std::string& videoName) const;

        void FreeVideo(const std::string& name);
        void CleanupAllVideos();
        void SetAudioVolume(const std::string& name, float volume);

        const std::unordered_map<std::string, std::shared_ptr<VideoData>>& GetVideos() const { return m_videos; }
        const std::string& GetCurrentVideo() const { return m_currentVideo; }
        bool IsRenderEnabled() const { return m_renderEnabled; }
        void SetRenderEnabled(bool enabled) { m_renderEnabled = enabled; }
        VideoFitMode GetFitMode() const { return m_fitMode; }
        void SetFitMode(VideoFitMode mode) { m_fitMode = mode; m_quadDirty = true; }

    private:
        bool FinishLoadingVideo(VideoData& video);
        void UpdateAndAdvance(VideoData& video, float deltaTime);
        void RenderVideoFrame(VideoData& video);
        void UpdateQuadForVideo(const VideoData& video);
        void ReleaseVideoResources(VideoData& video);
        void ResetStreamingState(VideoData& video);

        std::unordered_map<std::string, std::shared_ptr<VideoData>> m_videos;
        std::string m_currentVideo;
        bool m_isPlaying = false;
        bool m_renderEnabled = true;
        VideoFitMode m_fitMode = VideoFitMode::AspectFit;
        bool m_audioOnly = false;

        std::shared_ptr<graphics::Shader> m_videoShader;
        GLuint m_VAO = 0;
        GLuint m_VBO = 0;
        bool m_quadDirty = true;

        int m_screenWidth = 0;
        int m_screenHeight = 0;

        mutable std::mutex m_stateMutex;
        std::condition_variable m_decodeCv;
        std::thread m_decodeThread;
        std::atomic<bool> m_decodeStop{false};
        void DecodeThreadLoop();
    };
}

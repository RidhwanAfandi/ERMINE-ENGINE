/* Start Header ************************************************************************/
/*!
\file       ConsoleGUI.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       26/01/2026
\brief      This file contains the Unity-like Console window and log collector for the editor.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "PreCompile.h"
#include "imgui.h"
#include "ImGuiUIWindow.h"

namespace Ermine
{
    enum class ConsoleLogType : uint8_t
    {
        Info = 0,
        Warning = 1,
        Error = 2,
        Debug = 3
    };

    struct ConsoleLogEntry
    {
        ConsoleLogType type{ ConsoleLogType::Info };
        std::string    message;
        std::string    channel;   // Optional: subsystem/category
        std::string    details;   // Optional: stack trace or extra info
        uint64_t       frame{ 0 }; // Optional: frame when logged
        uint32_t       count{ 1 }; // For collapsed duplicates
        uint64_t       timestampMs{ 0 }; // Epoch ms
    };

    // Thread-safe log collector (singleton)
    class EditorConsole
    {
    public:
        static EditorConsole& GetInstance();

        void SetCapacity(size_t cap);
        size_t Capacity() const { return m_capacity; }
        void Clear();

        // Logging APIs
        void AddLog(ConsoleLogType type, std::string_view message,
            std::string_view channel = {}, std::string_view details = {},
            uint64_t frame = 0);

        void AddLogFmt(ConsoleLogType type, const char* fmt, ...);

        // Snapshot for UI (avoids drawing under a lock)
        void Snapshot(std::vector<ConsoleLogEntry>& out) const;

        // Incrementing version that changes when logs mutate
        uint64_t Version() const { return m_version.load(std::memory_order_relaxed); }

        // Stats
        std::array<uint32_t, 4> Counts() const;

    private:
        EditorConsole() = default;
        EditorConsole(const EditorConsole&) = delete;
        EditorConsole& operator=(const EditorConsole&) = delete;

        void addLogUnlocked(ConsoleLogEntry&& e);

        mutable std::mutex m_mutex;
        std::deque<ConsoleLogEntry> m_logs;
        size_t m_capacity{ 10000 };
        std::array<uint32_t, 4> m_counts{ 0,0,0,0 };
        std::atomic<uint64_t> m_version{0};
    };

    // Unity-like Console window
    class ConsoleGUI : public ImGUIWindow
    {
    public:
        ConsoleGUI();
        void Update() override;
		void Render() override;

        // Optional: call to bring focus to the search box
        void FocusSearch() { m_focusSearch = true; }

    private:
        // UI state
        bool m_showInfo{ true };
        bool m_showWarning{ true };
        bool m_showError{ true };
        bool m_showDebug{ false };
        bool m_collapse{ true };
        bool m_autoScroll{ true };
        bool m_pause{ false };
        bool m_focusSearch{ false };

        ImGuiTextFilter m_filter;
        int m_selectedIndex{ -1 };

        // Cached snapshot for drawing
        std::vector<ConsoleLogEntry> m_snapshot;

        // Cache of display list to avoid rebuilds when nothing changed
        std::vector<int> m_cachedDisplayIndices;
        std::vector<uint32_t> m_cachedCollapsedCounts;
        uint64_t m_lastSnapshotVersion{0};
        // Track toggles/filters used to build the cache
        bool m_cachedShowInfo{ true };
        bool m_cachedShowWarning{ true };
        bool m_cachedShowError{ true };
        bool m_cachedShowDebug{ false };
        bool m_cachedCollapse{ true };
        // Store filter text to detect changes
        std::string m_cachedFilterText;

        // Build filtered (and optionally collapsed) view indices
        void buildDisplayList(std::vector<int>& outDisplayIndices,
            std::vector<uint32_t>& outCollapsedCounts) const;

        static ImU32 colorFor(ConsoleLogType t);
        static const char* iconFor(ConsoleLogType t);
        static void copyToClipboard(const std::vector<ConsoleLogEntry>& src,
            const std::vector<int>& indices,
            const std::vector<uint32_t>* collapsedCounts,
            bool onlySelected, int selectedIndex);
    };

}

#if defined(ERMINE_USE_SPDLOG)
#include <spdlog/sinks/base_sink.h>

namespace Ermine
{
    class ImGuiConsoleSink_mt final : public spdlog::sinks::base_sink<std::mutex>
    {
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override;
        void flush_() override {}
    };
}
#endif

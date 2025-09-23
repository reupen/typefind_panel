#pragma once

namespace typefind_panel {

enum class SearchMode : uint32_t {
    mode_match_beginning_formatted_title = 0,
    mode_query = 1,
    mode_match_words_beginning_formatted_title = 2,
};

class ProgressiveSearch {
public:
    void init()
    {
        if (!m_running) {
            m_active = m_playlist_api->get_active_playlist();
            const auto count = m_playlist_api->playlist_get_item_count(m_active);
            m_entries.prealloc(count);
            m_playlist_api->playlist_get_all_items(m_active, m_entries);
            m_filter.set_size(count);
            m_filter.fill(true);
            m_buffer.prealloc(256);

            if (m_mode == SearchMode::mode_match_beginning_formatted_title
                || m_mode == SearchMode::mode_match_words_beginning_formatted_title) {
                m_formatted.resize(count);

                for (const auto n : std::ranges::views::iota(size_t{}, count)) {
                    m_entries[n]->format_title(nullptr, m_buffer, m_to, nullptr);
                    m_formatted[n] = mmh::to_utf16(m_buffer.c_str());
                }
            }
        }
        m_running = true;
    }

    void reset()
    {
        m_running = false;
        m_filter.set_size(0);
        m_entries.remove_all();
        m_string.clear();
        m_buffer.clear();
        m_formatted.clear();
    }

    void add_char(unsigned c)
    {
        init();
        m_string.push_back(c);
        run();
    }

    void set_string(std::wstring text)
    {
        if (m_running)
            m_filter.fill(true);
        init();
        m_string = std::move(text);
        run();
    }

    void set_pattern(const char* src, bool ignore_symbols)
    {
        m_ignore_symbols = ignore_symbols;
        titleformat_compiler::get()->compile(m_to, src);
    }

    void set_mode(SearchMode mode) { m_mode = mode; }

    bool on_key(WPARAM wp)
    {
        switch (wp) {
        case VK_RETURN: {
            if (!m_running)
                return true;

            bool ctrl_down = 0 != (GetKeyState(VK_CONTROL) & KF_UP);
            if (ctrl_down) {
                metadb_handle_ptr mh;
                const size_t focus = m_playlist_api->playlist_get_focus_item(m_active);
                if (focus != std::numeric_limits<size_t>::max())
                    m_playlist_api->queue_add_item_playlist(m_active, focus);
            } else {
                m_playlist_api->set_playing_playlist(m_active);
                const auto play_api = playback_control::get();
                play_api->play_start(playback_control::track_command_settrack);
            }
            return true;
        }
        case VK_DOWN: {
            if (!m_running)
                return true;

            const auto focus = m_playlist_api->playlist_get_focus_item(m_active);

            if (focus == std::numeric_limits<size_t>::max() || focus + 1 >= m_filter.get_count())
                return true;

            for (const auto n : std::ranges::views::iota(focus + 1, m_filter.get_size())) {
                if (m_filter[n]) {
                    m_playlist_api->playlist_set_selection(m_active, bit_array_true(), bit_array_one(n));
                    m_playlist_api->playlist_set_focus_item(m_active, n);
                    // api->playlist_set_focus_item(active, n);
                    break;
                }
            }
            return true;
        }
        case VK_UP: {
            if (!m_running)
                return true;

            const auto focus = m_playlist_api->playlist_get_focus_item(m_active);

            if (focus == 0 || focus >= m_filter.get_count())
                return true;

            for (const auto n : ranges::views::iota(size_t{}, focus) | ranges::views::reverse)
                if (m_filter[n]) {
                    m_playlist_api->playlist_set_selection(m_active, bit_array_true(), bit_array_one(n));
                    m_playlist_api->playlist_set_focus_item(m_active, n);
                    break;
                }
        }
            return true;
        }
        return false;
    }
    ProgressiveSearch() : m_running(false), m_active(0), m_mode(SearchMode::mode_match_beginning_formatted_title) {}

private:
    void run();

    bool m_running{};
    bool m_ignore_symbols{};
    metadb_handle_list_t<pfc::alloc_fast_aggressive> m_entries;
    service_ptr_t<titleformat_object> m_to;
    pfc::array_t<bool> m_filter;
    std::wstring m_string;
    pfc::string8_fastalloc m_buffer;
    static_api_ptr_t<playlist_manager> m_playlist_api;
    size_t m_active{};
    SearchMode m_mode{};
    std::vector<std::wstring> m_formatted;
};

} // namespace typefind_panel

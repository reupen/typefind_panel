
#pragma once

#include <ranges>

namespace typefind_panel {

enum t_search_mode {
    mode_pattern_beginning = 0,
    mode_query = 1,
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
#ifdef SEARCH_CACHING_ENABLED
            m_formatted.set_size(count);
            if (m_mode == mode_pattern_beginning) {
                for (const auto n : std::ranges::views::iota(size_t{}, count)) {
                    m_entries[n]->format_title(nullptr, m_buffer, m_to, nullptr);
                    m_formatted[n] = m_buffer;
                }
            }
#endif
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
#ifdef SEARCH_CACHING_ENABLED
        m_formatted.set_size(0);
#endif
    }

    void add_char(unsigned c)
    {
        init();
        m_string.add_char(c);
        run();
    }

    void set_string(const char* src)
    {
        if (m_running)
            m_filter.fill(true);
        init();
        m_string.set_string(src);
        run();
    }

    void set_pattern(const char* src) { titleformat_compiler::get()->compile(m_to, src); }

    void set_mode(t_uint32 mode) { m_mode = mode; }

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
    ProgressiveSearch() : m_running(false), m_active(0), m_mode(mode_pattern_beginning) {}

private:
    void run()
    {
        bool b_clear = m_mode == mode_query;
        if (m_mode == mode_query && m_string.get_length()) {
            static_api_ptr_t<search_filter_manager_v2> api;
            search_filter_v2::ptr p_filter;
            try {
                p_filter = api->create_ex(m_string, new service_impl_t<completion_notify_dummy>(),
                    search_filter_manager_v2::KFlagSuppressNotify);
                b_clear = false;
            } catch (const pfc::exception&) {
            }

            if (p_filter.is_valid()) {
                p_filter->test_multi(m_entries, m_filter.get_ptr());
            }
        }

        if (!b_clear && m_string.get_length()) {
            bool b_first = true;
            size_t focus;
            for (const auto n : std::ranges::views::iota(size_t{}, m_entries.get_count())) {
                if (m_mode == mode_query) {
                    if (m_filter[n]) {
                        focus = n;
                        b_first = false;
                        break;
                    }
                } else {
                    if (m_filter[n]) {
#ifdef SEARCH_CACHING_ENABLED
                        if (!stricmp_utf8_max(m_formatted[n], m_string, m_string.length()))
#else
                        m_entries[n]->format_title(0, m_buffer, m_to, 0);
                        if (!stricmp_utf8_max(m_buffer, m_string, m_string.length()))
#endif
                        {
                            if (b_first) {
                                focus = n;
                                b_first = false;
                            }
                        } else
                            m_filter[n] = false;
                    }
                }
            }

            // api->playlist_set_selection(active, bit_array_true(), bit_array_table_t<bool>(m_filter.get_ptr(),
            // m_filter.get_size()));
            const size_t the_focus = m_playlist_api->playlist_get_focus_item(m_active);
            if (!b_first && !(the_focus < m_filter.get_size() && m_filter[the_focus])) {
                // api->playlist_set_focus_item(active, focus);
                m_playlist_api->playlist_set_selection(m_active, bit_array_true(), bit_array_one(focus));
                m_playlist_api->playlist_set_focus_item(m_active, focus);
            } else if (!b_first && the_focus != pfc_infinite) {
                m_playlist_api->playlist_set_selection(m_active, bit_array_true(), bit_array_one(the_focus));
                m_playlist_api->playlist_ensure_visible(m_active, the_focus);
                // api->playlist_ensure_visible(active, the_focus);
            }
        } else {
            m_playlist_api->playlist_clear_selection(m_active);
        }
    }

    bool m_running{};
    metadb_handle_list_t<pfc::alloc_fast_aggressive> m_entries;
    service_ptr_t<titleformat_object> m_to;
    pfc::array_t<bool> m_filter;
    pfc::string8_fastalloc m_string;
    pfc::string8_fastalloc m_buffer;
    static_api_ptr_t<playlist_manager> m_playlist_api;
    size_t m_active{};
    t_uint32 m_mode{};
#ifdef SEARCH_CACHING_ENABLED
    pfc::array_t<pfc::string_simple> m_formatted;
#endif
};

} // namespace typefind_panel

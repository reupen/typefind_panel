
#pragma once

enum t_search_mode {
    mode_pattern_beginning = 0,
    mode_query = 1,
};

class progressive_search {
    bool m_running;
    metadb_handle_list_t<pfc::alloc_fast_aggressive> m_entries;
    service_ptr_t<titleformat_object> m_to;
    pfc::array_t<bool> m_filter;
    pfc::string8_fastalloc m_string;
    pfc::string8_fastalloc m_buffer;
    static_api_ptr_t<playlist_manager> api;
    unsigned active;
    t_uint32 m_mode;
#ifdef SEARCH_CACHING_ENABLED
    pfc::array_t<pfc::string_simple> m_formatted;
#endif
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
            unsigned focus;
            unsigned n, count = m_entries.get_count();
            for (n = 0; n < count; n++) {
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
            t_size the_focus = api->playlist_get_focus_item(active);
            if (!b_first && !(the_focus < m_filter.get_size() && m_filter[the_focus])) {
                // api->playlist_set_focus_item(active, focus);
                api->playlist_set_selection(active, bit_array_true(), bit_array_one(focus));
                api->playlist_set_focus_item(active, focus);
            } else if (!b_first && the_focus != pfc_infinite) {
                api->playlist_set_selection(active, bit_array_true(), bit_array_one(the_focus));
                api->playlist_ensure_visible(active, the_focus);
                // api->playlist_ensure_visible(active, the_focus);
            }
        } else {
            api->playlist_clear_selection(active);
        }
    }

public:
    void init()
    {
        if (!m_running) {
            active = api->get_active_playlist();
            unsigned count = api->playlist_get_item_count(active);
            m_entries.prealloc(count);
            api->playlist_get_all_items(active, m_entries);
            m_filter.set_size(count);
            m_filter.fill(true);
            m_buffer.prealloc(256);
#ifdef SEARCH_CACHING_ENABLED
            unsigned n;
            m_formatted.set_size(count);
            if (m_mode == mode_pattern_beginning) {
                for (n = 0; n < count; n++) {
                    m_entries[n]->format_title(0, m_buffer, m_to, 0);
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
        // hires_timer timer;
        // timer.start();
        init();
        m_string.add_char(c);
        run();
        // console::info(string_printf("search time: %u",(unsigned)(timer.query()*100000)));
    }
    void set_string(const char* src)
    {
        if (m_running)
            m_filter.fill(true);
        init();
        m_string.set_string(src);
        run();
    }
    void set_pattern(const char* src)
    {
        static_api_ptr_t<titleformat_compiler> tf_api;
        tf_api->compile(m_to, src);
    }
    void set_mode(t_uint32 mode) { m_mode = mode; }
    bool on_key(WPARAM wp)
    {
        switch (wp) {
        case VK_RETURN:
            if (m_running) {
                bool ctrl_down = 0 != (GetKeyState(VK_CONTROL) & KF_UP);
                if (ctrl_down) {
                    metadb_handle_ptr mh;
                    t_size focus = api->playlist_get_focus_item(active);
                    if (focus != pfc_infinite)
                        api->queue_add_item_playlist(active, focus);
                } else {
                    api->set_playing_playlist(active);
                    static_api_ptr_t<play_control> play_control;
                    play_control->play_start(play_control::track_command_settrack);
                }
            }
            return true;
        case VK_DOWN: {
            if (m_running) {
                unsigned focus = api->playlist_get_focus_item(active);
                unsigned count = m_filter.get_size();
                unsigned n;
                for (n = focus + 1; n < count; n++) {
                    if (m_filter[n]) {
                        api->playlist_set_selection(active, bit_array_true(), bit_array_one(n));
                        api->playlist_set_focus_item(active, n);
                        // api->playlist_set_focus_item(active, n);
                        break;
                    }
                }
            }
        }
            return true;
        case VK_UP: {
            if (m_running) {
                unsigned focus = api->playlist_get_focus_item(active);
                unsigned count = m_filter.get_size();
                unsigned n;
                for (n = focus; n > 0, n - 1 < count; n--) {
                    if (m_filter[n - 1]) {
                        api->playlist_set_selection(active, bit_array_true(), bit_array_one(n - 1));
                        api->playlist_set_focus_item(active, n - 1);
                        // api->playlist_set_focus_item(active, n-1);
                        break;
                    }
                }
            }
        }
            return true;
        }
        return false;
    }
    progressive_search() : m_running(false), active(0), m_mode(mode_pattern_beginning) {}
    ~progressive_search() {}
};

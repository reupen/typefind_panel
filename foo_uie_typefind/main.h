#pragma once

#define NOMINMAX
#define OEMRESOURCE
#define SEARCH_CACHING_ENABLED

#include <ranges>

// Included before windows.h, because pfc.h includes winsock2.h
#include "../pfc/pfc.h"

#include <Windows.h>
#include <windowsx.h>

#include <wil/win32_helpers.h>

#include "../foobar2000/SDK/foobar2000.h"

#include "../columns_ui-sdk/ui_extension.h"
#include "../ui_helpers/stdafx.h"
#include "../fbh/stdafx.h"

#include "resource.h"
#include "progressive_search.h"

using namespace uih::literals::spx;

namespace typefind_panel {

class ColourNotifier final : cui::colours::common_callback {
public:
    ColourNotifier(std::function<void()> on_dark_mode_change, std::function<void()> on_colours_change)
        : m_on_dark_mode_change(std::move(on_dark_mode_change))
        , m_on_colours_change(std::move(on_colours_change))
    {
        if (fb2k::std_api_try_get(m_api)) {
            m_api->register_common_callback(this);
        }
    }

    ~ColourNotifier()
    {
        if (m_api.is_valid()) {
            m_api->deregister_common_callback(this);
        }
    }

    void on_colour_changed(uint32_t changed_items_mask) const override
    {
        if (changed_items_mask & (cui::colours::colour_flag_text | cui::colours::colour_flag_background)) {
            m_on_colours_change();
        }
    }

    void on_bool_changed(uint32_t changed_items_mask) const override
    {
        if (changed_items_mask & cui::colours::bool_flag_dark_mode_enabled)
            m_on_dark_mode_change();
    }

private:
    std::function<void()> m_on_dark_mode_change;
    std::function<void()> m_on_colours_change;
    cui::colours::manager::ptr m_api;
};

class TypefindWindow : public uie::container_uie_window_v3 {
public:
    TypefindWindow();

    static void s_update_all_fonts();
    static void s_activate();

    static const GUID extension_guid;

    const GUID& get_extension_guid() const override { return extension_guid; }

    void get_name(pfc::string_base& out) const override;
    void get_category(pfc::string_base& out) const override;

    enum {
        stream_version_current = 0
    };
    void get_config(stream_writer* p_out, abort_callback& p_abort) const override;
    void set_config(stream_reader* p_source, size_t p_size, abort_callback& p_abort) override;

    unsigned get_type() const override { return ui_extension::type_panel; }

    uie::container_window_v3_config get_window_config() override { return {L"{89A3759F-348A-4e3f-BF43-3D16BC059186}"}; }

    void activate(bool b_focus = true)
    {
        // if (GetFocus() != wnd_edit)
        {
            height = uih::get_font_height(s_font.get()) + 2_spx;
            if (b_focus) {
                SetFocus(m_wnd_edit);
            }
            SendMessage(m_wnd_edit, EM_SETSEL, 0, -1);
            const auto text = uGetWindowText(m_wnd_edit);
            m_search.init();
            if (text.length())
                m_search.set_string(text);
            get_host()->on_size_limit_change(
                get_wnd(), ui_extension::size_limit_minimum_height | ui_extension::size_limit_maximum_height);
            on_size();
        }
    }
    void on_size(int cx, int cy);
    void on_size();

    bool have_config_popup() const override { return true; }
    bool show_config_popup(HWND wnd_parent) override;

    void get_menu_items(ui_extension::menu_hook_t& p_hook) override
    {
        p_hook.add_node(new uie::menu_node_configure(this));
        p_hook.add_node(new uie::simple_command_menu_node(
            "Activate", "Activate typefind", 0, [this, self = ptr{this}] { activate(); }));
    }

    friend class font_notify;

private:
    static LRESULT WINAPI s_handle_hooked_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) override;
    LRESULT WINAPI handle_hooked_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
    INT_PTR handle_config_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    void set_window_theme() const;

    constexpr static short ID_EDIT = 1001;

    inline static wil::unique_hfont s_font;
    inline static wil::unique_hbrush s_background_brush;
    inline static std::vector<TypefindWindow*> s_instances;

    HWND m_wnd_edit{};
    HWND m_wnd_previous_focus{};
    bool m_initialised{};
    WNDPROC m_editproc{};
    bool m_is_running{};
    int height{};
    t_uint32 m_mode{};
    ProgressiveSearch m_search;
    pfc::string8 m_pattern;
    std::unique_ptr<ColourNotifier> m_colours_notifier;
    modal_dialog_scope m_config_scope;
};

} // namespace typefind_panel

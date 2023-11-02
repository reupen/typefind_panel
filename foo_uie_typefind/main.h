#pragma once

#define NOMINMAX
#define OEMRESOURCE
#define SEARCH_CACHING_ENABLED

#include <ranges>

// Included before windows.h, because pfc.h includes winsock2.h
#include "../pfc/pfc.h"

#include <Windows.h>
#include <windowsx.h>

#include "../foobar2000/SDK/foobar2000.h"

#include "../columns_ui-sdk/ui_extension.h"
#include "../ui_helpers/stdafx.h"

#include "resource.h"
#include "progressive_search.h"

using namespace uih::literals::spx;

public:
    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) override;

    LRESULT WINAPI on_hook(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT WINAPI hook_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
    INT_PTR ConfigPopupProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    TypefindWindow();

    static void s_update_all_fonts();
    static void s_update_all_window_frames();
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
                SetFocus(wnd_edit);
            }
            uSendMessage(wnd_edit, EM_SETSEL, 0, -1);
            const auto text = uGetWindowText(wnd_edit);
            m_search.init();
            if (text.length())
                m_search.set_string(text);
            get_host()->on_size_limit_change(
                get_wnd(), ui_extension::size_limit_minimum_height | ui_extension::size_limit_maximum_height);
            on_size();
        }
    }
    void on_size(unsigned cx, unsigned cy);
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
    inline static wil::unique_hfont s_font;
    inline static std::vector<TypefindWindow*> s_instances;

    HWND wnd_edit;
    HWND wnd_prev;
    bool m_initialised;
    WNDPROC m_editproc;
    bool m_is_running;
    ProgressiveSearch m_search;
    unsigned height;
    pfc::string8 m_pattern;
    t_uint32 m_mode;

    modal_dialog_scope m_config_scope;
};

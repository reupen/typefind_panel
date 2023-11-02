#pragma once

#define NOMINMAX
#define OEMRESOURCE
#define SEARCH_CACHING_ENABLED

// Included before windows.h, because pfc.h includes winsock2.h
#include "../pfc/pfc.h"

#include <Windows.h>
#include <windowsx.h>
#include <CommCtrl.h>

#include "../foobar2000/SDK/foobar2000.h"

#include "../columns_ui-sdk/ui_extension.h"
#include "../ui_helpers/stdafx.h"

#include "resource.h"
#include "progressive_search.h"

class quickfind_window : public uie::container_uie_window_v3 {
    bool m_initialised;
    WNDPROC m_editproc;

    bool m_is_running;

    progressive_search m_search;

    unsigned height;
    pfc::string8 m_pattern;
    t_uint32 m_mode;

    modal_dialog_scope m_config_scope;

protected:
    HWND wnd_edit;
    HWND wnd_prev;

public:
    static pfc::ptr_list_t<quickfind_window> list_wnd;
    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    LRESULT WINAPI on_hook(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT WINAPI hook_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
    INT_PTR ConfigPopupProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    quickfind_window();

    static void g_update_all_fonts();

    ~quickfind_window();

    static const GUID extension_guid;

    virtual const GUID& get_extension_guid() const { return extension_guid; }

    virtual void get_name(pfc::string_base& out) const;
    virtual void get_category(pfc::string_base& out) const;

    enum {
        stream_version_current = 0
    };
    void get_config(stream_writer* p_out, abort_callback& p_abort) const;
    void set_config(stream_reader* p_source, t_size p_size, abort_callback& p_abort);

    unsigned get_type() const { return ui_extension::type_panel; }

    uie::container_window_v3_config get_window_config() override { return {L"{89A3759F-348A-4e3f-BF43-3D16BC059186}"}; }

    static void update_all_window_frames();

    void activate(bool b_focus = true)
    {
        // if (GetFocus() != wnd_edit)
        {
            height = uGetFontHeight(g_font) + 2;
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

    virtual bool have_config_popup() const { return true; }
    virtual bool show_config_popup(HWND wnd_parent);

    class menu_node_activate : public ui_extension::menu_node_command_t {
        service_ptr_t<quickfind_window> p_this;

    public:
        virtual bool get_display_data(pfc::string_base& p_out, unsigned& p_displayflags) const
        {
            p_out = "Activate";
            p_displayflags = 0;
            return true;
        }
        virtual bool get_description(pfc::string_base& p_out) const { return false; }
        virtual void execute() { p_this->activate(); }
        menu_node_activate(quickfind_window* host) : p_this(host){};
    };

    virtual void get_menu_items(ui_extension::menu_hook_t& p_hook)
    {
        ui_extension::menu_node_ptr p_node(new uie::menu_node_configure(this));
        ui_extension::menu_node_ptr p_node2(new menu_node_activate(this));
        p_hook.add_node(p_node);
        p_hook.add_node(p_node2);
    }

    friend class font_notify;

private:
    static HFONT g_font;
};

#include "main.h"
#include "version.h"

namespace typefind_panel {

DECLARE_COMPONENT_VERSION("Typefind",

    version,

    "allows you to locate tracks fast\n\n"
    "compiled: " __DATE__ "\n"
    "with Columns UI SDK version: " UI_EXTENSION_VERSION

);

constexpr GUID font_client_id = {0xc491b147, 0x91ea, 0x4247, {0x9e, 0x14, 0x3c, 0xde, 0x4e, 0xcc, 0xb0, 0xd}};

constexpr GUID guid_default_search_mode
    = {0x4e16a134, 0x1270, 0x4051, {0x9c, 0x4f, 0x77, 0x18, 0x19, 0xd4, 0xca, 0xd2}};

cfg_string cfg_default_search(
    GUID{0xe6e375cd, 0x6b89, 0x5fc8, {0x3f, 0xec, 0xce, 0xf4, 0x8b, 0xdc, 0x94, 0xcf}}, "%artist% - %title%");

cfg_int_t<t_uint32> cfg_default_search_mode(guid_default_search_mode, 0);

namespace {

class FontClient : public cui::fonts::client {
public:
    const GUID& get_client_guid() const override { return font_client_id; }

    void get_name(pfc::string_base& p_out) const override { p_out = "Typefind"; }

    cui::fonts::font_type_t get_default_font_type() const override { return cui::fonts::font_type_items; }

    void on_font_changed() const override { TypefindWindow::s_update_all_fonts(); }
};

FontClient::factory<FontClient> g_font_client_album_list;
} // namespace

void TypefindWindow::s_update_all_fonts()
{
    if (s_font != nullptr) {
        for (const auto instance : s_instances) {
            if (instance->m_wnd_edit)
                SendMessage(instance->m_wnd_edit, WM_SETFONT, 0, MAKELPARAM(0, 0));
        }
        s_font.reset();
    }

    s_font.reset(cui::fonts::helper(font_client_id).get_font());

    for (const auto instance : s_instances) {
        if (instance->m_wnd_edit)
            SendMessage(instance->m_wnd_edit, WM_SETFONT, reinterpret_cast<WPARAM>(s_font.get()), MAKELPARAM(1, 0));
    }
}

TypefindWindow::TypefindWindow() : m_pattern(cfg_default_search), m_mode(cfg_default_search_mode) {}

void TypefindWindow::s_activate()
{
    if (s_instances.empty()) {
        win32_helpers::message_box(
            core_api::get_main_window(), L"Please insert a Typefind window into a host", L"Error", MB_OK);
    } else {
        s_instances[0]->activate();
    }
}

void TypefindWindow::on_size(int cx, int cy)
{
    SetWindowPos(m_wnd_edit, nullptr, 0, 0, cx, height, SWP_NOZORDER);
}

void TypefindWindow::on_size()
{
    RECT rc;
    GetWindowRect(get_wnd(), &rc);
    on_size(wil::rect_width(rc), wil::rect_height(rc));
}

LRESULT TypefindWindow::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        s_instances.emplace_back(this);

        m_initialised = true;

        modeless_dialog_manager::g_add(wnd);

        m_search.set_pattern(m_pattern);
        m_search.set_mode(m_mode);

        m_wnd_edit = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0, wnd, reinterpret_cast<HMENU>(ID_EDIT), core_api::get_my_instance(), nullptr);

        if (m_wnd_edit) {
            if (s_font) {
                SendMessage(m_wnd_edit, WM_SETFONT, reinterpret_cast<WPARAM>(s_font.get()), MAKELPARAM(0, 0));
            } else
                s_update_all_fonts();

            SetWindowLongPtr(m_wnd_edit, GWLP_USERDATA, reinterpret_cast<LPARAM>(this));
            m_editproc = (WNDPROC)SetWindowLongPtr(
                m_wnd_edit, GWLP_WNDPROC, reinterpret_cast<LPARAM>(s_handle_hooked_edit_message));

            set_window_theme();
            m_colours_notifier = std::make_unique<ColourNotifier>([this, self = ptr{this}] { set_window_theme(); },
                [this, self = ptr{this}] {
                    s_background_brush.reset();
                    RedrawWindow(m_wnd_edit, nullptr, nullptr, RDW_INVALIDATE);
                });
        }
    } break;
    case WM_GETMINMAXINFO: {
        const auto mmi = reinterpret_cast<LPMINMAXINFO>(lp);
        mmi->ptMinTrackSize.y = height;
        mmi->ptMaxTrackSize.y = height;
        return 0;
    }
    case WM_SIZE:
        on_size(LOWORD(lp), HIWORD(lp));
        break;
    case WM_COMMAND:
        switch (wp) {
        case IDOK:
            if (m_search.on_key(VK_RETURN))
                return 0;
            break;
        case IDCANCEL: {
            SetFocus(m_wnd_previous_focus);
        }
            return 0;
        }
        break;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        cui::colours::helper colours;
        const auto dc = reinterpret_cast<HDC>(wp);
        SetTextColor(dc, colours.get_colour(cui::colours::colour_text));
        SetBkMode(dc, TRANSPARENT);

        if (!s_background_brush)
            s_background_brush.reset(CreateSolidBrush(colours.get_colour(cui::colours::colour_background)));

        return reinterpret_cast<LPARAM>(s_background_brush.get());
    }
    case WM_DESTROY:
        m_colours_notifier.reset();
        m_wnd_edit = nullptr;
        if (m_initialised) {
            std::erase(s_instances, this);
            if (s_instances.empty()) {
                s_font.reset();
                s_background_brush.reset();
            }
            m_initialised = false;
            modeless_dialog_manager::g_remove(wnd);
        }
        break;
    }
    return uDefWindowProc(wnd, msg, wp, lp);
}

LRESULT WINAPI TypefindWindow::s_handle_hooked_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    TypefindWindow* self = reinterpret_cast<TypefindWindow*>(GetWindowLongPtr(wnd, GWLP_USERDATA));

    return self ? self->handle_hooked_edit_message(wnd, msg, wp, lp) : uDefWindowProc(wnd, msg, wp, lp);
}

LRESULT WINAPI TypefindWindow::handle_hooked_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == VK_TAB) {
            ui_extension::window::g_on_tab(wnd);
        } else if (m_search.on_key(wp))
            return 0;
        else if (wp == VK_DELETE) {
            const LRESULT ret = CallWindowProc(m_editproc, wnd, msg, wp, lp);
            m_search.set_string(uGetWindowText(wnd));
            return ret;
        }
        /*
        else if (wp == VK_ESCAPE)
        {
            SetFocus(wnd_prev);
            return 0;
        }*/
        break;
    case WM_SYSKEYDOWN:
        break;
    case WM_CHAR: {
        bool ctrl_down = 0 != (GetKeyState(VK_CONTROL) & KF_UP);
        if (wp == VK_RETURN || (wp == 0xa && ctrl_down) || wp == VK_ESCAPE)
            return 0;
        else if (!ctrl_down) {
            // assert (wp != VK_DELETE);
            unsigned start{};
            unsigned end{};
            SendMessage(wnd, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
            if (wp == VK_BACK || start != end || end != SendMessage(wnd, WM_GETTEXTLENGTH, 0, 0)) {
                const LRESULT ret = CallWindowProc(m_editproc, wnd, msg, wp, lp);
                m_search.set_string(uGetWindowText(wnd));
                return ret;
            }
            m_search.add_char(gsl::narrow<unsigned>(wp));
        }
    } break;
    case WM_SETFOCUS:
        m_wnd_previous_focus = reinterpret_cast<HWND>(wp);
        if (height == 0)
            activate(false);
        break;
    case WM_KILLFOCUS: {
        m_search.reset();
        height = 0;
        get_host()->on_size_limit_change(
            get_wnd(), ui_extension::size_limit_minimum_height | ui_extension::size_limit_maximum_height);
        on_size();
    } break;
    case WM_GETDLGCODE:
        /*{
            MSG * msg = (LPMSG)lp;
            if (msg)
            {
            // let dialog manager handle it, otherwise to kill ping we have to process WM_CHAR to return 0 on wp == 0xd
        and 0xa if ((msg->lParam == WM_KEYDOWN) && msg->wParam == VK_BACK) return DLGC_WANTMESSAGE;
            }
        }*/
        break;
    }
    return CallWindowProc(m_editproc, wnd, msg, wp, lp);
}

void TypefindWindow::get_config(stream_writer* p_out, abort_callback& p_abort) const
{
    p_out->write_lendian_t(t_uint32(stream_version_current), p_abort);
    p_out->write_string(m_pattern, p_abort);
    p_out->write_lendian_t(m_mode, p_abort);
}

void TypefindWindow::get_name(pfc::string_base& out) const
{
    out.set_string("Typefind");
}
void TypefindWindow::get_category(pfc::string_base& out) const
{
    out.set_string("Toolbars");
}

void TypefindWindow::set_config(stream_reader* p_source, size_t p_size, abort_callback& p_abort)
{
    if (p_size) {
        t_uint32 version;
        p_source->read_lendian_t(version, p_abort);
        if (version <= stream_version_current) {
            p_source->read_string(m_pattern, p_abort);
            p_source->read_lendian_t(m_mode, p_abort);
        }
    }
}

const GUID TypefindWindow::extension_guid
    = {0x89a3759f, 0x348a, 0x4e3f, {0xbf, 0x43, 0x3d, 0x16, 0xbc, 0x5, 0x91, 0x86}};

INT_PTR TypefindWindow::handle_config_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        m_config_scope.initialize(FindOwningPopup(wnd));
        SetWindowText(GetDlgItem(wnd, IDC_VALUE), pfc::stringcvt::string_os_from_utf8(m_pattern.get_ptr()));

        HWND wnd_combo = GetDlgItem(wnd, IDC_MODE);
        ComboBox_AddString(wnd_combo, L"Search from beginning by pattern");
        ComboBox_AddString(wnd_combo, L"Query");
        ComboBox_SetCurSel(wnd_combo, m_mode);
        EnableWindow(GetDlgItem(wnd, IDC_VALUE), m_mode == mode_pattern_beginning);
    }
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDCANCEL:
            EndDialog(wnd, 0);
            return TRUE;
        case IDOK:
            m_pattern = uGetDlgItemText(wnd, IDC_VALUE);
            m_mode = ComboBox_GetCurSel(GetDlgItem(wnd, IDC_MODE));
            cfg_default_search = m_pattern;
            cfg_default_search_mode = m_mode;
            if (m_initialised) {
                m_search.reset();
                m_search.set_pattern(m_pattern);
                m_search.set_mode(m_mode);
            }
            EndDialog(wnd, 1);
            return TRUE;
        case IDC_MODE | (CBN_SELCHANGE << 16):
            EnableWindow(GetDlgItem(wnd, IDC_VALUE), ComboBox_GetCurSel(HWND(lp)) == 0);
            return TRUE;
        default:
            return FALSE;
        }
    case WM_DESTROY:
        m_config_scope.deinitialize();
        return FALSE;
    default:
        return FALSE;
    }
}

bool TypefindWindow::show_config_popup(HWND wnd_parent)
{
    return fbh::auto_dark_modal_dialog_box(IDD_CONFIG, wnd_parent, [this, self = ptr{this}](auto&&... args) {
        return handle_config_dialog_message(std::forward<decltype(args)>(args)...);
    }) != 0;
}

void TypefindWindow::set_window_theme() const
{
    if (!m_wnd_edit)
        return;

    const auto is_dark = cui::colours::is_dark_mode_active();
    SetWindowTheme(m_wnd_edit, is_dark ? L"DarkMode_Explorer" : nullptr, nullptr);
}

ui_extension::window_factory<TypefindWindow> blah;

class TypefindMenuItem : public mainmenu_commands {
    t_uint32 get_command_count() override { return 1; }

    GUID get_command(t_uint32 p_index) override
    {
        return {0x44d7861a, 0xdfd8, 0x4a44, {0x8e, 0x6a, 0x17, 0x38, 0xed, 0x3e, 0x3e, 0xd5}};
    }

    void get_name(t_uint32 p_index, pfc::string_base& p_out) override { p_out = "Type-find"; }

    bool get_description(t_uint32 p_index, pfc::string_base& p_out) override
    {
        p_out = "Activates type-find";
        return true;
    }

    GUID get_parent() override { return mainmenu_groups::edit_part2; }
    t_uint32 get_sort_priority() override { return sort_priority_dontcare; }

    bool get_display(t_uint32 p_index, pfc::string_base& p_text, t_uint32& p_flags) override
    {
        p_flags = 0;
        return false;
    }

    void execute(t_uint32 p_index, service_ptr_t<service_base> p_callback) override { TypefindWindow::s_activate(); }
};

namespace {

mainmenu_commands_factory_t<TypefindMenuItem> _menu_item;

}

} // namespace typefind_panel

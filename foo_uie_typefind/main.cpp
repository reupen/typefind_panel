#include "pch.h"

#include "main.h"
#include "version.h"
#include "resource.h"

using namespace wil::literals;

namespace typefind_panel {

namespace {

struct SearchModeDescription {
    SearchMode mode;
    wil::zwstring_view name;
};

std::vector search_mode_descriptions{
    SearchModeDescription{
        SearchMode::mode_match_words_beginning_formatted_title, L"Match beginning of any word in formatted title"_zv},
    SearchModeDescription{
        SearchMode::mode_match_beginning_formatted_title, L"Match beginning of entire formatted title"_zv},
    SearchModeDescription{SearchMode::mode_query, L"Use query syntax"_zv},
};

void update_dependent_config_controls(const ConfigPopupState& state, HWND wnd)
{
    const auto enable_title_format = state.mode == SearchMode::mode_match_beginning_formatted_title
        || state.mode == SearchMode::mode_match_words_beginning_formatted_title;

    const auto title_format_wnd = GetDlgItem(wnd, IDC_VALUE);
    const auto ignore_symbols_wnd = GetDlgItem(wnd, IDC_IGNORE_SYMBOLS);

    EnableWindow(title_format_wnd, enable_title_format);
    EnableWindow(ignore_symbols_wnd, enable_title_format);

    uSetWindowText(title_format_wnd, enable_title_format ? state.title_format.c_str() : "");
    Button_SetCheck(ignore_symbols_wnd, enable_title_format && state.ignore_symbols ? BST_CHECKED : BST_UNCHECKED);
    uSetWindowText(title_format_wnd, enable_title_format ? state.title_format.c_str() : "");
}

} // namespace

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
    GUID{0xe6e375cd, 0x6b89, 0x5fc8, {0x3f, 0xec, 0xce, 0xf4, 0x8b, 0xdc, 0x94, 0xcf}}, "%artist% %title%");

cfg_int_t<t_uint32> cfg_default_search_mode(
    guid_default_search_mode, WI_EnumValue(SearchMode::mode_match_words_beginning_formatted_title));

cfg_bool cfg_default_ignore_symbols(
    {0xc8be1c4f, 0x7add, 0x400d, {0xa4, 0xb, 0xf1, 0x30, 0x21, 0xbc, 0xba, 0x15}}, true);

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
    const auto old_font = std::move(s_font);
    s_font.reset(cui::fonts::helper(font_client_id).get_font());

    for (const auto instance : s_instances) {
        if (instance->m_wnd_edit)
            SetWindowFont(instance->m_wnd_edit, s_font.get(), TRUE);
    }
}

TypefindWindow::TypefindWindow()
    : m_ignore_symbols(cfg_default_ignore_symbols)
    , m_mode(static_cast<SearchMode>(cfg_default_search_mode.get()))
    , m_pattern(cfg_default_search.get())
{
}

void TypefindWindow::s_activate()
{
    if (s_instances.empty()) {
        fbh::show_info_box_modeless(core_api::get_main_window(), "Error",
            "Please insert a Typefind window into a host.", uih::InfoBoxType::Information);
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

        m_search.set_pattern(m_pattern, m_ignore_symbols);
        m_search.set_mode(m_mode);

        m_wnd_edit = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0, wnd, reinterpret_cast<HMENU>(ID_EDIT), core_api::get_my_instance(), nullptr);

        if (m_wnd_edit) {
            if (s_font) {
                SetWindowFont(m_wnd_edit, s_font.get(), FALSE);
            } else
                s_update_all_fonts();

            uih::subclass_window_and_paint_with_buffering(m_wnd_edit);
            uih::enhance_edit_control(m_wnd_edit);
            uih::subclass_window(m_wnd_edit, [this](auto wnd_proc, auto wnd, auto msg, auto wp, auto lp) {
                return handle_hooked_edit_message(wnd_proc, wnd, msg, wp, lp);
            });

            set_window_theme();
            m_colours_notifier = std::make_unique<ColourNotifier>([this, self = ptr{this}] { set_window_theme(); },
                [this, self = ptr{this}] {
                    s_background_brush.reset();
                    RedrawWindow(m_wnd_edit, nullptr, nullptr, RDW_INVALIDATE);
                });
        }
        break;
    }
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
    return DefWindowProc(wnd, msg, wp, lp);
}

std::optional<LRESULT> WINAPI TypefindWindow::handle_hooked_edit_message(
    WNDPROC wnd_proc, HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == VK_TAB) {
            ui_extension::window::g_on_tab(wnd);
        } else if (m_search.on_key(wp))
            return 0;
        else if (wp == VK_DELETE) {
            const LRESULT ret = CallWindowProc(wnd_proc, wnd, msg, wp, lp);
            m_search.set_string(uih::get_window_text(wnd));
            return ret;
        }
        break;
    case WM_SYSKEYDOWN:
        break;
    case WM_CHAR: {
        bool ctrl_down = 0 != (GetKeyState(VK_CONTROL) & KF_UP);

        if (wp == VK_RETURN || (wp == 0xa && ctrl_down) || wp == VK_ESCAPE)
            return 0;

        if (!ctrl_down) {
            unsigned start{};
            unsigned end{};
            SendMessage(wnd, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
            if (wp == VK_BACK || start != end || end != SendMessage(wnd, WM_GETTEXTLENGTH, 0, 0)) {
                const LRESULT ret = CallWindowProc(wnd_proc, wnd, msg, wp, lp);
                m_search.set_string(uih::get_window_text(wnd));
                return ret;
            }
            m_search.add_char(gsl::narrow<wchar_t>(wp));
        }
        break;
    }
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
        break;
    }
    }
    return {};
}

void TypefindWindow::get_config(stream_writer* p_out, abort_callback& p_abort) const
{
    p_out->write_lendian_t(static_cast<t_uint32>(stream_version_current), p_abort);
    p_out->write_string(m_pattern, p_abort);
    p_out->write_lendian_t(WI_EnumValue(m_mode), p_abort);
    p_out->write_lendian_t(m_ignore_symbols, p_abort);
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
    if (p_size == 0)
        return;

    if (const auto stream_version = p_source->read_lendian_t<uint32_t>(p_abort);
        stream_version > stream_version_current)
        return;

    p_source->read_string(m_pattern, p_abort);
    m_mode = static_cast<SearchMode>(p_source->read_lendian_t<uint32_t>(p_abort));

    try {
        m_ignore_symbols = p_source->read_lendian_t<bool>(p_abort);
    } catch (const exception_io_data_truncation&) {
    }
}

const GUID TypefindWindow::extension_guid
    = {0x89a3759f, 0x348a, 0x4e3f, {0xbf, 0x43, 0x3d, 0x16, 0xbc, 0x5, 0x91, 0x86}};

INT_PTR TypefindWindow::handle_config_dialog_message(ConfigPopupState& state, HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        m_config_scope.initialize(FindOwningPopup(wnd));
        HWND wnd_combo = GetDlgItem(wnd, IDC_MODE);
        for (auto&& [index, mode_desc] : ranges::views::enumerate(search_mode_descriptions)) {
            ComboBox_AddString(wnd_combo, mode_desc.name.c_str());
            if (m_mode == mode_desc.mode)
                ComboBox_SetCurSel(wnd_combo, index);
        }

        SetWindowText(GetDlgItem(wnd, IDC_VALUE), pfc::stringcvt::string_os_from_utf8(state.title_format.c_str()));

        if (state.ignore_symbols)
            Button_SetCheck(GetDlgItem(wnd, IDC_IGNORE_SYMBOLS), BST_CHECKED);

        update_dependent_config_controls(state, wnd);
        return TRUE;
    }
    case WM_COMMAND:
        switch (wp) {
        case IDCANCEL:
            EndDialog(wnd, 0);
            return TRUE;
        case IDOK: {
            m_pattern = state.title_format;
            cfg_default_search = m_pattern;

            m_mode = state.mode;
            cfg_default_search_mode = WI_EnumValue(m_mode);

            m_ignore_symbols = state.ignore_symbols;
            cfg_default_ignore_symbols = m_ignore_symbols;

            if (m_initialised) {
                m_search.reset();
                m_search.set_pattern(m_pattern, m_ignore_symbols);
                m_search.set_mode(m_mode);
            }
            EndDialog(wnd, 1);
            return TRUE;
        }
        case IDC_MODE | (CBN_SELCHANGE << 16): {
            const auto mode_index = ComboBox_GetCurSel(GetDlgItem(wnd, IDC_MODE));
            if (mode_index >= 0 && std::cmp_less(mode_index, search_mode_descriptions.size()))
                state.mode = search_mode_descriptions[mode_index].mode;

            update_dependent_config_controls(state, wnd);
            return TRUE;
        }
        case IDC_VALUE | (EN_CHANGE << 16):
            state.title_format = uGetWindowText(reinterpret_cast<HWND>(lp));
            return TRUE;
        case IDC_IGNORE_SYMBOLS:
            state.ignore_symbols = Button_GetCheck(reinterpret_cast<HWND>(lp)) == BST_CHECKED;
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
    return fbh::auto_dark_modal_dialog_box(IDD_CONFIG, wnd_parent,
               [this, self = ptr{this}, state{ConfigPopupState{m_mode, m_pattern, m_ignore_symbols}}](
                   auto&&... args) mutable {
                   return handle_config_dialog_message(state, std::forward<decltype(args)>(args)...);
               })
        != 0;
}

void TypefindWindow::set_window_theme() const
{
    if (!m_wnd_edit)
        return;

    const auto is_dark = cui::colours::is_dark_mode_active();
    SetWindowTheme(m_wnd_edit, is_dark ? L"DarkMode_Explorer" : nullptr, nullptr);
}

namespace {

uie::window_factory<TypefindWindow> _typefind_factory;

}

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

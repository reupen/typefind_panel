#include "main.h"

DECLARE_COMPONENT_VERSION("Typefind",

"0.3",

"allows you to locate tracks fast\n\n"
"compiled: " __DATE__ "\n"
"with Columns UI SDK version: " UI_EXTENSION_VERSION

);

struct create_guid : public GUID
{
	create_guid(t_uint32 p_data1, t_uint16 p_data2, t_uint16 p_data3, t_uint8 p_data41, t_uint8 p_data42, t_uint8 p_data43, t_uint8 p_data44, t_uint8 p_data45, t_uint8 p_data46, t_uint8 p_data47, t_uint8 p_data48) 
	{
		Data1 = p_data1;
		Data2 = p_data2;
		Data3 = p_data3;
		Data4[0] = p_data41;
		Data4[1] = p_data42;
		Data4[2] = p_data43;
		Data4[3] = p_data44;
		Data4[4] = p_data45;
		Data4[5] = p_data46;
		Data4[6] = p_data47;
		Data4[7] = p_data48;
	}
};

cfg_int cfg_frame(create_guid(0x05550547,0xbf98,0x088c,0xbe,0x0e,0x24,0x95,0xe4,0x9b,0x88,0xc7),2);

inline static LOGFONT get_def_font()
{
	LOGFONT foo;
	uGetMenuFont(&foo);
	return foo;
}

static cfg_struct_t<LOGFONT> cfg_font(create_guid(0xb2c703ed,0x5a98,0xfb67,0x82,0xa0,0xfd,0x1a,0x44,0xeb,0xd5,0x47),get_def_font());


// {4E16A134-1270-4051-9C4F-771819D4CAD2}
static const GUID guid_default_search_mode = 
{ 0x4e16a134, 0x1270, 0x4051, { 0x9c, 0x4f, 0x77, 0x18, 0x19, 0xd4, 0xca, 0xd2 } };

cfg_string cfg_default_search(create_guid(0xe6e375cd,0x6b89,0x5fc8,0x3f,0xec,0xce,0xf4,0x8b,0xdc,0x94,0xcf), "%artist% - %title%");
cfg_int_t<t_uint32> cfg_default_search_mode(guid_default_search_mode, 0);




pfc::ptr_list_t<quickfind_window> quickfind_window::list_wnd;
HFONT quickfind_window::g_font = 0;

void quickfind_window::g_update_all_fonts()
{
	if (g_font!=0)
	{
		unsigned n, count = quickfind_window::list_wnd.get_count();
		for (n=0; n<count; n++)
		{
			HWND wnd = quickfind_window::list_wnd[n]->wnd_edit;
			if (wnd) uSendMessage(wnd,WM_SETFONT,(WPARAM)0,MAKELPARAM(0,0));
		}
		DeleteObject(g_font);
	}

	g_font = CreateFontIndirect(&(LOGFONT)cfg_font);

	unsigned n, count = quickfind_window::list_wnd.get_count();
	for (n=0; n<count; n++)
	{
		HWND wnd = quickfind_window::list_wnd[n]->wnd_edit;
		if (wnd) 
		{
			uSendMessage(wnd,WM_SETFONT,(WPARAM)g_font,MAKELPARAM(1,0));
		}
	}
}



quickfind_window::quickfind_window() : wnd_edit(0), m_is_running(false), m_initialised(false), m_editproc(0), m_pattern(cfg_default_search), height(0), wnd_prev(0), m_mode(cfg_default_search_mode)
{
}

quickfind_window::~quickfind_window()
{
}

void quickfind_window::update_all_window_frames()
{
	unsigned n, count = list_wnd.get_count();
	long flags = 0;
	if (cfg_frame == 1) flags |= WS_EX_CLIENTEDGE;
	if (cfg_frame == 2) flags |= WS_EX_STATICEDGE;

	for (n=0; n<count; n++)
	{
		HWND wnd = list_wnd[n]->wnd_edit;
		if (wnd)
		{
			SetWindowLongPtr(wnd, GWL_EXSTYLE, flags);
			SetWindowPos(wnd,0,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
		}
	}
}

void quickfind_window::on_size(unsigned cx, unsigned cy)
{
	SetWindowPos(wnd_edit, 0, 0, 0, cx, height, SWP_NOZORDER);
}
void quickfind_window::on_size()
{
	RECT rc;
	GetWindowRect(get_wnd(), &rc);
	on_size(RECT_CX(rc), RECT_CY(rc));
}

LRESULT quickfind_window::on_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{

	switch(msg)
	{
	case WM_CREATE:
		{
			list_wnd.add_item(this);

			m_initialised = true;

			modeless_dialog_manager::g_add(wnd);

			long flags = 0;
			if (cfg_frame == 1) flags |= WS_EX_CLIENTEDGE;
			else if (cfg_frame == 2) flags |= WS_EX_STATICEDGE;

			m_search.set_pattern(m_pattern);
			m_search.set_mode(m_mode);

			wnd_edit = CreateWindowEx(flags, WC_EDIT, _T(""),
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0,
				wnd, HMENU(IDC_TREE), core_api::get_my_instance(), NULL);

			if (wnd_edit)
			{
				if (g_font)
				{
					uSendMessage(wnd_edit,WM_SETFONT,(WPARAM)g_font,MAKELPARAM(0,0));
				}
				else
					g_update_all_fonts();

				SetWindowLongPtr(wnd_edit,GWL_USERDATA,(LPARAM)(this));
				m_editproc = (WNDPROC)SetWindowLongPtr(wnd_edit,GWL_WNDPROC,(LPARAM)(hook_proc));
			}
		}
		break;
	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO mmi = LPMINMAXINFO(lp);
			mmi->ptMinTrackSize.y = height;
			mmi->ptMaxTrackSize.y = height;
			return 0;
		}
	case WM_SIZE:
		on_size(LOWORD(lp), HIWORD(lp));
		break;
	case WM_COMMAND:
		switch (wp)
		{
		case IDOK:
			if (m_search.on_key(VK_RETURN))
				return 0;
			break;
		case IDCANCEL:
			{
				SetFocus(wnd_prev);
			}
			return 0;
		}
		break;
	case WM_DESTROY:
		wnd_edit=0;
		if (m_initialised) 
		{
			list_wnd.remove_item(this);
			if (list_wnd.get_count() == 0)
			{
				DeleteFont(g_font);
				g_font = 0;
			}
			m_initialised = false;
			modeless_dialog_manager::g_remove(wnd);
		}
		break;
	}
	return uDefWindowProc(wnd, msg, wp, lp);
}

LRESULT WINAPI quickfind_window::hook_proc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	quickfind_window * p_this;
	LRESULT rv;

	p_this = reinterpret_cast<quickfind_window*>(GetWindowLongPtr(wnd,GWL_USERDATA));

	rv = p_this ? p_this->on_hook(wnd,msg,wp,lp) : uDefWindowProc(wnd, msg, wp, lp);

	return rv;
}

LRESULT WINAPI quickfind_window::on_hook(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{

	switch(msg)
	{
	case WM_KEYDOWN:
		if (wp == VK_TAB)
		{
			ui_extension::window::g_on_tab(wnd);
		}
		else if (m_search.on_key(wp)) return 0;
		else if (wp == VK_DELETE)
		{
			LRESULT ret = CallWindowProc(m_editproc,wnd,msg,wp,lp);
			m_search.set_string(string_utf8_from_window(wnd));
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
	case  WM_CHAR:
		{
			bool ctrl_down = 0!=(GetKeyState(VK_CONTROL) & KF_UP);
			if (wp == VK_RETURN || (wp == 0xa && ctrl_down)||wp == VK_ESCAPE) return 0;
			else
				if (!ctrl_down)
			{
				//assert (wp != VK_DELETE);
				unsigned start, end;
				SendMessage(wnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
				if (wp == VK_BACK || start != end || end != uSendMessage(wnd, WM_GETTEXTLENGTH, 0, 0))
				{
					LRESULT ret = uCallWindowProc(m_editproc,wnd,msg,wp,lp);
					m_search.set_string(string_utf8_from_window(wnd));
					return ret;
				}
				m_search.add_char(wp);
			}
		}
		break;
	case WM_SETFOCUS:
		wnd_prev = (HWND)wp;
		if (height ==0) activate(false);
		break;
	case WM_KILLFOCUS:
		{
			m_search.reset();
			height = 0;
			get_host()->on_size_limit_change(get_wnd(), ui_extension::size_limit_minimum_height|ui_extension::size_limit_maximum_height);
			on_size();
		}
		break;
	case WM_GETDLGCODE:
		/*{
			MSG * msg = (LPMSG)lp;
			if (msg)
			{
			// let dialog manager handle it, otherwise to kill ping we have to process WM_CHAR to return 0 on wp == 0xd and 0xa
				if ((msg->lParam == WM_KEYDOWN) && msg->wParam == VK_BACK) return DLGC_WANTMESSAGE;
			}
		}*/
		break;
	}
	return uCallWindowProc(m_editproc,wnd,msg,wp,lp);
}

void quickfind_window::get_config(stream_writer * p_out, abort_callback & p_abort)const
{
	p_out->write_lendian_t(t_uint32(stream_version_current), p_abort);
	p_out->write_string(m_pattern, p_abort);
	p_out->write_lendian_t(m_mode, p_abort);
}

void quickfind_window::get_name(pfc::string_base & out)const
{
	out.set_string("Typefind");
}
void quickfind_window::get_category(pfc::string_base & out)const
{
	out.set_string("Toolbars");
}

void quickfind_window::set_config(stream_reader * p_source, t_size p_size, abort_callback & p_abort)
{
	if (p_size)
	{
		t_uint32 version;
		p_source->read_lendian_t(version, p_abort);
		if (version <= stream_version_current)
		{
			p_source->read_string(m_pattern, p_abort);
			p_source->read_lendian_t(m_mode, p_abort);
		}
	}
}


// {89A3759F-348A-4e3f-BF43-3D16BC059186}
const GUID quickfind_window::extension_guid = 
{ 0x89a3759f, 0x348a, 0x4e3f, { 0xbf, 0x43, 0x3d, 0x16, 0xbc, 0x5, 0x91, 0x86 } };

BOOL CALLBACK quickfind_window::ConfigPopupProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		SetWindowLongPtr(wnd,DWL_USER,lp);
		{
			quickfind_window * ptr = reinterpret_cast<quickfind_window*>(lp);
			ptr->m_config_scope.initialize(FindOwningPopup(wnd));
			SetWindowText(GetDlgItem(wnd, IDC_VALUE), pfc::stringcvt::string_os_from_utf8(ptr->m_pattern.get_ptr()));

			HWND wnd_combo = GetDlgItem(wnd, IDC_MODE);
			ComboBox_AddString(wnd_combo, L"Search from beginning by pattern");
			ComboBox_AddString(wnd_combo, L"Query");
			ComboBox_SetCurSel(wnd_combo, ptr->m_mode);
			EnableWindow(GetDlgItem(wnd, IDC_VALUE), ptr->m_mode == mode_pattern_beginning); 
		}
		return TRUE;
	case WM_COMMAND:
		switch(wp)
		{
		case IDCANCEL:
			{
				EndDialog(wnd, 0);
			}
			return TRUE;
		case IDOK:
			{
				quickfind_window * ptr = reinterpret_cast<quickfind_window*>(GetWindowLongPtr(wnd,DWL_USER));
				ptr->m_pattern = string_utf8_from_window(wnd, IDC_VALUE);
				ptr->m_mode = ComboBox_GetCurSel(GetDlgItem(wnd, IDC_MODE));
				cfg_default_search = ptr->m_pattern;
				cfg_default_search_mode = ptr->m_mode;
				if (ptr->m_initialised)
				{
					ptr->m_search.reset();
					ptr->m_search.set_pattern(ptr->m_pattern);
					ptr->m_search.set_mode(ptr->m_mode);
				}
				EndDialog(wnd,1);
			}
			return TRUE;
		case IDC_MODE|(CBN_SELCHANGE<<16):
			EnableWindow(GetDlgItem(wnd, IDC_VALUE), ComboBox_GetCurSel(HWND(lp)) == 0); 
			return TRUE;
		default:
			return FALSE;
		}
	case WM_DESTROY:
		{
			quickfind_window * ptr = reinterpret_cast<quickfind_window*>(GetWindowLongPtr(wnd,DWL_USER));
			ptr->m_config_scope.deinitialize();
		}
		return FALSE;
	default:
		return FALSE;
	}
}

bool quickfind_window::show_config_popup(HWND wnd_parent)
{
	bool rv = !!uDialogBox(IDD_CONFIG,wnd_parent,ConfigPopupProc,reinterpret_cast<LPARAM>(this));
	return rv;
}


ui_extension::window_factory<quickfind_window> blah;

class menu_item_impl_typefind : public mainmenu_commands
{


	virtual t_uint32 get_command_count() {return 1;};
	virtual GUID get_command(t_uint32 p_index)
	{
		// {44D7861A-DFD8-4a44-8E6A-1738ED3E3ED5}
		static const GUID rv = 
		{ 0x44d7861a, 0xdfd8, 0x4a44, { 0x8e, 0x6a, 0x17, 0x38, 0xed, 0x3e, 0x3e, 0xd5 } };
		return rv;
	}
	virtual void get_name(t_uint32 p_index,pfc::string_base & p_out){p_out = "Type-find";}
	virtual bool get_description(t_uint32 p_index,pfc::string_base & p_out){p_out = "Activates type-find"; return true;}
	virtual GUID get_parent() {return mainmenu_groups::edit_part2;}
	virtual t_uint32 get_sort_priority() {return sort_priority_dontcare;}
	virtual bool get_display(t_uint32 p_index,pfc::string_base & p_text,t_uint32 & p_flags) 
	{
		p_flags = 0;
		return false;
		/*
		if (quickfind_window::list_wnd.get_count())
		{
			get_name(p_index,p_text);
			return true;
		}
		else return false;
		*/
	}
	virtual void execute(t_uint32 p_index,service_ptr_t<service_base> p_callback)
	{
		if (quickfind_window::list_wnd.get_count())
		{
			quickfind_window::list_wnd[0]->activate();
		}
		else win32_helpers::message_box(core_api::get_main_window(), _T("Please insert a typefind window into a host"), _T("Error"), MB_OK);
	}

};

mainmenu_commands_factory_t<menu_item_impl_typefind> g_menu;
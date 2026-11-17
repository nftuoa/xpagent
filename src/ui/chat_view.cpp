#include "ui/chat_view.hpp"

#include <richedit.h>
#include <shellapi.h>

#include <cstdio>

#include "platform/encoding.hpp"
#include "ui/i18n.hpp"

namespace xpagent {

namespace {

size_t Utf8CompletePrefixLength(const std::string& s) {
    if (s.empty()) return 0;
    size_t n = s.size();
    size_t i = n;
    while (i > 0 && (static_cast<unsigned char>(s[i - 1]) & 0xC0) == 0x80 && (n - i) < 3) {
        --i;
    }
    if (i == 0) return 0;
    unsigned char lead = static_cast<unsigned char>(s[i - 1]);
    size_t expected = 1;
    if ((lead & 0x80) == 0) expected = 1;
    else if ((lead & 0xE0) == 0xC0) expected = 2;
    else if ((lead & 0xF0) == 0xE0) expected = 3;
    else if ((lead & 0xF8) == 0xF0) expected = 4;
    else return n;

    if (n - (i - 1) < expected) {
        return i - 1;
    }
    return n;
}

std::wstring CurrentTimeStampW() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[16];
    _snwprintf(buf, 15, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    buf[15] = L'\0';
    return buf;
}

DWORD CALLBACK StreamOutFileCallback(DWORD_PTR cookie, LPBYTE buf, LONG cb, LONG* pcb) {
    HANDLE h = reinterpret_cast<HANDLE>(cookie);
    DWORD written = 0;
    if (!WriteFile(h, buf, static_cast<DWORD>(cb), &written, nullptr)) {
        *pcb = 0;
        return 1;
    }
    *pcb = static_cast<LONG>(written);
    return 0;
}

} // namespace

ChatView::~ChatView() {
    if (riched_dll_) {
        FreeLibrary(riched_dll_);
        riched_dll_ = nullptr;
    }
}

bool ChatView::Create(HWND parent, HINSTANCE hinst, int x, int y, int w, int h) {
    riched_dll_ = LoadLibraryW(L"msftedit.dll");
    if (riched_dll_) {
        hwnd_ = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            x, y, w, h, parent, nullptr, hinst, nullptr);
        if (!hwnd_) {
            FreeLibrary(riched_dll_);
            riched_dll_ = nullptr;
        }
    }
    if (!hwnd_) {
        riched_dll_ = LoadLibraryW(L"riched20.dll");
        if (riched_dll_) {
            hwnd_ = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"RichEdit20W", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                x, y, w, h, parent, nullptr, hinst, nullptr);
        }
    }
    if (!hwnd_) {
        hwnd_ = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            x, y, w, h, parent, nullptr, hinst, nullptr);
    }
    if (!hwnd_) return false;

    SendMessageW(hwnd_, EM_EXLIMITTEXT, 0, 4 * 1024 * 1024);
    SendMessageW(hwnd_, EM_AUTOURLDETECT, TRUE, 0);
    SendMessageW(hwnd_, EM_SETEVENTMASK, 0, ENM_LINK);
    SendMessageW(hwnd_, EM_SETBKGNDCOLOR, 0, RGB(255, 255, 255));
    return true;
}

void ChatView::Resize(int x, int y, int w, int h) {
    if (hwnd_) MoveWindow(hwnd_, x, y, w, h, TRUE);
}

void ChatView::Clear() {
    pending_utf8_.clear();
    if (hwnd_) SetWindowTextW(hwnd_, L"");
}

void ChatView::AppendStyledWide(
    const std::wstring& text, COLORREF color, bool bold, bool mono, bool indent) {
    if (!hwnd_ || text.empty()) return;

    LONG text_len = GetWindowTextLengthW(hwnd_);
    CHARRANGE cr{text_len, text_len};
    SendMessageW(hwnd_, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr));

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_FACE | CFM_SIZE;
    cf.crTextColor = color;
    cf.dwEffects = bold ? CFE_BOLD : 0;
    cf.yHeight = mono ? 180 : 200; // 9pt / 10pt in twips
    wcscpy(cf.szFaceName, mono ? L"Lucida Console" : L"Tahoma");
    SendMessageW(hwnd_, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

    PARAFORMAT2 pf{};
    pf.cbSize = sizeof(pf);
    pf.dwMask = PFM_STARTINDENT;
    pf.dxStartIndent = indent ? 240 : 80;
    SendMessageW(hwnd_, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&pf));

    SendMessageW(hwnd_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    ScrollToBottom();
}

void ChatView::ScrollToBottom() {
    if (!hwnd_) return;
    SendMessageW(hwnd_, WM_VSCROLL, SB_BOTTOM, 0);
}

void ChatView::AppendBanner(const std::wstring& text) {
    AppendStyledWide(text + L"\r\n\r\n", RGB(96, 110, 140), false, false, false);
}

void ChatView::AppendSystemNotice(const std::wstring& text) {
    AppendStyledWide(L"*** [" + CurrentTimeStampW() + L"] " + text + L" ***\r\n",
                     RGB(128, 86, 152), true, false, false);
}

void ChatView::AppendUserMessage(const std::string& utf8_text) {
    AppendStyledWide(std::wstring(Tr().chat_you_label) + L"  " + CurrentTimeStampW() + L" " +
                         Tr().chat_says_label + L"\r\n",
                     RGB(16, 68, 158), true, false, false);
    AppendStyledWide(Utf8ToWide(utf8_text) + L"\r\n\r\n", RGB(24, 24, 24), false, false, true);
}

void ChatView::BeginAssistantTurn(const std::string& model_name) {
    pending_utf8_.clear();
    std::wstring label = L"XPAgent";
    if (!model_name.empty()) {
        label += L" (" + Utf8ToWide(model_name) + L")";
    }
    label += L"  " + CurrentTimeStampW() + L" " + Tr().chat_says_label + L"\r\n";
    AppendStyledWide(label, RGB(34, 120, 42), true, false, false);
}

void ChatView::AppendAssistantDelta(const std::string& utf8_delta) {
    pending_utf8_ += utf8_delta;
    size_t valid_len = Utf8CompletePrefixLength(pending_utf8_);
    if (valid_len == 0) return;

    std::string ready = pending_utf8_.substr(0, valid_len);
    pending_utf8_.erase(0, valid_len);
    AppendStyledWide(Utf8ToWide(ready), RGB(25, 25, 25), false, false, true);
}

void ChatView::EndAssistantTurn() {
    if (!pending_utf8_.empty()) {
        AppendStyledWide(Utf8ToWide(pending_utf8_), RGB(25, 25, 25), false, false, true);
        pending_utf8_.clear();
    }
    AppendStyledWide(L"\r\n\r\n", RGB(25, 25, 25), false, false, false);
}

void ChatView::AppendToolStatus(const std::string& utf8_text) {
    AppendStyledWide(L"  [" + CurrentTimeStampW() + L" " + Tr().chat_tool_tag + L"] " +
                         Utf8ToWide(utf8_text) + L"\r\n",
                     RGB(150, 92, 0), true, true, true);
}

void ChatView::AppendToolResult(const std::string& utf8_text) {
    AppendStyledWide(Utf8ToWide(utf8_text) + L"\r\n",
                     RGB(88, 96, 110), false, true, true);
}

void ChatView::AppendError(const std::string& utf8_text) {
    AppendStyledWide(L"[" + CurrentTimeStampW() + L" " + Tr().chat_error_tag + L"] " +
                         Utf8ToWide(utf8_text) + L"\r\n\r\n",
                     RGB(190, 28, 28), true, false, false);
}

bool ChatView::SaveToFile(const std::wstring& file_path) const {
    if (!hwnd_ || file_path.empty()) return false;

    bool is_rtf = false;
    if (file_path.size() >= 4) {
        std::wstring ext = file_path.substr(file_path.size() - 4);
        if (_wcsicmp(ext.c_str(), L".rtf") == 0) {
            is_rtf = true;
        }
    }

    HANDLE h = CreateFileW(file_path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    if (is_rtf) {
        EDITSTREAM es{};
        es.dwCookie = reinterpret_cast<DWORD_PTR>(h);
        es.pfnCallback = StreamOutFileCallback;
        SendMessageW(hwnd_, EM_STREAMOUT, SF_RTF, reinterpret_cast<LPARAM>(&es));
        CloseHandle(h);
        return es.dwError == 0;
    }

    int len = GetWindowTextLengthW(hwnd_);
    std::wstring wtext(static_cast<size_t>(len) + 1, L'\0');
    if (len > 0) {
        GetWindowTextW(hwnd_, &wtext[0], len + 1);
    }
    wtext.resize(static_cast<size_t>(len));
    std::string utf8 = "\xEF\xBB\xBF" + WideToUtf8(wtext);
    DWORD written = 0;
    BOOL ok = WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(h);
    return ok && written == utf8.size();
}

bool ChatView::HandleNotify(NMHDR* nmhdr) {
    if (!nmhdr || nmhdr->hwndFrom != hwnd_) return false;
    if (nmhdr->code == EN_LINK) {
        auto* link = reinterpret_cast<ENLINK*>(nmhdr);
        if (link->msg == WM_LBUTTONUP) {
            LONG len = link->chrg.cpMax - link->chrg.cpMin;
            if (len > 0 && len < 2048) {
                std::wstring url(static_cast<size_t>(len) + 1, L'\0');
                TEXTRANGEW tr{link->chrg, &url[0]};
                SendMessageW(hwnd_, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));
                ShellExecuteW(hwnd_, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            return true;
        }
    }
    return false;
}

} // namespace xpagent

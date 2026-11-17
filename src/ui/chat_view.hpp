#pragma once

#include <windows.h>

#include <string>

namespace xpagent {

class ChatView {
public:
    ChatView() = default;
    ~ChatView();

    bool Create(HWND parent, HINSTANCE hinst, int x, int y, int w, int h);
    HWND Handle() const { return hwnd_; }
    void Resize(int x, int y, int w, int h);

    void Clear();
    void AppendBanner(const std::wstring& text);
    void AppendSystemNotice(const std::wstring& text);
    void AppendUserMessage(const std::string& utf8_text);
    void BeginAssistantTurn(const std::string& model_name);
    void AppendAssistantDelta(const std::string& utf8_delta);
    void EndAssistantTurn();
    void AppendToolStatus(const std::string& utf8_text);
    void AppendToolResult(const std::string& utf8_text);
    void AppendError(const std::string& utf8_text);

    bool SaveToFile(const std::wstring& file_path) const;
    bool HandleNotify(NMHDR* nmhdr);

private:
    void AppendStyledWide(const std::wstring& text, COLORREF color, bool bold, bool mono, bool indent = false);
    void ScrollToBottom();

    HMODULE riched_dll_ = nullptr;
    HWND hwnd_ = nullptr;
    std::string pending_utf8_;
};

} // namespace xpagent

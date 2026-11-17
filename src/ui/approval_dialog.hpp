#pragma once

#include <windows.h>

#include <string>

namespace xpagent {

// Shows a modal Windows XP-styled confirmation dialog for high-risk tool actions.
// Returns true if user explicitly approves execution.
bool ShowApprovalDialog(
    HWND parent,
    HINSTANCE hinst,
    const std::string& title_utf8,
    const std::string& detail_utf8);

} // namespace xpagent

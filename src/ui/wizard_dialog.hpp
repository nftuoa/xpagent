#pragma once

#include <windows.h>

#include "agent/config.hpp"
#include "net/http_client.hpp"

namespace xpagent {

// Shows Windows XP-style configuration wizard dialog.
// Returns true if user clicked Finish/Save.
bool ShowSetupWizardDialog(
    HWND parent,
    HINSTANCE hinst,
    const HttpClient& http,
    AgentConfig& in_out_cfg);

std::wstring BrowseForFolderXp(HWND parent, const std::wstring& title);

} // namespace xpagent

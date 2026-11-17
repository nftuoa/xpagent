#pragma once

#include <string>

namespace xpagent {

std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);
std::string CodePageToUtf8(const std::string& bytes, unsigned int codepage);
std::string Utf8ToCodePage(const std::string& utf8, unsigned int codepage);

} // namespace xpagent

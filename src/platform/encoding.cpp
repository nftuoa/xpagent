#include "platform/encoding.hpp"

#include <windows.h>

namespace xpagent {

namespace {

std::wstring MultiByteToWide(const std::string& input, unsigned int codepage) {
    if (input.empty()) return {};
    int needed = MultiByteToWideChar(
        codepage, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(
        codepage, 0, input.data(), static_cast<int>(input.size()), &out[0], needed);
    return out;
}

std::string WideToMultiByte(const std::wstring& input, unsigned int codepage) {
    if (input.empty()) return {};
    int needed = WideCharToMultiByte(
        codepage, 0, input.data(), static_cast<int>(input.size()),
        nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(
        codepage, 0, input.data(), static_cast<int>(input.size()),
        &out[0], needed, nullptr, nullptr);
    return out;
}

} // namespace

std::wstring Utf8ToWide(const std::string& utf8) {
    return MultiByteToWide(utf8, CP_UTF8);
}

std::string WideToUtf8(const std::wstring& wide) {
    return WideToMultiByte(wide, CP_UTF8);
}

std::string CodePageToUtf8(const std::string& bytes, unsigned int codepage) {
    if (bytes.empty()) return {};
    // Validate UTF-8 directly if codepage is already UTF-8
    if (codepage == CP_UTF8) return bytes;
    return WideToUtf8(MultiByteToWide(bytes, codepage));
}

std::string Utf8ToCodePage(const std::string& utf8, unsigned int codepage) {
    if (utf8.empty()) return {};
    if (codepage == CP_UTF8) return utf8;
    return WideToMultiByte(Utf8ToWide(utf8), codepage);
}

} // namespace xpagent

#pragma once

#include <windows.h>

namespace xpagent {

class CriticalSection {
public:
    CriticalSection() { InitializeCriticalSection(&cs_); }
    ~CriticalSection() { DeleteCriticalSection(&cs_); }

    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;

    void Lock() { EnterCriticalSection(&cs_); }
    void Unlock() { LeaveCriticalSection(&cs_); }

private:
    CRITICAL_SECTION cs_;
};

class ScopedLock {
public:
    explicit ScopedLock(CriticalSection& cs) : cs_(cs) { cs_.Lock(); }
    ~ScopedLock() { cs_.Unlock(); }

    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

private:
    CriticalSection& cs_;
};

class WinEvent {
public:
    explicit WinEvent(bool manual_reset = false, bool initial_state = false)
        : handle_(CreateEventW(nullptr, manual_reset ? TRUE : FALSE,
                               initial_state ? TRUE : FALSE, nullptr)) {}
    ~WinEvent() {
        if (handle_) CloseHandle(handle_);
    }

    WinEvent(const WinEvent&) = delete;
    WinEvent& operator=(const WinEvent&) = delete;

    void Set() {
        if (handle_) SetEvent(handle_);
    }
    void Reset() {
        if (handle_) ResetEvent(handle_);
    }
    DWORD Wait(DWORD timeout_ms = INFINITE) const {
        return handle_ ? WaitForSingleObject(handle_, timeout_ms) : WAIT_FAILED;
    }
    HANDLE Handle() const { return handle_; }

private:
    HANDLE handle_;
};

} // namespace xpagent

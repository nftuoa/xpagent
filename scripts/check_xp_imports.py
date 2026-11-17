#!/usr/bin/env python3
"""
Audit a 32-bit Windows PE binary via i686-w64-mingw32-objdump to ensure
Windows XP SP3 (NT 5.1) compatibility:
1. Subsystem version <= 5.01 (Windows XP).
2. No non-XP / Vista+ DLLs (ucrtbase.dll, bcrypt.dll, api-ms-win-*, libstdc++, etc.).
3. No known Vista+ API imports from kernel32/advapi32/ws2_32/user32/shell32/msvcrt.
"""

import re
import subprocess
import sys

FORBIDDEN_DLL_PREFIXES = (
    "ucrtbase",
    "api-ms-win-",
    "ext-ms-",
    "bcrypt",
    "ncrypt",
    "libstdc++",
    "libgcc_s",
    "libwinpthread",
    "libcurl",
    "libmbed",
)

# Known Vista / Win7 / Win8 / Win10+ functions that commonly leak from modern toolchains/libs
FORBIDDEN_SYMBOLS = {
    "GetTickCount64",
    "InitializeConditionVariable",
    "SleepConditionVariableCS",
    "SleepConditionVariableSRW",
    "WakeConditionVariable",
    "WakeAllConditionVariable",
    "InitializeSRWLock",
    "AcquireSRWLockExclusive",
    "ReleaseSRWLockExclusive",
    "AcquireSRWLockShared",
    "ReleaseSRWLockShared",
    "TryAcquireSRWLockExclusive",
    "TryAcquireSRWLockShared",
    "InitializeCriticalSectionEx",
    "CreateEventExW",
    "CreateEventExA",
    "CreateSemaphoreExW",
    "CreateWaitableTimerExW",
    "InitOnceExecuteOnce",
    "InitOnceBeginInitialize",
    "InitOnceComplete",
    "GetFinalPathNameByHandleW",
    "GetFinalPathNameByHandleA",
    "CreateFile2",
    "CopyFile2",
    "CancelIoEx",
    "CancelSynchronousIo",
    "SetThreadErrorMode",
    "GetErrorMode",
    "GetSystemTimePreciseAsFileTime",
    "GetNumaHighestNodeNumber",
    "GetLogicalProcessorInformationEx",
    "LocaleNameToLCID",
    "LCIDToLocaleName",
    "GetUserDefaultLocaleName",
    "CompareStringEx",
    "LCMapStringEx",
    "GetLocaleInfoEx",
    "SetFileInformationByHandle",
    "GetFileInformationByHandleEx",
    "CreateSymbolicLinkW",
    "CreateSymbolicLinkA",
    "InetPtonW",
    "inet_pton",
    "InetNtopW",
    "inet_ntop",
    "GetAddrInfoW",
    "FreeAddrInfoW",
    "GetAddrInfoExW",
    "WSAPoll",
    "BCryptGenRandom",
    "BCryptOpenAlgorithmProvider",
    "BCryptCloseAlgorithmProvider",
    "RegGetValueW",
    "RegGetValueA",
    "SHGetKnownFolderPath",
    "TaskDialog",
    "TaskDialogIndirect",
    "rand_s",
    "_time64",  # Wait: _time64 exists in XP msvcrt.dll? Actually _time64 was added in VC7/XP msvcrt; _localtime64_s was Vista/VC8.
    "localtime_s",
    "gmtime_s",
    "memcpy_s",
    "memmove_s",
    "strcpy_s",
    "strcat_s",
    "sprintf_s",
    "snprintf_s",
    "_snprintf_s",
    "vsnprintf_s",
    "_vsnprintf_s",
    "_vsnprintf_s_l",
    "_vswprintf_s",
    "_snwprintf_s",
    "strerror_s",
    "_wfopen_s",
    "fopen_s",
}

# Allowed system DLLs natively present on stock Windows XP SP3
ALLOWED_XP_DLLS = {
    "kernel32.dll",
    "user32.dll",
    "gdi32.dll",
    "advapi32.dll",
    "shell32.dll",
    "shlwapi.dll",
    "comctl32.dll",
    "comdlg32.dll",
    "ole32.dll",
    "oleaut32.dll",
    "uxtheme.dll",
    "gdiplus.dll",
    "ws2_32.dll",
    "crypt32.dll",
    "msvcrt.dll",
    "winmm.dll",
    "version.dll",
    "imm32.dll",
    "msimg32.dll",
    "psapi.dll",
    "iphlpapi.dll",
    "userenv.dll",
    "secur32.dll",
}


def audit_pe(exe_path: str) -> int:
    proc = subprocess.run(
        ["i686-w64-mingw32-objdump", "-p", exe_path],
        capture_output=True,
        text=True,
        check=True,
    )
    out = proc.stdout

    # Check machine type (0x014c = i386)
    if "file format pei-i386" not in out:
        print(f"[FAIL] {exe_path} is not pei-i386 32-bit PE!")
        return 1

    # Check subsystem version
    m_major = re.search(r"MajorSubsystemVersion\s+(\d+)", out)
    m_minor = re.search(r"MinorSubsystemVersion\s+(\d+)", out)
    major_os = re.search(r"MajorOperatingSystemVersion\s+(\d+)", out)
    minor_os = re.search(r"MinorOperatingSystemVersion\s+(\d+)", out)
    if m_major and m_minor:
        maj, min_v = int(m_major.group(1)), int(m_minor.group(1))
        print(f"[INFO] SubsystemVersion = {maj}.{min_v:02d}")
        if (maj, min_v) > (5, 1):
            print(f"[FAIL] SubsystemVersion {maj}.{min_v:02d} > 5.01 (Windows XP)!")
            return 1
    if major_os and minor_os:
        omaj, omin = int(major_os.group(1)), int(minor_os.group(1))
        print(f"[INFO] OperatingSystemVersion = {omaj}.{omin:02d}")
        if (omaj, omin) > (5, 1):
            print(f"[WARN] OperatingSystemVersion {omaj}.{omin:02d} > 5.01")

    # Parse DLL imports
    current_dll = None
    dlls = set()
    bad_dlls = []
    bad_syms = []

    for line in out.splitlines():
        m_dll = re.match(r"^\s*DLL Name:\s*(\S+)", line)
        if m_dll:
            current_dll = m_dll.group(1).lower()
            dlls.add(current_dll)
            if any(current_dll.startswith(p) for p in FORBIDDEN_DLL_PREFIXES):
                bad_dlls.append(current_dll)
            elif current_dll not in ALLOWED_XP_DLLS:
                print(f"[WARN] Non-standard DLL imported: {current_dll}")
            continue

        if current_dll:
            m_sym = re.match(r"^\s*[0-9a-fA-F]+\s+(?:<none>|[0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+(\S+)", line)
            if m_sym:
                sym = m_sym.group(1)
                if sym in FORBIDDEN_SYMBOLS:
                    bad_syms.append((current_dll, sym))

    print(f"[INFO] Imported DLLs ({len(dlls)}): {', '.join(sorted(dlls))}")

    if bad_dlls:
        print(f"[FAIL] Forbidden DLLs for Windows XP: {bad_dlls}")
        return 1

    if bad_syms:
        print("[FAIL] Forbidden Vista+ symbols imported:")
        for dll, sym in bad_syms:
            print(f"       - {dll}: {sym}")
        return 1

    print(f"[PASS] {exe_path} passed Windows XP SP3 PE & import audit.")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <path-to-exe>")
        sys.exit(2)
    sys.exit(audit_pe(sys.argv[1]))

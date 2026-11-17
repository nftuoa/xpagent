#include "ui/i18n.hpp"

namespace xpagent {

namespace {

UiLang g_current_lang = UiLang::EnUS;

const UiStrings kEnUS = {
    L"XPAgent - Windows XP AI Desktop Assistant",
    L"XPAgent - Windows XP AI Assistant",

    L"&File",
    L"Connection & Model &Wizard...",
    L"Change &Workspace Folder...",
    L"Save Chat &Log...",
    L"E&xit",

    L"&Conversation",
    L"&New Conversation",
    L"&Stop Generation",
    L"Send Window &Nudge",

    L"&Tools",
    L"System &Health Check",
    L"Show/Hide Desktop &Buddy",

    L"&Language",
    L"&English",
    L"简体中文 (&Chinese)",

    L"&Help",
    L"&About XPAgent...",

    L"Common Tasks",
    L"New conversation",
    L"Change workspace...",
    L"Setup wizard...",
    L"Quick Diagnostics",
    L"Check PC health with AI",
    L"Inspect running processes",
    L"Safety & Experience",
    L"Allow workspace file writes",
    L"Confirm before exec_cmd",
    L"Show floating buddy",
    L"Enable chat sound alerts",

    L" (Online)",
    L" (Typing...)",
    L" (Approval Needed)",
    L"Workspace: ",
    L"Settings...",
    L"Save Log",

    L"Insert File...",
    L"Emoticons",
    L"Nudge!",
    L"Clear",
    L"Save Log...",
    L"&Send",
    L"Stop",

    L":-)  Smile",
    L"^_^  Happy",
    L">_<  Frustrated",
    L"(y)  Thumbs up",
    L"\\o/  Cheer",
    L"List workspace files",
    L"Please call list_dir to list files in the current workspace directory.",
    L"Check hardware & disk space",
    L"Please call get_system_info to inspect CPU, RAM, OS version, and disk space.",
    L"Inspect running processes",
    L"Please call list_processes to inspect currently running processes.",

    L"Status: Ready (Built-in TLS 1.3)",
    L"Status: XPAgent is typing",
    L"Model: ",
    L"Workspace: ",

    L"Welcome to XPAgent Live Chat (Native Windows XP Client)",
    L"• Direct HTTPS: Bundled mbedTLS 3.6 + libcurl + Mozilla CA bundle\r\n"
    L"• Shortcuts: Enter to send, Shift+Enter for newline, export .rtf/.txt logs",
    L"Configuration saved. Welcome to XPAgent!",
    L"Note: API is not configured yet. Click 'Settings...' or 'Setup wizard...' to get started.",
    L"Started a new conversation.",
    L"You",
    L"says:",
    L"Tool",
    L"Error",
    L"You sent a window nudge!",
    L"Chat log saved to: ",
    L"Failed to save chat log.",

    L"A task is currently running. Please wait or click Stop first.",
    L"Failed to start background agent thread.",
    L"Failed to initialize XPAgent main window.",
    L"Select XPAgent Workspace Root Folder",
    L"All Files (*.*)\0*.*\0Text & Code (*.txt;*.log;*.ini;*.json;*.bat)\0*.txt;*.log;*.ini;*.json;*.bat\0",
    L"Please inspect file: ",
    L"RTF Rich Text Log (*.rtf)\0*.rtf\0UTF-8 Plain Text (*.txt)\0*.txt\0",
    L"About XPAgent",
    L"XPAgent v1.1\r\n\r\n"
    L"Native AI Desktop Agent for Windows XP (NT 5.1).\r\n"
    L"• UI: Pure Win32 API + uxtheme.dll (Luna) + RichEdit + GDI+\r\n"
    L"• TLS: Statically linked mbedTLS 3.6 + libcurl + CryptGenRandom\r\n"
    L"• Tools: Workspace file sandbox, approved cmd.exe, system diagnostics",

    L"&Show XPAgent Window",
    L"Show Floating &Buddy",
    L"E&xit",
    L"XPAgent Approval Needed",
    L"AI task completed.",
    L"XPAgent Error",
    L"An error occurred.",
    L"XP Buddy Ready",
    L"Thinking...",
    L"Needs Approval!",

    L"XPAgent Setup Wizard",
    L"XPAgent Connection & Safety Wizard",
    L"Configure your OpenAI-compatible API endpoint, workspace folder, and prompt.",
    L"API Base URL:",
    L"API Key:",
    L"Default Model:",
    L"HTTP/SOCKS Proxy:",
    L"Workspace Folder:",
    L"System Prompt:",
    L"UI Language:",
    L"Auto (Follow Windows)",
    L"English",
    L"简体中文 (Chinese)",
    L"&Browse...",
    L"&Test TLS 1.3",
    L"Bundled mbedTLS 3.6 + Mozilla CA bundle (bypasses XP Schannel)",
    L"Testing TLS handshake and API authentication...",
    L"Failed to start connection test thread.",
    L"&Finish && Save",
    L"&Cancel",

    L"XPAgent Security Approval",
    L"Windows XP has no UAC isolation. Review the requested action carefully before allowing:",
    L"&Allow",
    L"&Deny",

    "Warning: AI requests to run a command",
    "Working Directory:\r\n",
    "\r\n\r\nCommand (cmd.exe /c):\r\n",
    "Confirm File Write",
    "Append to file:\r\n",
    "Overwrite file:\r\n",
    "\r\nBytes: ",
    "You are XPAgent, an AI assistant collaborating with the user through a native Windows XP (NT 5.1) desktop client. "
    "Answer questions directly using your knowledge, and when helpful use the client's local tools "
    "(reading/writing files in the workspace, inspecting XP system/process info, or running cmd.exe commands after user approval). "
    "Follow Windows XP path and command conventions, and keep answers concise and accurate.",
    "Please call get_system_info to inspect this Windows XP machine's OS version, CPU, RAM, and disk space, and summarize clearly.",
    "Please call list_processes to inspect currently running processes and highlight anything notable."
};

const UiStrings kZhCN = {
    L"XPAgent - Windows XP AI 即时助手",
    L"XPAgent - Windows XP AI 助手",

    L"文件(&F)",
    L"连接与模型向导(&W)...",
    L"切换工作区目录(&D)...",
    L"保存聊天记录(&L)...",
    L"退出(&X)",

    L"会话(&C)",
    L"新建会话(&N)",
    L"停止生成(&S)",
    L"发送窗口抖动(&J)",

    L"工具(&T)",
    L"一键系统体检(&D)",
    L"显示/隐藏桌面小助手(&B)",

    L"语言/Lang(&L)",
    L"&English",
    L"简体中文(&C)",

    L"帮助(&H)",
    L"关于 XPAgent(&A)...",

    L"常见任务",
    L"新建对话会话",
    L"切换工作区目录...",
    L"连接与模型向导...",
    L"快捷诊断",
    L"让 AI 体检本机配置",
    L"检查当前运行进程",
    L"Agent 安全与体验",
    L"允许工作区文件写入",
    L"执行命令需弹窗审批",
    L"显示桌面浮动小助手",
    L"开启聊天事件提示音",

    L" (在线)",
    L" (正在输入...)",
    L" (等待安全审批)",
    L"工作区: ",
    L"设置...",
    L"保存记录",

    L"插入文件...",
    L"表情/常用语",
    L"抖一抖!",
    L"清屏",
    L"保存记录...",
    L"发送(&S)",
    L"停止",

    L":-)  微笑",
    L"^_^  开心",
    L">_<  抓狂",
    L"(y)  点赞",
    L"\\o/  欢呼",
    L"查看工作区文件列表",
    L"请调用 list_dir 列出当前工作区目录的文件。",
    L"检查系统硬件与磁盘空间",
    L"请调用 get_system_info 检查本机配置、内存与各磁盘剩余空间。",
    L"检查当前运行进程",
    L"请调用 list_processes 列出当前正在运行的进程。",

    L"状态: 就绪 (TLS 1.3 直连)",
    L"状态: XPAgent 正在输入回复",
    L"模型: ",
    L"工作区: ",

    L"欢迎来到 XPAgent 实时对话室 (Windows XP 原生客户端)",
    L"• 安全直连: 内置 mbedTLS 3.6 + libcurl + Mozilla CA 证书包\r\n"
    L"• 快捷键: Enter 发送消息，Shift+Enter 换行，支持导出 .rtf/.txt 聊天记录",
    L"已完成初始化配置，欢迎使用 XPAgent！",
    L"提示：尚未完成 API 配置，请点击右上角“设置...”或左侧向导进行配置。",
    L"已开启全新对话会话。",
    L"您",
    L"说:",
    L"工具",
    L"错误",
    L"您发送了一个窗口抖动！",
    L"聊天记录已保存至: ",
    L"保存聊天记录失败。",

    L"当前已有任务正在执行，请稍候或先点击“停止”。",
    L"无法启动后台 Agent 线程",
    L"XPAgent 主窗口初始化失败。",
    L"选择 XPAgent 工作区根目录",
    L"所有文件 (*.*)\0*.*\0文本与代码 (*.txt;*.log;*.ini;*.json;*.bat)\0*.txt;*.log;*.ini;*.json;*.bat\0",
    L"请查看文件: ",
    L"RTF 富文本聊天记录 (*.rtf)\0*.rtf\0UTF-8 纯文本 (*.txt)\0*.txt\0",
    L"关于 XPAgent",
    L"XPAgent v1.1\r\n\r\n"
    L"面向 Windows XP (NT 5.1) 的原生 AI 即时助手。\r\n"
    L"• UI: 纯 Win32 API + uxtheme.dll (Luna) + RichEdit + GDI+\r\n"
    L"• TLS: 内置静态编译 mbedTLS 3.6 + libcurl + CryptGenRandom 熵源\r\n"
    L"• 工具: 沙箱文件读写、审批制 cmd.exe 执行、系统与进程诊断",

    L"显示 XPAgent 主窗口(&S)",
    L"显示桌面浮动小助手(&B)",
    L"退出(&X)",
    L"XPAgent 需要您审批",
    L"AI 任务已完成回复。",
    L"XPAgent 执行异常",
    L"发生错误",
    L"XP 助手待命",
    L"正在思考中...",
    L"等待您确认!",

    L"XPAgent 设置向导",
    L"XPAgent 连接与安全配置向导",
    L"配置兼容 OpenAI 协议的云端或局域网大模型接口与本地工作区目录。",
    L"API Base URL:",
    L"API Key:",
    L"默认模型名称:",
    L"HTTP/SOCKS 代理:",
    L"沙箱工作区目录:",
    L"系统提示词:",
    L"界面语言:",
    L"自动 (跟随系统)",
    L"English",
    L"简体中文",
    L"浏览(&B)...",
    L"测试 TLS 1.3 连接(&T)",
    L"内置 mbedTLS 3.6 + Mozilla CA 证书包（不依赖 XP Schannel）",
    L"正在执行 TLS 握手与接口测试...",
    L"无法启动测试线程",
    L"完成并保存(&F)",
    L"取消(&C)",

    L"XPAgent 安全审批",
    L"Windows XP 未启用 UAC 隔离，请核对以下操作内容后决定是否放行：",
    L"允许执行(&A)",
    L"拒绝(&D)",

    "警告: AI 请求执行系统命令",
    "工作目录:\r\n",
    "\r\n\r\n即将执行命令 (cmd.exe /c):\r\n",
    "确认文件写入操作",
    "追加写入文件:\r\n",
    "覆盖写入文件:\r\n",
    "\r\n字节数: ",
    "你是 XPAgent，通过 Windows XP (NT 5.1) 桌面客户端与用户协作的 AI 助手。"
    "你可以直接基于自身知识解答问题，也可按需调用客户端提供的本地工具（读写工作区文件、诊断 XP 系统与进程状态，或在用户批准后执行 cmd.exe 命令）。"
    "涉及命令或文件路径时请遵循 Windows XP 语法习惯，回答请简洁、准确。",
    "请调用 get_system_info 检查本机 Windows XP 硬件、内存和磁盘状态，并给出简洁评估。",
    "请调用 list_processes 查看当前运行的进程概览，列出占用主要注意力的进程。"
};

} // namespace

UiLang DetectOsUiLang() {
    LANGID lang_id = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(lang_id) == LANG_CHINESE) {
        return UiLang::ZhCN;
    }
    return UiLang::EnUS;
}

UiLang ResolveUiLang(const std::string& pref) {
    if (pref == "en" || pref == "en-US") return UiLang::EnUS;
    if (pref == "zh" || pref == "zh-CN") return UiLang::ZhCN;
    return DetectOsUiLang();
}

void SetCurrentUiLang(UiLang lang) {
    g_current_lang = lang;
}

UiLang GetCurrentUiLang() {
    return g_current_lang;
}

const UiStrings& Tr() {
    return (g_current_lang == UiLang::ZhCN) ? kZhCN : kEnUS;
}

} // namespace xpagent

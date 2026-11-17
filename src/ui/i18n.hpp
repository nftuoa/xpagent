#pragma once

#include <windows.h>

#include <string>

namespace xpagent {

enum class UiLang {
    EnUS,
    ZhCN
};

struct UiStrings {
    const wchar_t* app_title;
    const wchar_t* tray_tooltip;

    // Top menus
    const wchar_t* menu_file;
    const wchar_t* menu_file_wizard;
    const wchar_t* menu_file_workspace;
    const wchar_t* menu_file_save_log;
    const wchar_t* menu_file_exit;

    const wchar_t* menu_chat;
    const wchar_t* menu_chat_new;
    const wchar_t* menu_chat_stop;
    const wchar_t* menu_chat_nudge;

    const wchar_t* menu_tools;
    const wchar_t* menu_tools_sysinfo;
    const wchar_t* menu_tools_buddy;

    const wchar_t* menu_lang;
    const wchar_t* menu_lang_en;
    const wchar_t* menu_lang_zh;

    const wchar_t* menu_help;
    const wchar_t* menu_help_about;

    // Task pane
    const wchar_t* tp_group_common;
    const wchar_t* tp_new_chat;
    const wchar_t* tp_workspace;
    const wchar_t* tp_wizard;
    const wchar_t* tp_group_diag;
    const wchar_t* tp_sys_check;
    const wchar_t* tp_proc_check;
    const wchar_t* tp_group_policy;
    const wchar_t* tp_allow_write;
    const wchar_t* tp_cmd_approval;
    const wchar_t* tp_show_buddy;
    const wchar_t* tp_enable_sounds;

    // Chat header
    const wchar_t* hdr_online;
    const wchar_t* hdr_typing;
    const wchar_t* hdr_approval;
    const wchar_t* hdr_workspace_prefix;
    const wchar_t* hdr_btn_settings;
    const wchar_t* hdr_btn_save;

    // Toolbar & input
    const wchar_t* tb_insert_file;
    const wchar_t* tb_emoticons;
    const wchar_t* tb_nudge;
    const wchar_t* tb_clear;
    const wchar_t* tb_save;
    const wchar_t* btn_send;
    const wchar_t* btn_stop;

    // Quick inserts / emoticons
    const wchar_t* qi_smile;
    const wchar_t* qi_happy;
    const wchar_t* qi_frustrated;
    const wchar_t* qi_thumbsup;
    const wchar_t* qi_cheer;
    const wchar_t* qi_list_dir;
    const wchar_t* qi_list_dir_prompt;
    const wchar_t* qi_sysinfo;
    const wchar_t* qi_sysinfo_prompt;
    const wchar_t* qi_procs;
    const wchar_t* qi_procs_prompt;

    // Status bar
    const wchar_t* status_ready;
    const wchar_t* status_typing_prefix;
    const wchar_t* status_model_prefix;
    const wchar_t* status_ws_prefix;

    // Chat view messages
    const wchar_t* chat_welcome_notice;
    const wchar_t* chat_welcome_banner;
    const wchar_t* chat_setup_done;
    const wchar_t* chat_setup_pending;
    const wchar_t* chat_new_session;
    const wchar_t* chat_you_label;
    const wchar_t* chat_says_label;
    const wchar_t* chat_tool_tag;
    const wchar_t* chat_error_tag;
    const wchar_t* chat_nudge_notice;
    const wchar_t* chat_saved_prefix;
    const wchar_t* chat_save_failed;

    // Dialogs & prompts
    const wchar_t* msg_busy;
    const wchar_t* msg_start_worker_err;
    const wchar_t* msg_init_err;
    const wchar_t* browse_workspace_title;
    const wchar_t* open_file_filter;
    const wchar_t* open_file_prefix;
    const wchar_t* save_log_filter;
    const wchar_t* about_title;
    const wchar_t* about_body;

    // Tray & Buddy
    const wchar_t* tray_show;
    const wchar_t* tray_buddy;
    const wchar_t* tray_exit;
    const wchar_t* tray_need_approval;
    const wchar_t* tray_done;
    const wchar_t* tray_error;
    const wchar_t* tray_error_fallback;
    const wchar_t* buddy_idle;
    const wchar_t* buddy_thinking;
    const wchar_t* buddy_approval;

    // Wizard dialog
    const wchar_t* wiz_window_title;
    const wchar_t* wiz_banner_title;
    const wchar_t* wiz_banner_sub;
    const wchar_t* wiz_lbl_base_url;
    const wchar_t* wiz_lbl_api_key;
    const wchar_t* wiz_lbl_model;
    const wchar_t* wiz_lbl_proxy;
    const wchar_t* wiz_lbl_workspace;
    const wchar_t* wiz_lbl_sys_prompt;
    const wchar_t* wiz_lbl_lang;
    const wchar_t* wiz_lang_auto;
    const wchar_t* wiz_lang_en;
    const wchar_t* wiz_lang_zh;
    const wchar_t* wiz_btn_browse;
    const wchar_t* wiz_btn_test;
    const wchar_t* wiz_status_default;
    const wchar_t* wiz_status_testing;
    const wchar_t* wiz_status_thread_err;
    const wchar_t* wiz_btn_save;
    const wchar_t* wiz_btn_cancel;

    // Approval dialog
    const wchar_t* appr_window_title;
    const wchar_t* appr_desc;
    const wchar_t* appr_btn_allow;
    const wchar_t* appr_btn_deny;

    // Tool approval texts (UTF-8)
    const char* tool_cmd_title;
    const char* tool_cmd_workdir;
    const char* tool_cmd_command;
    const char* tool_write_title;
    const char* tool_write_append;
    const char* tool_write_overwrite;
    const char* tool_write_bytes;
    const char* default_system_prompt;
    const char* quick_sys_check_prompt;
    const char* quick_proc_check_prompt;
};

UiLang DetectOsUiLang();
UiLang ResolveUiLang(const std::string& pref);
void SetCurrentUiLang(UiLang lang);
UiLang GetCurrentUiLang();
const UiStrings& Tr();

} // namespace xpagent

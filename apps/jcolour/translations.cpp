#include "translations.h"
#include <QApplication>

namespace jcolour {

Translator::Translator(QObject* parent) : QObject(parent) {
    initTranslations();
}

Translator& Translator::instance() {
    static Translator instance;
    return instance;
}

void Translator::initTranslations() {
    // key -> { English, Chinese }
    translations_ = {
        // File menu
        { "File",          { "&File",          "文件(&F)" } },
        { "Open Image…",   { "Open Image…",    "打开图像…" } },
        { "Print…",        { "Print…",         "打印…" } },
        { "Quit",          { "&Quit",          "退出(&Q)" } },

        // Edit menu
        { "Edit",          { "&Edit",          "编辑(&E)" } },
        { "Undo",          { "Undo",           "撤销" } },
        { "Redo",          { "Redo",           "重做" } },
        { "Cut",           { "Cut",            "剪切" } },
        { "Copy",          { "Copy",           "复制" } },
        { "Paste",         { "Paste",         "粘贴" } },

        // View menu
        { "View",          { "&View",          "视图(&V)" } },
        { "Toggle Inspector", { "Toggle Inspector", "切换检查器" } },
        { "Toggle Sidebar",   { "Toggle Sidebar",   "切换侧边栏" } },

        // Tools menu
        { "Tools",         { "&Tools",         "工具(&T)" } },
        { "Measure Target (i1Pro3)…", { "Measure Target (i1Pro3)…", "测量目标 (i1Pro3)…" } },
        { "Settings…",     { "Settings…",      "设置…" } },

        // Help menu
        { "Help",          { "&Help",          "帮助(&H)" } },
        { "About jcolour", { "About jcolour",  "关于 jcolour" } },

        // Inspector tabs
        { "Input",         { "Input",          "输入" } },
        { "Color",         { "Color",          "色彩" } },
        { "Print",         { "Print",          "打印" } },
        { "Finishing",     { "Finishing",      "印后加工" } },
        { "System",        { "System",         "色彩校正" } },

        // Color sub-tabs
        { "Adjust",        { "Adjust",         "调整" } },
        { "Mgmt",          { "Mgmt",           "管理" } },
        { "B&W",           { "B&W",            "黑白" } },
        { "Curve",         { "Curve",          "曲线" } },

        // Inspector labels
        { "Image",               { "Image",               "图像" } },
        { "Source profile",      { "Source profile",      "源配置文件" } },
        { "Channel corrections", { "Channel corrections", "通道校正" } },
        { "Brightness / contrast / saturation", { "Brightness / contrast / saturation", "亮度 / 对比度 / 饱和度" } },
        { "Sharpen / soften",    { "Sharpen / soften",    "锐化 / 柔化" } },
        { "Output ICC profile",  { "Output ICC profile",  "输出 ICC 配置文件" } },
        { "Rendering intent",   { "Rendering intent",    "渲染意图" } },
        { "Soft proofing",       { "Soft proofing",       "软打样" } },
        { "Paper",               { "Paper",               "纸张" } },
        { "Margins",             { "Margins",             "边距" } },
        { "Position",            { "Position",            "位置" } },
        { "Printer",             { "Printer",             "打印机" } },
        { "Edition",             { "Edition",             "版本" } },
        { "Contact sheet",       { "Contact sheet",       "联系单" } },
        { "Calibration",         { "Calibration",         "校准" } },
        { "Inks",                { "Inks",                "墨水" } },
        { "Profiles",            { "Profiles",            "配置文件" } },
        { "Subscription",       { "Subscription",       "订阅" } },
        { "Reset",              { "Reset",              "重置" } },

        // Settings dialog
        { "Settings",            { "Settings",            "设置" } },
        { "Language",            { "Language",            "语言" } },
        { "English",             { "English",             "English" } },
        { "简体中文",            { "Simplified Chinese",  "简体中文" } },
        { "Apply",               { "Apply",              "应用" } },
        { "Cancel",              { "Cancel",             "取消" } },
        { "OK",                  { "OK",                 "确定" } },

        // Toolbar
        { "Open",                { "Open",                "打开" } },
        { "Add",                 { "Add",                 "添加" } },
        { "Print",               { "Print",               "打印" } },
        { "Hold",                { "Hold",                "暂停" } },
        { "Cancel",              { "Cancel",              "取消" } },
        { "Approve",             { "Approve",             "批准" } },
        { "Reject",              { "Reject",              "拒绝" } },
        { "Job Explorer",        { "Job Explorer",        "作业浏览器" } },
        { "System Manager",      { "System Manager",      "流程设定" } },

        // Status bar
        { "Total",               { "Total",               "总计" } },
        { "Held",                { "Held",                "暂停" } },
        { "Printing",            { "Printing",            "打印中" } },
        { "Printer",             { "Printer",             "打印机" } },
        { "Connected",           { "Connected",           "已连接" } },
        { "Disconnected",        { "Disconnected",        "已断开" } },
        { "Warning",             { "Warning",             "警告" } },

        // Workflow sidebar
        { "Group by",            { "Group by",            "分组依据" } },
        { "All Jobs",             { "All Jobs",             "所有作业" } },

        // Messages
        { "No Job Selected",      { "No Job Selected",      "未选择作业" } },
        { "Select a job in the table below before printing.", { "请先在下方表格中选择一个作业后再打印。", } },
        { "Image loaded:",        { "Image loaded:",        "图像已加载：" } },
        { "Sent to",              { "Sent to",              "已发送至" } },
        { "Waiting for i1Pro3…", { "Waiting for i1Pro3…", "正在等待 i1Pro3…" } },

        // Input tab options
        { "Embedded (auto)",      { "Embedded (auto)",      "嵌入（自动）" } },
        { "sRGB IEC61966-2.1",    { "sRGB IEC61966-2.1",    "sRGB IEC61966-2.1" } },
        { "Adobe RGB (1998)",      { "Adobe RGB (1998)",      "Adobe RGB (1998)" } },
        { "ProPhoto RGB",         { "ProPhoto RGB",         "ProPhoto RGB" } },
        { "Display P3",           { "Display P3",           "Display P3" } },

        // Rendering intents
        { "Perceptual",               { "Perceptual",               "可感知" } },
        { "Relative Colorimetric",    { "Relative Colorimetric",     "相对比色" } },
        { "Saturation",               { "Saturation",               "饱和度" } },
        { "Absolute Colorimetric",    { "Absolute Colorimetric",    "绝对比色" } },

        // Paper sizes
        { "A4",                 { "A4",                 "A4" } },
        { "A3",                 { "A3",                 "A3" } },
        { "A3+",                { "A3+",                "A3+" } },
        { "A2",                 { "A2",                 "A2" } },
        { "17×22",              { "17×22",              "17×22" } },
        { "24×36",              { "24×36",              "24×36" } },
        { "Custom",             { "Custom",             "自定义" } },

        // Margin labels
        { "Top",                { "Top",                "上" } },
        { "Right",              { "Right",              "右" } },
        { "Bottom",             { "Bottom",             "下" } },
        { "Left",               { "Left",               "左" } },
        { "X",                  { "X",                  "X" } },
        { "Y",                  { "Y",                  "Y" } },
        { "Rot",                { "Rot",                "旋转" } },
        { "W",                  { "W",                  "宽" } },
        { "H",                  { "H",                  "高" } },
        { "Copies",             { "Copies",             "份数" } },

        // Buttons
        { "Nesting…",           { "Nesting…",           "嵌套…" } },
        { "Test print…",        { "Test print…",        "试印…" } },
        { "Open edition manager…", { "Open edition manager…", "打开版本管理器…" } },
        { "Generate contact sheet…", { "Generate contact sheet…", "生成联系单…" } },
        { "G7 calibration wizard…", { "G7 calibration wizard…", "G7 校准向导…" } },
        { "Monitor calibration check…", { "Monitor calibration check…", "显示器校准检查…" } },
        { "Manage ink cartridges…", { "Manage ink cartridges…", "管理墨盒…" } },
        { "Open profile library…", { "Open profile library…", "打开配置文件库…" } },
        { "Manage subscription…", { "Manage subscription…", "管理订阅…" } },
        { "Browse…",            { "Browse…",            "浏览…" } },
        { "Black Point Compensation", { "Black Point Compensation", "黑点补偿" } },

        // Status
        { "Free trial · 14 days remaining", { "Free trial · 14 days remaining", "免费试用 · 剩余 14 天" } },
        { "no image loaded",          { "(no image loaded)",         "（未加载图像）" } },

        // Device Center
        { "Device Center",           { "Device Center",              "设备中心" } },
        { "Devices",                 { "Devices",                    "设备" } },
        { "Add",                     { "Add",                        "添加" } },
        { "Discover",                { "Discover",                   "发现" } },
        { "Ready",                   { "Ready",                      "就绪" } },
        { "Offline",                 { "Offline",                    "离线" } },
        { "Printing",                { "Printing",                   "打印中" } },
        { "Warning",                 { "Warning",                   "警告" } },
        { "IP",                      { "IP",                         "IP地址" } },
        { "Connection",              { "Connection",                 "连接" } },
        { "TCP/IP",                  { "TCP/IP",                    "TCP/IP" } },
        { "USB",                     { "USB",                        "USB" } },
        { "Ink Mode",                { "Ink Mode",                   "墨水模式" } },
        { "Photo Black",             { "Photo Black",               "照片黑" } },
        { "Matte Black",             { "Matte Black",               "哑光黑" } },
        { "Resolution",              { "Resolution",                 "分辨率" } },
        { "dpi",                     { "dpi",                        "dpi" } },
        { "Ink Levels",              { "Ink Levels",                 "墨水余量" } },
        { "Workflow",                { "Workflow",                   "工作流程" } },
        { "B&W Multi-Channel",       { "B&W Multi-Channel",         "黑白多通道" } },
        { "B&W Single Black",        { "B&W Single Black",          "黑白单黑" } },
        { "Color Photo",             { "Color Photo",                "彩色照片" } },
        { "Ink Curves",              { "Ink Curves",                 "墨水曲线" } },
        { "Input",                   { "Input",                       "输入" } },
        { "Output",                  { "Output",                     "输出" } },
        { "Brightness",              { "Brightness",                 "亮度" } },
        { "Reset",                   { "Reset",                       "重置" } },
        { "Test Print",              { "Test Print",                  "试印" } },
    };
}

QString Translator::translate(const char* key) const {
    QString k(key);
    if (translations_.contains(k)) {
        int idx = (currentLang_ == Language::English) ? 0 : 1;
        if (idx < translations_[k].size()) {
            return translations_[k][idx];
        }
    }
    return key;
}

void Translator::setLanguage(Language lang) {
    if (currentLang_ != lang) {
        currentLang_ = lang;
        emit languageChanged();
    }
}

} // namespace jcolour
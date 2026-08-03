#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

// Lightweight vector icon set (Lucide-style stroked line icons), painted with
// QPainter so we depend on nothing beyond Qt Gui. Replaces the emoji / random
// Unicode glyphs that used to stand in for toolbar + sidebar icons.

namespace jcolour {
namespace ic {

enum class Icon {
    Printer,
    Sliders,      // 建檔校準
    Clock,        // 記錄
    Settings,     // 設定 / gear
    FilePlus,     // 新建
    FolderOpen,   // 打开
    Save,         // 保存
    Undo,
    Redo,
    ImagePlus,    // 导入图片
    Eye,          // 软打样
    AlertTriangle,// 色域警告
    Grid,         // 自动拼版
    Minus,        // 缩小
    Plus,         // 放大
};

// Single-colour pixmap rendered in a 24×24 coordinate space, scaled to `px`
// logical pixels and tagged with `dpr` for crisp HiDPI output.
QPixmap pixmap(Icon name, const QColor& color, int px, qreal dpr = 1.0);

// One-state icon (toolbar buttons).
QIcon icon(Icon name, const QColor& color, int px = 18);

// Two-state icon: `off` when unchecked, `on` when the button is checked.
// Used by the sidebar nav (gray → white) and checkable toolbar toggles.
QIcon toggleIcon(Icon name, const QColor& off, const QColor& on, int px = 18);

}  // namespace ic
}  // namespace jcolour

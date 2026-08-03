#pragma once

#include <QString>

namespace jcolour::theme {

// Returns the global QSS stylesheet for the macOS dark theme.
QString darkStyleSheet();

// Apply Fusion style + dark palette + stylesheet to the application.
void apply();

// Shared "new look" page stylesheet (design/ui-mockup.html tokens: white cards,
// #d2d5d9 borders, #2e7dd7 accent, rounded controls). Apply on a page root to
// bring its child controls in line with the 列印 / 建檔校準 pages. Style the
// primary action button by setObjectName("primary").
QString pageStyle();

} // namespace jcolour::theme

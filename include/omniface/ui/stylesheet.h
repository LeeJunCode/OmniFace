#pragma once

#include <QString>

namespace omniface::ui {

inline QString AppStyleSheet() {
    return QStringLiteral(
        "QMainWindow { background-color: #f5f5f5; }"
        "QWidget { color: #333333; font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif; font-size: 13px; }"
        "QDialog { background-color: #ffffff; }"
        "QMessageBox { background-color: #ffffff; }"
        "QMessageBox QLabel { color: #333333; font-size: 13px; background: transparent; }"
        "QMessageBox QPushButton { min-width: 80px; min-height: 28px; }"
        "QPushButton { background-color: #ffffff; border: 1px solid #c0c0c0; border-radius: 4px; padding: 6px 14px; color: #333333; min-height: 28px; }"
        "QPushButton:hover { background-color: #e8e8e8; border-color: #a0a0a0; }"
        "QPushButton:pressed { background-color: #d0d0d0; }"
        "QPushButton:disabled { background-color: #f0f0f0; color: #b0b0b0; border-color: #d8d8d8; }"
        "QPushButton#playBtn { background-color: #e8f5e9; border-color: #4caf50; color: #2e7d32; }"
        "QPushButton#playBtn:hover { background-color: #c8e6c9; }"
        "QPushButton#playBtn:disabled { background-color: #f0f0f0; color: #b0b0b0; border-color: #d8d8d8; }"
        "QPushButton#stopBtn { background-color: #ffebee; border-color: #e53935; color: #c62828; }"
        "QPushButton#stopBtn:hover { background-color: #ffcdd2; }"
        "QPushButton#stopBtn:disabled { background-color: #f0f0f0; color: #b0b0b0; border-color: #d8d8d8; }"
        "QPushButton#replayBtn { background-color: #e3f2fd; border-color: #42a5f5; color: #1565c0; }"
        "QPushButton#replayBtn:hover { background-color: #bbdefb; }"
        "QPushButton#replayBtn:disabled { background-color: #f0f0f0; color: #b0b0b0; border-color: #d8d8d8; }"
        "QLabel { background: transparent; border: none; }"
        "QLabel#videoLabel { background-color: #fafafa; border: 2px solid #d0d0d0; border-radius: 8px; color: #999999; font-size: 15px; }"
        "QLabel#refThumbnail { background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px; color: #999999; font-size: 12px; }"
        "QStatusBar { background-color: #f0f0f0; border-top: 1px solid #d0d0d0; color: #666666; }"
        "QScrollArea { border: none; background: transparent; }"
        "QMenuBar { background-color: #fafafa; border-bottom: 1px solid #e0e0e0; padding: 2px; }"
        "QMenuBar::item { padding: 4px 12px; border-radius: 4px; }"
        "QMenuBar::item:selected { background-color: #e0e0e0; }"
        "QMenu { background-color: #ffffff; border: 1px solid #d0d0d0; padding: 4px; }"
        "QMenu::item { padding: 6px 28px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #e3f2fd; }"
        "QMenu::separator { height: 1px; background: #e0e0e0; margin: 4px 8px; }"
        "QProgressBar#processingProgress { background-color: #e0e0e0; border: none; border-radius: 2px; }"
        "QProgressBar#processingProgress::chunk { background-color: #42a5f5; border-radius: 2px; }"
        "QSplitter::handle { background-color: #d0d0d0; }"
        "QSplitter::handle:hover { background-color: #b0b0b0; }"
        "QSplitter::handle:horizontal { width: 4px; }"
        "QSplitter::handle:vertical { height: 3px; }"
    );
}

inline QString ConfirmDialogStyleSheet() {
    return QStringLiteral(
        "QMessageBox { background-color: #ffffff; }"
        "QLabel { color: #333333; font-size: 13px; background: transparent; border: none; }"
        "QPushButton { min-width: 80px; min-height: 28px; background-color: #ffffff; "
        "border: 1px solid #c0c0c0; border-radius: 4px; padding: 6px 14px; color: #333333; }"
        "QPushButton:hover { background-color: #e8e8e8; }");
}

}  // namespace omniface::ui

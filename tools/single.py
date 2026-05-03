#!/usr/bin/env python3
"""
Zen IDE — editor para a linguagem Zen/BuLang
Requer: PyQt6  (pip install PyQt6)
Uso:    python zen_ide.py [--zen /caminho/para/zen]
"""

import sys
import os
import subprocess
import shlex
import argparse
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QTextEdit, QPlainTextEdit, QToolBar, QStatusBar,
    QFileDialog, QLabel, QLineEdit, QDialog, QDialogButtonBox,
    QFormLayout, QCheckBox, QMessageBox, QTabWidget, QFrame,
    QComboBox, QSizePolicy
)
from PyQt6.QtGui import (
    QFont, QColor, QTextCharFormat, QSyntaxHighlighter,
    QTextDocument, QIcon, QPalette, QAction, QKeySequence,
    QTextCursor, QPainter
)
from PyQt6.QtCore import (
    Qt, QThread, QObject, pyqtSignal, QRegularExpression,
    QRect, QSize, QProcess, QSettings
)

# ──────────────────────────────────────────────────────────────
# TEMA  (dark, inspirado no VS Code)
# ──────────────────────────────────────────────────────────────
THEME = {
    "bg":           "#1e1e2e",
    "bg_panel":     "#181825",
    "bg_line":      "#313244",
    "fg":           "#cdd6f4",
    "fg_dim":       "#6c7086",
    "keyword":      "#cba6f7",   # roxo
    "builtin":      "#89b4fa",   # azul
    "string":       "#a6e3a1",   # verde
    "number":       "#fab387",   # laranja
    "comment":      "#585b70",   # cinzento
    "operator":     "#89dceb",   # ciano
    "type":         "#f9e2af",   # amarelo
    "func_def":     "#89b4fa",   # azul
    "selection":    "#45475a",
    "cursor":       "#f5c2e7",
    "border":       "#313244",
    "toolbar_bg":   "#11111b",
    "run_btn":      "#a6e3a1",
    "run_fg":       "#1e1e2e",
    "stop_btn":     "#f38ba8",
    "output_ok":    "#a6e3a1",
    "output_err":   "#f38ba8",
    "output_info":  "#89b4fa",
    "output_warn":  "#f9e2af",
}

STYLESHEET = f"""
QMainWindow, QWidget {{
    background-color: {THEME['bg']};
    color: {THEME['fg']};
    font-family: 'Consolas', 'JetBrains Mono', 'Fira Code', monospace;
}}
QToolBar {{
    background-color: {THEME['toolbar_bg']};
    border-bottom: 1px solid {THEME['border']};
    spacing: 4px;
    padding: 4px 8px;
}}
QToolBar QToolButton {{
    background-color: transparent;
    color: {THEME['fg']};
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px 10px;
    font-size: 12px;
}}
QToolBar QToolButton:hover {{
    background-color: {THEME['bg_line']};
    border-color: {THEME['border']};
}}
QToolBar QToolButton:pressed {{
    background-color: {THEME['selection']};
}}
QToolBar QLabel {{
    color: {THEME['fg_dim']};
    padding: 0 4px;
    font-size: 11px;
}}
QLineEdit {{
    background-color: {THEME['bg_panel']};
    color: {THEME['fg']};
    border: 1px solid {THEME['border']};
    border-radius: 4px;
    padding: 3px 8px;
    font-family: 'Consolas', monospace;
    font-size: 12px;
    selection-background-color: {THEME['selection']};
}}
QLineEdit:focus {{
    border-color: {THEME['builtin']};
}}
QSplitter::handle {{
    background-color: {THEME['border']};
}}
QTabWidget::pane {{
    border: 1px solid {THEME['border']};
    background-color: {THEME['bg']};
}}
QTabBar::tab {{
    background-color: {THEME['bg_panel']};
    color: {THEME['fg_dim']};
    border: 1px solid {THEME['border']};
    border-bottom: none;
    padding: 5px 14px;
    margin-right: 2px;
    font-size: 12px;
}}
QTabBar::tab:selected {{
    background-color: {THEME['bg']};
    color: {THEME['fg']};
    border-top: 2px solid {THEME['builtin']};
}}
QStatusBar {{
    background-color: {THEME['toolbar_bg']};
    color: {THEME['fg_dim']};
    border-top: 1px solid {THEME['border']};
    font-size: 11px;
    padding: 2px 8px;
}}
QScrollBar:vertical {{
    background-color: {THEME['bg_panel']};
    width: 10px;
    border: none;
}}
QScrollBar::handle:vertical {{
    background-color: {THEME['bg_line']};
    border-radius: 5px;
    min-height: 20px;
}}
QScrollBar::handle:vertical:hover {{
    background-color: {THEME['selection']};
}}
QScrollBar:horizontal {{
    background-color: {THEME['bg_panel']};
    height: 10px;
    border: none;
}}
QScrollBar::handle:horizontal {{
    background-color: {THEME['bg_line']};
    border-radius: 5px;
}}
QMessageBox {{
    background-color: {THEME['bg']};
}}
QDialog {{
    background-color: {THEME['bg']};
}}
QFormLayout QLabel {{
    color: {THEME['fg']};
}}
QCheckBox {{
    color: {THEME['fg']};
    spacing: 6px;
}}
QCheckBox::indicator {{
    width: 14px; height: 14px;
    border: 1px solid {THEME['border']};
    border-radius: 3px;
    background-color: {THEME['bg_panel']};
}}
QCheckBox::indicator:checked {{
    background-color: {THEME['builtin']};
}}
QComboBox {{
    background-color: {THEME['bg_panel']};
    color: {THEME['fg']};
    border: 1px solid {THEME['border']};
    border-radius: 4px;
    padding: 3px 8px;
}}
QComboBox::drop-down {{
    border: none;
    width: 20px;
}}
"""

# ──────────────────────────────────────────────────────────────
# SYNTAX HIGHLIGHTER — Zen/BuLang
# ──────────────────────────────────────────────────────────────
class ZenHighlighter(QSyntaxHighlighter):
    def __init__(self, document):
        super().__init__(document)
        self._rules = []

        def fmt(color, bold=False, italic=False):
            f = QTextCharFormat()
            f.setForeground(QColor(color))
            if bold:   f.setFontWeight(700)
            if italic: f.setFontItalic(True)
            return f

        kw = fmt(THEME["keyword"], bold=True)
        bi = fmt(THEME["builtin"])
        st = fmt(THEME["string"])
        nm = fmt(THEME["number"])
        cm = fmt(THEME["comment"], italic=True)
        op = fmt(THEME["operator"])
        ty = fmt(THEME["type"], bold=True)
        fn = fmt(THEME["func_def"])

        # keywords
        keywords = (
            "var if elif clock else while for loop break continue return import "
            "struct class def process self father son true false nil null "
            "frame and or not in do yield spawn resume"
        ).split()
        for kw_str in keywords:
            self._rules.append((
                QRegularExpression(r'\b' + kw_str + r'\b'), kw
            ))

        # types / buffer types
        types = (
            "Int8Array Int16Array Int32Array Uint8Array Uint16Array "
            "Uint32Array Float32Array Float64Array"
        ).split()
        for t in types:
            self._rules.append((QRegularExpression(r'\b' + t + r'\b'), ty))

        # built-ins
        builtins = (
            "print len int float bool push pop sin cos tan sqrt pow abs "
            "floor ceil log exp atan2 deg rad clock"
        ).split()
        for b in builtins:
            self._rules.append((QRegularExpression(r'\b' + b + r'(?=\s*\()'), bi))

        # função def nome(
        self._rules.append((
            QRegularExpression(r'\bdef\s+(\w+)\s*\('),
            fn  # aplica ao grupo 0 (linha toda) — refina abaixo
        ))

        # números: hex, float, int
        self._rules.append((QRegularExpression(r'\b0x[0-9a-fA-F]+\b'), nm))
        self._rules.append((QRegularExpression(r'\b\d+\.\d*([eE][+-]?\d+)?\b'), nm))
        self._rules.append((QRegularExpression(r'\b\d+([eE][+-]?\d+)?\b'), nm))

        # strings com escape
        self._rules.append((QRegularExpression(r'"(?:[^"\\]|\\.)*"'), st))
        # verbatim @"..."
        self._rules.append((QRegularExpression(r'@"(?:[^"]|"")*"'), st))

        # operadores
        self._rules.append((
            QRegularExpression(r'[+\-*/%&|^~<>!=]+|<<|>>|&&|\|\|'), op
        ))

        # comentário linha
        self._rules.append((QRegularExpression(r'//[^\n]*'), cm))

        # comentário bloco (tratado em highlightBlock)
        self._comment_start = QRegularExpression(r'/\*')
        self._comment_end   = QRegularExpression(r'\*/')
        self._comment_fmt   = cm

    def highlightBlock(self, text):
        # regras simples
        for pattern, fmt in self._rules:
            it = pattern.globalMatch(text)
            while it.hasNext():
                m = it.next()
                self.setFormat(m.capturedStart(), m.capturedLength(), fmt)

        # comentários bloco multi-linha
        self.setCurrentBlockState(0)
        start = 0
        if self.previousBlockState() != 1:
            m = self._comment_start.match(text)
            start = m.capturedStart() if m.hasMatch() else -1

        while start >= 0:
            m_end = self._comment_end.match(text, start)
            if m_end.hasMatch():
                length = m_end.capturedStart() - start + m_end.capturedLength()
                self.setFormat(start, length, self._comment_fmt)
                m2 = self._comment_start.match(text, start + length)
                start = m2.capturedStart() if m2.hasMatch() else -1
            else:
                self.setCurrentBlockState(1)
                self.setFormat(start, len(text) - start, self._comment_fmt)
                break

# ──────────────────────────────────────────────────────────────
# EDITOR COM NÚMEROS DE LINHA
# ──────────────────────────────────────────────────────────────
class LineNumberArea(QWidget):
    def __init__(self, editor):
        super().__init__(editor)
        self.editor = editor

    def sizeHint(self):
        return QSize(self.editor._line_number_width(), 0)

    def paintEvent(self, event):
        self.editor._paint_line_numbers(event)


class CodeEditor(QPlainTextEdit):
    def __init__(self):
        super().__init__()
        self._line_area = LineNumberArea(self)

        font = QFont("Consolas", 12)
        font.setFixedPitch(True)
        self.setFont(font)

        # cores
        pal = self.palette()
        pal.setColor(QPalette.ColorRole.Base,            QColor(THEME["bg"]))
        pal.setColor(QPalette.ColorRole.Text,            QColor(THEME["fg"]))
        pal.setColor(QPalette.ColorRole.Highlight,       QColor(THEME["selection"]))
        pal.setColor(QPalette.ColorRole.HighlightedText, QColor(THEME["fg"]))
        self.setPalette(pal)

        self.setTabStopDistance(
            self.fontMetrics().horizontalAdvance(' ') * 4
        )
        self.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)

        self.blockCountChanged.connect(self._update_line_area_width)
        self.updateRequest.connect(self._update_line_area)
        self.cursorPositionChanged.connect(self._highlight_current_line)

        self._update_line_area_width(0)
        self._highlight_current_line()

        self._highlighter = ZenHighlighter(self.document())

    def _line_number_width(self):
        digits = max(3, len(str(self.blockCount())))
        return 10 + self.fontMetrics().horizontalAdvance('9') * digits

    def _update_line_area_width(self, _):
        self.setViewportMargins(self._line_number_width(), 0, 0, 0)

    def _update_line_area(self, rect, dy):
        if dy:
            self._line_area.scroll(0, dy)
        else:
            self._line_area.update(0, rect.y(), self._line_area.width(), rect.height())
        if rect.contains(self.viewport().rect()):
            self._update_line_area_width(0)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        cr = self.contentsRect()
        self._line_area.setGeometry(QRect(cr.left(), cr.top(), self._line_number_width(), cr.height()))

    def _paint_line_numbers(self, event):
        painter = QPainter(self._line_area)
        painter.fillRect(event.rect(), QColor(THEME["bg_panel"]))

        block  = self.firstVisibleBlock()
        num    = block.blockNumber()
        top    = round(self.blockBoundingGeometry(block).translated(self.contentOffset()).top())
        bottom = top + round(self.blockBoundingRect(block).height())
        fh     = self.fontMetrics().height()

        painter.setFont(self.font())

        while block.isValid() and top <= event.rect().bottom():
            if block.isVisible() and bottom >= event.rect().top():
                is_cur = (num == self.textCursor().blockNumber())
                painter.setPen(QColor(THEME["fg"] if is_cur else THEME["fg_dim"]))
                painter.drawText(
                    0, top, self._line_number_width() - 6, fh,
                    Qt.AlignmentFlag.AlignRight, str(num + 1)
                )
            block  = block.next()
            top    = bottom
            bottom = top + round(self.blockBoundingRect(block).height())
            num   += 1

    def _highlight_current_line(self):
        sel = QTextEdit.ExtraSelection()
        sel.format.setBackground(QColor(THEME["bg_line"]))
        sel.format.setProperty(QTextCharFormat.Property.FullWidthSelection, True)
        sel.cursor = self.textCursor()
        sel.cursor.clearSelection()
        self.setExtraSelections([sel])

    def keyPressEvent(self, e):
        # auto-indent
        if e.key() == Qt.Key.Key_Return:
            cursor = self.textCursor()
            block  = cursor.block().text()
            indent = len(block) - len(block.lstrip())
            if block.rstrip().endswith('{'):
                indent += 4
            super().keyPressEvent(e)
            self.insertPlainText(' ' * indent)
            return
        # tab → 4 espaços
        if e.key() == Qt.Key.Key_Tab:
            self.insertPlainText('    ')
            return
        super().keyPressEvent(e)


# ──────────────────────────────────────────────────────────────
# PAINEL DE OUTPUT
# ──────────────────────────────────────────────────────────────
class OutputPanel(QTextEdit):
    def __init__(self):
        super().__init__()
        self.setReadOnly(True)
        font = QFont("Consolas", 11)
        self.setFont(font)
        pal = self.palette()
        pal.setColor(QPalette.ColorRole.Base, QColor(THEME["bg_panel"]))
        pal.setColor(QPalette.ColorRole.Text, QColor(THEME["fg"]))
        self.setPalette(pal)
        self.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)

    def _append(self, text, color):
        fmt = QTextCharFormat()
        fmt.setForeground(QColor(color))
        cursor = self.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        cursor.insertText(text + "\n", fmt)
        self.setTextCursor(cursor)
        self.ensureCursorVisible()

    def info(self, text):    self._append(text, THEME["output_info"])
    def ok(self, text):      self._append(text, THEME["output_ok"])
    def error(self, text):   self._append(text, THEME["output_err"])
    def warn(self, text):    self._append(text, THEME["output_warn"])
    def plain(self, text):   self._append(text, THEME["fg"])

    def clear_output(self):
        self.clear()


# ──────────────────────────────────────────────────────────────
# WORKER — executa zen num QProcess
# ──────────────────────────────────────────────────────────────
class ZenRunner(QObject):
    stdout_line = pyqtSignal(str)
    stderr_line = pyqtSignal(str)
    finished    = pyqtSignal(int)

    def __init__(self):
        super().__init__()
        self._proc = None

    def run(self, cmd: list[str]):
        self._proc = QProcess()
        self._proc.readyReadStandardOutput.connect(self._on_stdout)
        self._proc.readyReadStandardError.connect(self._on_stderr)
        self._proc.finished.connect(self._on_finished)
        self._proc.start(cmd[0], cmd[1:])

    def kill(self):
        if self._proc and self._proc.state() != QProcess.ProcessState.NotRunning:
            self._proc.kill()

    def _on_stdout(self):
        data = self._proc.readAllStandardOutput().data().decode("utf-8", errors="replace")
        for line in data.splitlines():
            self.stdout_line.emit(line)

    def _on_stderr(self):
        data = self._proc.readAllStandardError().data().decode("utf-8", errors="replace")
        for line in data.splitlines():
            self.stderr_line.emit(line)

    def _on_finished(self, code, _status):
        self.finished.emit(code)


# ──────────────────────────────────────────────────────────────
# DIÁLOGO DE OPÇÕES DE EXECUÇÃO
# ──────────────────────────────────────────────────────────────
class RunOptionsDialog(QDialog):
    def __init__(self, parent, opts):
        super().__init__(parent)
        self.setWindowTitle("Opções de execução")
        self.setModal(True)
        self.setMinimumWidth(380)

        layout = QVBoxLayout(self)
        form   = QFormLayout()

        self.chk_dis     = QCheckBox("Mostrar disassembly (--dis)")
        self.chk_dis_only= QCheckBox("Só disassembly, não executa (--dis-only)")
        self.chk_verbose = QCheckBox("Verbose (-v)")
        self.chk_strip   = QCheckBox("Strip debug (--strip-debug)")
        self.txt_dump    = QLineEdit()
        self.txt_dump.setPlaceholderText("deixar vazio para não fazer dump")
        self.txt_include = QLineEdit()
        self.txt_include.setPlaceholderText("-I /caminho/includes")
        self.txt_args    = QLineEdit()
        self.txt_args.setPlaceholderText("argumentos para o script (args[])")

        self.chk_dis.setChecked(opts.get("dis", False))
        self.chk_dis_only.setChecked(opts.get("dis_only", False))
        self.chk_verbose.setChecked(opts.get("verbose", False))
        self.chk_strip.setChecked(opts.get("strip", False))
        self.txt_dump.setText(opts.get("dump", ""))
        self.txt_include.setText(opts.get("include", ""))
        self.txt_args.setText(opts.get("script_args", ""))

        form.addRow(self.chk_dis)
        form.addRow(self.chk_dis_only)
        form.addRow(self.chk_verbose)
        form.addRow(self.chk_strip)
        form.addRow("Dump bytecode:", self.txt_dump)
        form.addRow("Include path:", self.txt_include)
        form.addRow("Args do script:", self.txt_args)

        btns = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)

        layout.addLayout(form)
        layout.addWidget(btns)

    def get_opts(self):
        return {
            "dis":         self.chk_dis.isChecked(),
            "dis_only":    self.chk_dis_only.isChecked(),
            "verbose":     self.chk_verbose.isChecked(),
            "strip":       self.chk_strip.isChecked(),
            "dump":        self.txt_dump.text().strip(),
            "include":     self.txt_include.text().strip(),
            "script_args": self.txt_args.text().strip(),
        }


# ──────────────────────────────────────────────────────────────
# JANELA PRINCIPAL
# ──────────────────────────────────────────────────────────────
class ZenIDE(QMainWindow):
    def __init__(self, zen_path="zen"):
        super().__init__()
        self._settings   = QSettings("ZenVM", "ZenIDE")
        self._zen_path   = zen_path
        self._file_path  = None
        self._modified   = False
        self._runner     = ZenRunner()
        self._run_opts   = {}

        self._runner.stdout_line.connect(self._on_stdout)
        self._runner.stderr_line.connect(self._on_stderr)
        self._runner.finished.connect(self._on_finished)

        self.setWindowTitle("Zen IDE")
        self.resize(1200, 750)
        self.setStyleSheet(STYLESHEET)

        self._build_toolbar()
        self._build_central()
        self._build_statusbar()

        self._new_file()

    # ── Toolbar ──────────────────────────────────────────────
    def _build_toolbar(self):
        tb = QToolBar("Toolbar principal")
        tb.setMovable(False)
        tb.setIconSize(QSize(16, 16))
        self.addToolBar(tb)

        def act(label, shortcut, tip, slot, color=None):
            a = QAction(label, self)
            if shortcut: a.setShortcut(QKeySequence(shortcut))
            a.setStatusTip(tip)
            a.triggered.connect(slot)
            btn = tb.addAction(a)
            return a

        # ── ficheiro
        act("⬜ Novo",    "Ctrl+N", "Novo ficheiro",  self._new_file)
        act("📂 Abrir",   "Ctrl+O", "Abrir ficheiro", self._open_file)
        act("💾 Guardar", "Ctrl+S", "Guardar",        self._save_file)
        act("💾 Guardar Como", "Ctrl+Shift+S", "Guardar como", self._save_as)

        tb.addSeparator()

        # ── execução
        self._act_run  = act("▶  Executar",    "F5",    "Executar script",        self._run)
        self._act_run_e= act("⚡ Executar -e", "F6",    "Executar como -e inline", self._run_inline)
        self._act_stop = act("■  Parar",       "F7",    "Parar execução",         self._stop)
        self._act_stop.setEnabled(False)

        tb.addSeparator()

        # ── zen opções directas
        act("🔍 Disassembly",  "F8",  "Mostrar bytecode",                   self._run_dis)
        act("📦 Dump .znb",    "F9",  "Gerar bytecode .znb ao lado do script", self._run_dump)
        act("📋 REPL",         "F10", "Abrir REPL no terminal",             self._open_repl)

        tb.addSeparator()

        # ── zen path
        tb.addWidget(QLabel("zen:"))
        self._zen_edit = QLineEdit(self._zen_path)
        self._zen_edit.setFixedWidth(200)
        self._zen_edit.setToolTip("Caminho para o executável zen")
        self._zen_edit.textChanged.connect(lambda t: setattr(self, '_zen_path', t))
        tb.addWidget(self._zen_edit)

        act("⚙", None, "Opções de execução", self._show_run_opts)

        tb.addSeparator()
        act("🗑 Limpar output", None, "Limpar painel de output", self._clear_output)

    # ── Layout central ───────────────────────────────────────
    def _build_central(self):
        splitter = QSplitter(Qt.Orientation.Vertical)

        # editor
        self._editor = CodeEditor()
        self._editor.document().contentsChanged.connect(self._on_modified)

        # tabs de output
        self._tabs = QTabWidget()
        self._output   = OutputPanel()
        self._dis_view = OutputPanel()

        self._tabs.addTab(self._output,   "Output")
        self._tabs.addTab(self._dis_view, "Disassembly")

        splitter.addWidget(self._editor)
        splitter.addWidget(self._tabs)
        splitter.setSizes([480, 220])

        self.setCentralWidget(splitter)

    # ── Statusbar ────────────────────────────────────────────
    def _build_statusbar(self):
        self._status_file  = QLabel("novo ficheiro")
        self._status_pos   = QLabel("Ln 1, Col 1")
        self._status_zen   = QLabel("")
        sb = self.statusBar()
        sb.addWidget(self._status_file, 1)
        sb.addWidget(self._status_pos)
        sb.addWidget(self._status_zen)
        self._editor.cursorPositionChanged.connect(self._update_pos)

    def _update_pos(self):
        c = self._editor.textCursor()
        self._status_pos.setText(f"Ln {c.blockNumber()+1}, Col {c.columnNumber()+1}")

    def _last_dialog_dir(self):
        if self._file_path:
            return os.path.dirname(self._file_path)
        saved = self._settings.value("last_dir", "", type=str)
        return saved or os.getcwd()

    def _remember_dialog_dir(self, path):
        if not path:
            return
        directory = path if os.path.isdir(path) else os.path.dirname(path)
        if directory:
            self._settings.setValue("last_dir", directory)

    # ── Ficheiro ─────────────────────────────────────────────
    def _new_file(self):
        if not self._check_save(): return
        self._editor.setPlainText(
            '// Novo script Zen\n\nprint("Olá, Mundo!");\n'
        )
        self._file_path = None
        self._modified  = False
        self._update_title()

    def _open_file(self):
        if not self._check_save(): return
        path, _ = QFileDialog.getOpenFileName(
            self, "Abrir ficheiro Zen", self._last_dialog_dir(),
            "Zen scripts (*.zen);;Bytecode (*.zenb);;Todos (*)"
        )
        if path:
            try:
                with open(path, "r", encoding="utf-8") as f:
                    self._editor.setPlainText(f.read())
                self._remember_dialog_dir(path)
                self._file_path = path
                self._modified  = False
                self._update_title()
                self._output.info(f"Aberto: {path}")
            except Exception as e:
                QMessageBox.critical(self, "Erro", str(e))

    def _save_file(self):
        if not self._file_path:
            self._save_as()
        else:
            self._do_save(self._file_path)

    def _save_as(self):
        default_path = self._file_path or os.path.join(self._last_dialog_dir(), "novo_script.zen")
        path, _ = QFileDialog.getSaveFileName(
            self, "Guardar como", default_path,
            "Zen scripts (*.zen);;Todos (*)"
        )
        if path:
            self._do_save(path)

    def _do_save(self, path):
        try:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self._editor.toPlainText())
            self._remember_dialog_dir(path)
            self._file_path = path
            self._modified  = False
            self._update_title()
            self._status_zen.setText(f"Guardado: {os.path.basename(path)}")
        except Exception as e:
            QMessageBox.critical(self, "Erro ao guardar", str(e))

    def _on_modified(self):
        if not self._modified:
            self._modified = True
            self._update_title()

    def _update_title(self):
        name = os.path.basename(self._file_path) if self._file_path else "sem título"
        mod  = " •" if self._modified else ""
        self.setWindowTitle(f"Zen IDE — {name}{mod}")
        self._status_file.setText(self._file_path or "novo ficheiro")

    def _check_save(self):
        if not self._modified: return True
        r = QMessageBox.question(
            self, "Guardar alterações?",
            "O ficheiro tem alterações não guardadas. Guardar?",
            QMessageBox.StandardButton.Yes |
            QMessageBox.StandardButton.No  |
            QMessageBox.StandardButton.Cancel
        )
        if r == QMessageBox.StandardButton.Yes:   self._save_file(); return True
        if r == QMessageBox.StandardButton.No:    return True
        return False

    def _prepare_source_file_for_dump(self):
        if not self._file_path:
            QMessageBox.information(
                self,
                "Guardar antes do dump",
                "Guarda o script .zen primeiro para gerar o bytecode .znb na mesma pasta."
            )
            self._save_as()
        elif self._modified:
            self._save_file()

        if not self._file_path or self._modified:
            return None
        return self._file_path

    def _default_dump_path(self, source_path):
        base, _ = os.path.splitext(source_path)
        return base + ".znb"

    # ── Construir comando zen ─────────────────────────────────
    def _build_cmd(self, extra_flags=None, force_file=None, override_dump=None, ignore_dump=False) -> list[str]:
        cmd = [self._zen_path]
        opts = self._run_opts

        if opts.get("dis"):      cmd.append("--dis")
        if opts.get("dis_only"): cmd += ["--dis", "--dis-only"]
        if opts.get("verbose"):  cmd.append("-v")
        if opts.get("strip"):    cmd.append("--strip-debug")
        if override_dump:         cmd += ["--dump", override_dump]
        elif not ignore_dump and opts.get("dump"):
            cmd += ["--dump", opts["dump"]]
        if opts.get("include"):  cmd += ["-I", opts["include"]]

        if extra_flags:
            cmd += extra_flags

        if force_file:
            cmd.append(force_file)
        elif self._file_path:
            cmd.append(self._file_path)
        else:
            # sem ficheiro → inline via -e
            cmd += ["-e", self._editor.toPlainText()]

        if opts.get("script_args"):
            cmd += shlex.split(opts["script_args"])

        return cmd

    # ── Executar ──────────────────────────────────────────────
    def _start_run(self, cmd):
        self._output.clear_output()
        self._output.info(f"$ {' '.join(cmd)}\n")
        self._tabs.setCurrentIndex(0)
        self._act_run.setEnabled(False)
        self._act_stop.setEnabled(True)
        self._status_zen.setText("A executar…")
        self._runner.run(cmd)

    def _run(self):
        if self._modified and self._file_path:
            self._save_file()
        elif not self._file_path:
            # guarda temporário
            import tempfile
            tmp = tempfile.NamedTemporaryFile(suffix=".zen", delete=False, mode="w")
            tmp.write(self._editor.toPlainText())
            tmp.close()
            self._start_run(self._build_cmd(force_file=tmp.name))
            return
        self._start_run(self._build_cmd())

    def _run_inline(self):
        code = self._editor.toPlainText()
        cmd  = [self._zen_path, "-e", code]
        self._start_run(cmd)

    def _run_dis(self):
        if self._modified and self._file_path: self._save_file()
        self._dis_view.clear_output()
        self._tabs.setCurrentIndex(1)

        cmd = self._build_cmd(extra_flags=["----dis-only"])
        self._output.info(f"$ {' '.join(cmd[:3])}…")
        self._act_run.setEnabled(False)
        self._act_stop.setEnabled(True)
        self._status_zen.setText("Disassembly…")

        # redirige para dis_view
        runner = ZenRunner()
        runner.stdout_line.connect(lambda l: self._dis_view.plain(l))
        runner.stderr_line.connect(lambda l: self._dis_view.error(l))
        runner.finished.connect(lambda c: (
            self._act_run.setEnabled(True),
            self._act_stop.setEnabled(False),
            self._status_zen.setText(f"Exit {c}")
        ))
        runner.run(cmd)
        self._extra_runner = runner

    def _run_dump(self):
        if not self._file_path:
            self._save_as()
            if not self._file_path:
                return

        if self._modified:
            self._save_file()

        dump_path = self._default_dump_path(self._file_path)

        cmd = self._build_cmd(
            extra_flags=["--dump", dump_path, "-v"],
            ignore_dump=True
        )

        self._output.info(f"Dump bytecode para: {dump_path}")
        self._start_run(cmd)

    def _open_repl(self):
        # tenta abrir um terminal externo com o REPL
        terminals = [
            ["x-terminal-emulator", "-e"],
            ["xterm", "-e"],
            ["gnome-terminal", "--"],
            ["konsole", "-e"],
        ]
        zen = self._zen_path
        launched = False
        for term in terminals:
            try:
                subprocess.Popen(term + [zen])
                launched = True
                break
            except FileNotFoundError:
                continue
        if not launched:
            self._output.warn("Não foi possível abrir terminal. Corre manualmente: " + zen)

    def _stop(self):
        self._runner.kill()

    def _clear_output(self):
        self._output.clear_output()

    def _show_run_opts(self):
        d = RunOptionsDialog(self, self._run_opts)
        if d.exec() == QDialog.DialogCode.Accepted:
            self._run_opts = d.get_opts()
            self._output.info("Opções actualizadas.")

    # ── Callbacks runner ─────────────────────────────────────
    def _on_stdout(self, line):
        self._output.plain(line)

    def _on_stderr(self, line):
        # tenta detectar erros de compilação
        low = line.lower()
        if any(k in low for k in ("error", "erro", "failed", "fatal")):
            self._output.error(line)
        elif any(k in low for k in ("warning", "warn")):
            self._output.warn(line)
        else:
            self._output.error(line)

    def _on_finished(self, code):
        self._act_run.setEnabled(True)
        self._act_stop.setEnabled(False)
        if code == 0:
            self._output.ok(f"\n✓ Processo terminou (exit 0)")
            self._status_zen.setText("OK")
        else:
            self._output.error(f"\n✗ Processo terminou (exit {code})")
            self._status_zen.setText(f"Exit {code}")

    # ── Fechar ────────────────────────────────────────────────
    def closeEvent(self, event):
        if self._check_save():
            event.accept()
        else:
            event.ignore()


# ──────────────────────────────────────────────────────────────
# MAIN
# ──────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Zen IDE")
 
    app = QApplication(sys.argv)
    app.setApplicationName("Zen IDE")

    ide = ZenIDE(zen_path="/media/projectos/projects/cpp/zenvm/bin/zen")
 

    ide.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

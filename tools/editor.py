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
import tempfile
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QTextEdit, QPlainTextEdit, QToolBar, QStatusBar,
    QFileDialog, QLabel, QLineEdit, QDialog, QDialogButtonBox,
    QFormLayout, QCheckBox, QMessageBox, QTabWidget, QFrame,
    QComboBox, QSizePolicy, QTabBar, QPushButton, QMenu
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
    "keyword":      "#cba6f7",
    "builtin":      "#89b4fa",
    "string":       "#a6e3a1",
    "number":       "#fab387",
    "comment":      "#585b70",
    "operator":     "#89dceb",
    "type":         "#f9e2af",
    "func_def":     "#89b4fa",
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
    "tab_close":    "#f38ba8",
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
    padding: 5px 8px 5px 14px;
    margin-right: 2px;
    font-size: 12px;
    min-width: 80px;
}}
QTabBar::tab:selected {{
    background-color: {THEME['bg']};
    color: {THEME['fg']};
    border-top: 2px solid {THEME['builtin']};
}}
QTabBar::tab:hover {{
    background-color: {THEME['bg_line']};
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
QPushButton#closeTab {{
    background: transparent;
    color: {THEME['fg_dim']};
    border: none;
    border-radius: 3px;
    font-size: 11px;
    padding: 0px 2px;
    min-width: 16px;
    max-width: 16px;
    min-height: 16px;
    max-height: 16px;
}}
QPushButton#closeTab:hover {{
    background-color: {THEME['stop_btn']};
    color: {THEME['bg']};
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

        keywords = (
            "var if else while for loop break continue return import "
            "struct class def process self father son true false nil null "
            "frame and or not in do yield spawn resume"
        ).split()
        for kw_str in keywords:
            self._rules.append((
                QRegularExpression(r'\b' + kw_str + r'\b'), kw
            ))

        types = (
            "Int8Array Int16Array Int32Array Uint8Array Uint16Array "
            "Uint32Array Float32Array Float64Array"
        ).split()
        for t in types:
            self._rules.append((QRegularExpression(r'\b' + t + r'\b'), ty))

        builtins = (
            "print len int float bool push pop sin cos tan sqrt pow abs "
            "floor ceil log exp atan2 deg rad clock"
        ).split()
        for b in builtins:
            self._rules.append((QRegularExpression(r'\b' + b + r'(?=\s*\()'), bi))

        self._rules.append((
            QRegularExpression(r'\bdef\s+(\w+)\s*\('), fn
        ))

        self._rules.append((QRegularExpression(r'\b0x[0-9a-fA-F]+\b'), nm))
        self._rules.append((QRegularExpression(r'\b\d+\.\d*([eE][+-]?\d+)?\b'), nm))
        self._rules.append((QRegularExpression(r'\b\d+([eE][+-]?\d+)?\b'), nm))

        self._rules.append((QRegularExpression(r'"(?:[^"\\]|\\.)*"'), st))
        self._rules.append((QRegularExpression(r'@"(?:[^"]|"")*"'), st))

        self._rules.append((
            QRegularExpression(r'[+\-*/%&|^~<>!=]+|<<|>>|&&|\|\|'), op
        ))

        self._rules.append((QRegularExpression(r'//[^\n]*'), cm))

        self._comment_start = QRegularExpression(r'/\*')
        self._comment_end   = QRegularExpression(r'\*/')
        self._comment_fmt   = cm

    def highlightBlock(self, text):
        for pattern, fmt in self._rules:
            it = pattern.globalMatch(text)
            while it.hasNext():
                m = it.next()
                self.setFormat(m.capturedStart(), m.capturedLength(), fmt)

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
        if e.key() == Qt.Key.Key_Return:
            cursor = self.textCursor()
            block  = cursor.block().text()
            indent = len(block) - len(block.lstrip())
            if block.rstrip().endswith('{'):
                indent += 4
            super().keyPressEvent(e)
            self.insertPlainText(' ' * indent)
            return
        if e.key() == Qt.Key.Key_Tab:
            self.insertPlainText('    ')
            return
        super().keyPressEvent(e)

    def contextMenuEvent(self, event):
        menu = self.createStandardContextMenu()
        menu.addSeparator()

        act_fmt = QAction("{ }  Formatar chavetas (nova linha)", self)
        act_fmt.triggered.connect(self._format_braces)
        menu.addAction(act_fmt)

        act_sel = QAction("{ }  Formatar selecção", self)
        act_sel.triggered.connect(lambda: self._format_braces(selection_only=True))
        menu.addAction(act_sel)

        menu.exec(event.globalPos())

    def _format_braces(self, selection_only=False):
        """Move { para nova linha (Allman style) e re-indenta."""
        cursor = self.textCursor()

        if selection_only and cursor.hasSelection():
            start  = cursor.selectionStart()
            end    = cursor.selectionEnd()
            cursor.setPosition(start)
            cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
            original = cursor.selectedText()
            # Qt usa \u2029 como separador de parágrafo na selecção
            original = original.replace('\u2029', '\n')
        else:
            original = self.toPlainText()
            start    = None

        formatted = _allman_format(original)

        if selection_only and start is not None:
            cursor.insertText(formatted)
        else:
            # substitui tudo preservando scroll
            vbar = self.verticalScrollBar()
            pos  = vbar.value()
            self.setPlainText(formatted)
            vbar.setValue(pos)


# ──────────────────────────────────────────────────────────────
# FORMATTER  (Allman style: { em nova linha, re-indentação)
# ──────────────────────────────────────────────────────────────
def _allman_format(code: str) -> str:
    """
    Reformata código Zen/BuLang no estilo Allman:
      - '{' vai sempre para nova linha (com a indentação do bloco pai)
      - '}' fica na sua própria linha
      - re-indentação a 4 espaços por nível
    Preserva strings e comentários intactos.
    """
    import re

    # ── passo 1: tokenizar para não tocar em strings/comentários ──
    # Produz lista de (tipo, texto): 'str', 'cmt_line', 'cmt_block', 'code'
    TOKEN_RE = re.compile(
        r'(?P<str>@?"(?:[^"\\]|\\.)*")'        # string (incl. @"...")
        r'|(?P<cmt_line>//[^\n]*)'              # comentário linha
        r'|(?P<cmt_block>/\*.*?\*/)'            # comentário bloco
        , re.DOTALL
    )

    tokens = []
    last = 0
    for m in TOKEN_RE.finditer(code):
        if m.start() > last:
            tokens.append(('code', code[last:m.start()]))
        kind = m.lastgroup
        tokens.append((kind, m.group()))
        last = m.end()
    if last < len(code):
        tokens.append(('code', code[last:]))

    # ── passo 2: nas partes 'code', separa { e } em linhas próprias ──
    def reformat_code(text: str) -> str:
        # Garante que { e } ficam em linhas separadas
        text = re.sub(r'[ \t]*\{[ \t]*', '\n{\n', text)
        text = re.sub(r'[ \t]*\}[ \t]*', '\n}\n', text)
        # colapsa linhas em branco múltiplas num único \n
        text = re.sub(r'\n{3,}', '\n\n', text)
        return text

    parts = []
    for kind, txt in tokens:
        if kind == 'code':
            parts.append(reformat_code(txt))
        else:
            parts.append(txt)

    joined = ''.join(parts)

    # ── passo 3: re-indentação ──────────────────────────────────
    lines  = joined.splitlines()
    result = []
    indent = 0
    INDENT = '    '   # 4 espaços

    for raw in lines:
        line = raw.strip()
        if not line:
            result.append('')
            continue

        # fecha antes de imprimir
        if line.startswith('}'):
            indent = max(0, indent - 1)

        result.append(INDENT * indent + line)

        # abre depois de imprimir
        if line == '{':
            indent += 1

    # remove linhas em branco no início/fim
    text_out = '\n'.join(result).strip() + '\n'
    # colapsa >2 linhas em branco consecutivas
    text_out = re.sub(r'\n{3,}', '\n\n', text_out)
    return text_out


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
    def clear_output(self):  self.clear()


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

        self.chk_dis      = QCheckBox("Mostrar disassembly (--dis)")
        self.chk_dis_only = QCheckBox("Só disassembly, não executa (--dis-only)")
        self.chk_verbose  = QCheckBox("Verbose (-v)")
        self.chk_strip    = QCheckBox("Strip debug (--strip-debug)")
        self.txt_dump     = QLineEdit()
        self.txt_dump.setPlaceholderText("deixar vazio para não fazer dump")
        self.txt_include  = QLineEdit()
        self.txt_include.setPlaceholderText("-I /caminho/includes")
        self.txt_args     = QLineEdit()
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
# ESTRUTURA DE DADOS POR TAB DE EDITOR
# ──────────────────────────────────────────────────────────────
class EditorTab:
    """Agrupa editor + estado (path, modified) para uma tab."""
    def __init__(self):
        self.editor    = CodeEditor()
        self.file_path = None
        self.modified  = False


# ──────────────────────────────────────────────────────────────
# TAB BAR COM BOTÃO × POR TAB
# ──────────────────────────────────────────────────────────────
class ClosableTabBar(QTabBar):
    """QTabBar que emite closeRequested(index) ao clicar no ×."""
    closeRequested = pyqtSignal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTabsClosable(False)   # vamos desenhar o nosso próprio botão
        self._close_btns: dict[int, QPushButton] = {}

    def addCloseButton(self, index: int):
        btn = QPushButton("×")
        btn.setObjectName("closeTab")
        btn.setFixedSize(16, 16)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.clicked.connect(lambda _, i=index: self._request_close(i))
        self._close_btns[index] = btn
        self.setTabButton(index, QTabBar.ButtonPosition.RightSide, btn)

    def _request_close(self, index):
        # botões ficam fixados ao índice original; recalcula pelo botão
        for i, btn in self._close_btns.items():
            if btn == self.sender():
                self.closeRequested.emit(i)
                return
        self.closeRequested.emit(index)

    def removeTab(self, index):
        # limpa o dict de botões e reindexiza
        self._close_btns.pop(index, None)
        new_btns = {}
        for i, btn in self._close_btns.items():
            new_i = i if i < index else i - 1
            new_btns[new_i] = btn
            # re-wire click para o novo índice
            try:
                btn.clicked.disconnect()
            except Exception:
                pass
            btn.clicked.connect(lambda _, ni=new_i: self.closeRequested.emit(ni))
        self._close_btns = new_btns
        super().removeTab(index)


# ──────────────────────────────────────────────────────────────
# EDITOR TAB WIDGET (wrapper sobre QTabWidget)
# ──────────────────────────────────────────────────────────────
class EditorTabWidget(QTabWidget):
    """QTabWidget com ClosableTabBar e gestão de EditorTab."""

    currentEditorChanged = pyqtSignal()   # emitido quando a tab activa muda

    def __init__(self, parent=None):
        super().__init__(parent)
        self._tab_bar = ClosableTabBar(self)
        self.setTabBar(self._tab_bar)
        self._tab_bar.closeRequested.connect(self._on_close_requested)
        self.currentChanged.connect(lambda _: self.currentEditorChanged.emit())

        # lista paralela à ordem das tabs
        self._tabs: list[EditorTab] = []

    # ── API pública ──────────────────────────────────────────

    def add_editor_tab(self, tab: EditorTab, label: str) -> int:
        """Adiciona uma EditorTab e devolve o índice."""
        idx = self.addTab(tab.editor, label)
        self._tab_bar.addCloseButton(idx)
        self._tabs.insert(idx, tab)
        self.setCurrentIndex(idx)
        return idx

    def current_tab(self) -> EditorTab | None:
        idx = self.currentIndex()
        if 0 <= idx < len(self._tabs):
            return self._tabs[idx]
        return None

    def tab_at(self, index: int) -> EditorTab | None:
        if 0 <= index < len(self._tabs):
            return self._tabs[index]
        return None

    def count_tabs(self) -> int:
        return len(self._tabs)

    def update_tab_label(self, index: int):
        tab = self.tab_at(index)
        if tab is None:
            return
        name = os.path.basename(tab.file_path) if tab.file_path else "sem título"
        mod  = " •" if tab.modified else ""
        self.setTabText(index, name + mod)

    def index_of_tab(self, tab: EditorTab) -> int:
        try:
            return self._tabs.index(tab)
        except ValueError:
            return -1

    # ── Fechar tab ───────────────────────────────────────────

    def _on_close_requested(self, index: int):
        """Chamado pelo botão ×.  Delega ao IDE para guardar se necessário."""
        # emite sinal para o IDE tratar
        self._pending_close = index
        self.parent()._close_tab(index)

    def force_remove_tab(self, index: int):
        """Remove a tab sem perguntar (já confirmado pelo IDE)."""
        self._tabs.pop(index)
        self._tab_bar.removeTab(index)
        # se ficámos sem tabs, cria uma nova em branco
        if self.count_tabs() == 0:
            self.parent()._new_file()


# ──────────────────────────────────────────────────────────────
# JANELA PRINCIPAL
# ──────────────────────────────────────────────────────────────
class ZenIDE(QMainWindow):

    SESSION_KEY = "session/open_files"
    SESSION_ACTIVE = "session/active_tab"

    def __init__(self, zen_path="zen"):
        super().__init__()
        self._settings  = QSettings("ZenVM", "ZenIDE")
        self._zen_path  = zen_path
        self._runner    = ZenRunner()
        self._run_opts  = {}
        self._extra_runner = None
        self._dump_runner  = None

        self._runner.stdout_line.connect(self._on_stdout)
        self._runner.stderr_line.connect(self._on_stderr)
        self._runner.finished.connect(self._on_finished)

        self.setWindowTitle("Zen IDE")
        self.resize(1200, 750)
        self.setStyleSheet(STYLESHEET)

        self._build_toolbar()
        self._build_central()
        self._build_statusbar()

        # Restaura sessão ou abre tab em branco
        self._restore_session()

    # ── Propriedades convenientes para a tab activa ──────────

    @property
    def _current_tab(self) -> EditorTab | None:
        return self._editor_tabs.current_tab()

    @property
    def _editor(self):
        t = self._current_tab
        return t.editor if t else None

    @property
    def _file_path(self):
        t = self._current_tab
        return t.file_path if t else None

    @_file_path.setter
    def _file_path(self, v):
        t = self._current_tab
        if t:
            t.file_path = v

    @property
    def _modified(self):
        t = self._current_tab
        return t.modified if t else False

    @_modified.setter
    def _modified(self, v):
        t = self._current_tab
        if t:
            t.modified = v

    # ── Toolbar ──────────────────────────────────────────────
    def _build_toolbar(self):
        tb = QToolBar("Toolbar principal")
        tb.setMovable(False)
        tb.setIconSize(QSize(16, 16))
        self.addToolBar(tb)

        def act(label, shortcut, tip, slot):
            a = QAction(label, self)
            if shortcut: a.setShortcut(QKeySequence(shortcut))
            a.setStatusTip(tip)
            a.triggered.connect(slot)
            tb.addAction(a)
            return a

        act("⬜ Novo",         "Ctrl+N",       "Nova tab",            self._new_file)
        act("📂 Abrir",        "Ctrl+O",       "Abrir ficheiro",      self._open_file)
        act("💾 Guardar",      "Ctrl+S",       "Guardar",             self._save_file)
        act("💾 Guardar Como", "Ctrl+Shift+S", "Guardar como",        self._save_as)

        tb.addSeparator()

        self._act_run   = act("▶  Executar",    "F5",  "Executar script",          self._run)
        self._act_run_e = act("⚡ Executar -e", "F6",  "Executar como -e inline",  self._run_inline)
        self._act_stop  = act("■  Parar",       "F7",  "Parar execução",           self._stop)
        self._act_stop.setEnabled(False)

        tb.addSeparator()

        act("🔍 Disassembly", "F8",  "Mostrar bytecode",                    self._run_dis)
        act("📦 Dump",   "F9",  "Gerar bytecode .znb ao lado do script", self._run_dump)
        act("🚀 Dump+Run",    "F11", "Dump bytecode e corre o .znb gerado",   self._run_dump_and_run)
        act("📋 REPL",        "F10", "Abrir REPL no terminal",              self._open_repl)

        tb.addSeparator()

        tb.addWidget(QLabel("zen:"))
        self._zen_edit = QLineEdit(self._zen_path)
        self._zen_edit.setFixedWidth(130)
        self._zen_edit.setToolTip("Caminho para o executável zen")
        self._zen_edit.textChanged.connect(lambda t: setattr(self, '_zen_path', t))
        tb.addWidget(self._zen_edit)

        act("⚙", None, "Opções de execução", self._show_run_opts)

        tb.addSeparator()
        act("🗑 Limpar output", None, "Limpar painel de output", self._clear_output)

    # ── Layout central ───────────────────────────────────────
    def _build_central(self):
        splitter = QSplitter(Qt.Orientation.Vertical)

        # editor multi-tab
        self._editor_tabs = EditorTabWidget(self)
        self._editor_tabs.currentEditorChanged.connect(self._on_tab_changed)

        # painel de output
        self._output_tabs = QTabWidget()
        self._output      = OutputPanel()
        self._dis_view    = OutputPanel()
        self._output_tabs.addTab(self._output,   "Output")
        self._output_tabs.addTab(self._dis_view, "Disassembly")

        splitter.addWidget(self._editor_tabs)
        splitter.addWidget(self._output_tabs)
        splitter.setSizes([480, 220])

        self.setCentralWidget(splitter)

    # ── Statusbar ────────────────────────────────────────────
    def _build_statusbar(self):
        self._status_file = QLabel("novo ficheiro")
        self._status_pos  = QLabel("Ln 1, Col 1")
        self._status_zen  = QLabel("")
        sb = self.statusBar()
        sb.addWidget(self._status_file, 1)
        sb.addWidget(self._status_pos)
        sb.addWidget(self._status_zen)

    def _connect_editor_signals(self, tab: EditorTab):
        tab.editor.document().contentsChanged.connect(
            lambda: self._on_modified(tab)
        )
        tab.editor.cursorPositionChanged.connect(self._update_pos)

    def _update_pos(self):
        e = self._editor
        if not e: return
        c = e.textCursor()
        self._status_pos.setText(f"Ln {c.blockNumber()+1}, Col {c.columnNumber()+1}")

    def _on_tab_changed(self):
        self._update_title()
        self._update_pos()

    def _last_dialog_dir(self):
        fp = self._file_path
        if fp:
            return os.path.dirname(fp)
        saved = self._settings.value("last_dir", "", type=str)
        return saved or os.getcwd()

    def _remember_dialog_dir(self, path):
        if not path: return
        directory = path if os.path.isdir(path) else os.path.dirname(path)
        if directory:
            self._settings.setValue("last_dir", directory)

    # ── Gestão de tabs ───────────────────────────────────────

    def _make_tab(self, content='// Novo script Zen\n\nprint("Olá, Mundo!");\n',
                  file_path=None, modified=False) -> EditorTab:
        tab = EditorTab()
        tab.file_path = file_path
        tab.modified  = modified
        tab.editor.setPlainText(content)
        tab.editor.document().setModified(False)
        self._connect_editor_signals(tab)
        return tab

    def _add_tab(self, tab: EditorTab):
        name  = os.path.basename(tab.file_path) if tab.file_path else "sem título"
        mod   = " •" if tab.modified else ""
        self._editor_tabs.add_editor_tab(tab, name + mod)
        self._update_title()

    def _close_tab(self, index: int):
        tab = self._editor_tabs.tab_at(index)
        if tab is None: return

        if tab.modified:
            name = os.path.basename(tab.file_path) if tab.file_path else "sem título"
            r = QMessageBox.question(
                self, "Guardar antes de fechar?",
                f"'{name}' tem alterações não guardadas. Guardar?",
                QMessageBox.StandardButton.Yes |
                QMessageBox.StandardButton.No  |
                QMessageBox.StandardButton.Cancel
            )
            if r == QMessageBox.StandardButton.Cancel:
                return
            if r == QMessageBox.StandardButton.Yes:
                # guarda a tab em questão (mesmo que não seja a activa)
                self._save_tab(tab)

        self._editor_tabs.force_remove_tab(index)
        self._update_title()
        self._save_session()

    # ── Ficheiro ─────────────────────────────────────────────

    def _new_file(self):
        tab = self._make_tab()
        self._add_tab(tab)
        self._save_session()

    def _open_file(self):
        paths, _ = QFileDialog.getOpenFileNames(
            self, "Abrir ficheiro(s) Zen", self._last_dialog_dir(),
            "Zen scripts (*.zen);;Bytecode (*.zenb);;Todos (*)"
        )
        for path in paths:
            # verifica se já está aberto
            already = self._find_tab_by_path(path)
            if already is not None:
                self._editor_tabs.setCurrentIndex(
                    self._editor_tabs.index_of_tab(already)
                )
                continue
            try:
                with open(path, "r", encoding="utf-8") as f:
                    content = f.read()
                self._remember_dialog_dir(path)
                tab = self._make_tab(content=content, file_path=path)
                self._add_tab(tab)
                self._output.info(f"Aberto: {path}")
            except Exception as e:
                QMessageBox.critical(self, "Erro", str(e))
        self._save_session()

    def _find_tab_by_path(self, path: str) -> EditorTab | None:
        for i in range(self._editor_tabs.count_tabs()):
            t = self._editor_tabs.tab_at(i)
            if t and t.file_path == path:
                return t
        return None

    def _save_file(self):
        tab = self._current_tab
        if tab is None: return
        if not tab.file_path:
            self._save_as()
        else:
            self._save_tab(tab)

    def _save_as(self):
        tab = self._current_tab
        if tab is None: return
        default = tab.file_path or os.path.join(self._last_dialog_dir(), "novo_script.zen")
        path, _ = QFileDialog.getSaveFileName(
            self, "Guardar como", default,
            "Zen scripts (*.zen);;Todos (*)"
        )
        if path:
            self._do_save(tab, path)

    def _save_tab(self, tab: EditorTab):
        """Guarda a tab; se não tiver path pede save-as apenas para essa tab."""
        if not tab.file_path:
            # activa a tab e pede nome
            idx = self._editor_tabs.index_of_tab(tab)
            if idx >= 0:
                self._editor_tabs.setCurrentIndex(idx)
            self._save_as()
        else:
            self._do_save(tab, tab.file_path)

    def _do_save(self, tab: EditorTab, path: str):
        try:
            with open(path, "w", encoding="utf-8") as f:
                f.write(tab.editor.toPlainText())
            self._remember_dialog_dir(path)
            tab.file_path = path
            tab.modified  = False
            idx = self._editor_tabs.index_of_tab(tab)
            if idx >= 0:
                self._editor_tabs.update_tab_label(idx)
            self._update_title()
            self._status_zen.setText(f"Guardado: {os.path.basename(path)}")
            self._save_session()
        except Exception as e:
            QMessageBox.critical(self, "Erro ao guardar", str(e))

    def _on_modified(self, tab: EditorTab):
        if not tab.modified:
            tab.modified = True
            idx = self._editor_tabs.index_of_tab(tab)
            if idx >= 0:
                self._editor_tabs.update_tab_label(idx)
            self._update_title()

    def _update_title(self):
        tab = self._current_tab
        if tab:
            name = os.path.basename(tab.file_path) if tab.file_path else "sem título"
            mod  = " •" if tab.modified else ""
            self.setWindowTitle(f"Zen IDE — {name}{mod}")
            self._status_file.setText(tab.file_path or "novo ficheiro")
        else:
            self.setWindowTitle("Zen IDE")
            self._status_file.setText("")

    # ── Sessão ───────────────────────────────────────────────

    def _save_session(self):
        paths = []
        for i in range(self._editor_tabs.count_tabs()):
            t = self._editor_tabs.tab_at(i)
            if t and t.file_path:
                paths.append(t.file_path)
        self._settings.setValue(self.SESSION_KEY, paths)
        self._settings.setValue(self.SESSION_ACTIVE,
                                self._editor_tabs.currentIndex())

    def _restore_session(self):
        paths  = self._settings.value(self.SESSION_KEY, [], type=list)
        active = self._settings.value(self.SESSION_ACTIVE, 0, type=int)

        opened = 0
        for path in paths:
            if not os.path.isfile(path):
                continue
            try:
                with open(path, "r", encoding="utf-8") as f:
                    content = f.read()
                tab = self._make_tab(content=content, file_path=path)
                self._add_tab(tab)
                opened += 1
            except Exception:
                pass

        if opened == 0:
            self._new_file()
        else:
            # restaura tab activa
            idx = min(active, self._editor_tabs.count_tabs() - 1)
            self._editor_tabs.setCurrentIndex(max(0, idx))
            self._output.info(f"Sessão restaurada: {opened} ficheiro(s).")

        self._update_title()

    # ── Utilitários de source para execução ──────────────────

    def _ensure_saved(self, tab: EditorTab) -> str | None:
        """Garante que a tab está gravada e devolve o path, ou None se cancelado."""
        if tab.file_path and not tab.modified:
            return tab.file_path
        if tab.file_path and tab.modified:
            self._do_save(tab, tab.file_path)
            return tab.file_path if not tab.modified else None
        # sem path — guarda como
        self._save_tab(tab)
        return tab.file_path

    def _default_dump_path(self, source_path):
        base, _ = os.path.splitext(source_path)
        return base + ".znb"

    # ── Construir comando zen ─────────────────────────────────
    def _build_cmd(self, extra_flags=None, force_file=None,
                   override_dump=None, ignore_dump=False) -> list[str]:
        cmd  = [self._zen_path]
        opts = self._run_opts

        if opts.get("dis"):      cmd.append("--dis")
        if opts.get("dis_only"): cmd += ["--dis", "--dis-only"]
        if opts.get("verbose"):  cmd.append("-v")
        if opts.get("strip"):    cmd.append("--strip-debug")
        if override_dump:
            cmd += ["--dump", override_dump]
        elif not ignore_dump and opts.get("dump"):
            cmd += ["--dump", opts["dump"]]
        if opts.get("include"):  cmd += ["-I", opts["include"]]

        if extra_flags:
            cmd += extra_flags

        if force_file:
            cmd.append(force_file)
        else:
            tab = self._current_tab
            if tab and tab.file_path:
                cmd.append(tab.file_path)
            else:
                cmd += ["-e", (tab.editor.toPlainText() if tab else "")]

        if opts.get("script_args"):
            cmd += shlex.split(opts["script_args"])

        return cmd

    # ── Executar ──────────────────────────────────────────────
    def _start_run(self, cmd):
        self._output.clear_output()
        self._output.info(f"$ {' '.join(cmd)}\n")
        self._output_tabs.setCurrentIndex(0)
        self._act_run.setEnabled(False)
        self._act_stop.setEnabled(True)
        self._status_zen.setText("A executar…")
        self._runner.run(cmd)

    def _run(self):
        tab = self._current_tab
        if not tab: return

        if tab.file_path:
            if tab.modified:
                self._do_save(tab, tab.file_path)
            self._start_run(self._build_cmd())
        else:
            # sem path: ficheiro temporário
            tmp = tempfile.NamedTemporaryFile(suffix=".zen", delete=False, mode="w",
                                             encoding="utf-8")
            tmp.write(tab.editor.toPlainText())
            tmp.close()
            self._start_run(self._build_cmd(force_file=tmp.name))

    def _run_inline(self):
        tab = self._current_tab
        if not tab: return
        code = tab.editor.toPlainText()
        self._start_run([self._zen_path, "-e", code])

    def _run_dis(self):
        tab = self._current_tab
        if not tab: return
        if tab.file_path and tab.modified:
            self._do_save(tab, tab.file_path)

        self._dis_view.clear_output()
        self._output_tabs.setCurrentIndex(1)

        cmd = self._build_cmd(extra_flags=["--dis-only"])
        self._output.info(f"$ {' '.join(cmd[:3])}…")
        self._act_run.setEnabled(False)
        self._act_stop.setEnabled(True)
        self._status_zen.setText("Disassembly…")

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
        """Dump .znb — grava sempre o ficheiro antes de fazer dump."""
        tab = self._current_tab
        if not tab: return

        # garante que o ficheiro está guardado (pede path se necessário)
        saved_path = self._ensure_saved(tab)
        if not saved_path:
            return   # utilizador cancelou

        dump_path = self._default_dump_path(saved_path)
        cmd = self._build_cmd(
            extra_flags=["--dump", dump_path, "-v"],
            force_file=saved_path,
            ignore_dump=True
        )
        self._output.info(f"Dump bytecode para: {dump_path}")
        self._start_run(cmd)

    def _run_dump_and_run(self):
        """Dump .znb e de seguida executa o bytecode gerado."""
        tab = self._current_tab
        if not tab: return

        saved_path = self._ensure_saved(tab)
        if not saved_path:
            return

        dump_path = self._default_dump_path(saved_path)

        # Fase 1 — dump
        dump_cmd = [self._zen_path, "--dump", dump_path, "-v", saved_path]
        opts = self._run_opts
        if opts.get("include"): dump_cmd += ["-I", opts["include"]]

        self._output.clear_output()
        self._output.info(f"[1/2] Dump para: {dump_path}")
        self._output.info(f"$ {' '.join(dump_cmd)}\n")
        self._output_tabs.setCurrentIndex(0)
        self._act_run.setEnabled(False)
        self._act_stop.setEnabled(True)
        self._status_zen.setText("A compilar para bytecode…")

        # runner para a fase 1
        dump_runner = ZenRunner()
        dump_runner.stdout_line.connect(self._on_stdout)
        dump_runner.stderr_line.connect(self._on_stderr)

        def _on_dump_finished(code):
            if code != 0:
                self._act_run.setEnabled(True)
                self._act_stop.setEnabled(False)
                self._output.error(f"\n✗ Dump falhou (exit {code}) — execução cancelada.")
                self._status_zen.setText(f"Dump falhou (exit {code})")
                return

            self._output.ok(f"\n✓ Bytecode gerado: {os.path.basename(dump_path)}")
            self._output.info(f"\n[2/2] A executar bytecode…")

            # Fase 2 — corre o .znb
            run_cmd = [self._zen_path, dump_path]
            if opts.get("verbose"):     run_cmd.insert(1, "-v")
            if opts.get("strip"):       run_cmd.insert(1, "--strip-debug")
            if opts.get("include"):     run_cmd += ["-I", opts["include"]]
            if opts.get("script_args"): run_cmd += shlex.split(opts["script_args"])

            self._output.info(f"$ {' '.join(run_cmd)}\n")
            self._status_zen.setText("A executar bytecode…")
            self._runner.run(run_cmd)

        dump_runner.finished.connect(_on_dump_finished)
        dump_runner.run(dump_cmd)
        # mantém referência para não ser garbage collected
        self._dump_runner = dump_runner

    def _open_repl(self):
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

    # ── Fechar janela ─────────────────────────────────────────
    def closeEvent(self, event):
        # verifica tabs com alterações não guardadas
        unsaved = []
        for i in range(self._editor_tabs.count_tabs()):
            t = self._editor_tabs.tab_at(i)
            if t and t.modified:
                unsaved.append((i, t))

        if unsaved:
            names = ", ".join(
                os.path.basename(t.file_path) if t.file_path else "sem título"
                for _, t in unsaved
            )
            r = QMessageBox.question(
                self, "Guardar alterações?",
                f"Ficheiros com alterações:\n{names}\n\nGuardar tudo?",
                QMessageBox.StandardButton.Yes |
                QMessageBox.StandardButton.No  |
                QMessageBox.StandardButton.Cancel
            )
            if r == QMessageBox.StandardButton.Cancel:
                event.ignore()
                return
            if r == QMessageBox.StandardButton.Yes:
                for _, t in unsaved:
                    self._save_tab(t)

        self._save_session()
        event.accept()


# ──────────────────────────────────────────────────────────────
# MAIN
# ──────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Zen IDE")
    parser.add_argument("--zen", default="/media/projectos/projects/cpp/zenvm/bin/zen",
                        help="Caminho para o executável zen")
    args, _ = parser.parse_known_args()

    app = QApplication(sys.argv)
    app.setApplicationName("Zen IDE")

    ide = ZenIDE(zen_path=args.zen)
    ide.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
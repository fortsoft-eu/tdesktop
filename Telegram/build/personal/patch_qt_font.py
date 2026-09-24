from pathlib import Path
import sys


path = Path(sys.argv[1]) / "src/gui/text/qfontdatabase.cpp"
original = path.read_text(encoding="utf-8")
marker = "TDESKTOP_UNIFONT_PIXEL_FONT"
if marker in original:
    old_policy = "def.styleStrategy |= unifont ? QFont::NoAntialias : QFont::PreferAntialias;"
    new_policy = 'def.styleStrategy |= (unifont || family->name.compare(QLatin1String("Tahoma"), Qt::CaseInsensitive) == 0)\n            ? QFont::NoAntialias : QFont::PreferAntialias;'
    if old_policy in original:
        original = original.replace(old_policy, new_policy)
        path.write_bytes(original.replace("\n", "\r\n").encode("utf-8"))
        print("Updated personal Qt Tahoma font policy.")
    else:
        print("Personal Qt font policy already installed.")

anchor = "    QFontDef def = request;\n    def.pixelSize = pixelSize;\n"
replacement = anchor + """
    if (qEnvironmentVariableIsSet("TDESKTOP_UNIFONT_PIXEL_FONT")) {
        const bool unifont = family->name.compare(QLatin1String("Unifont"), Qt::CaseInsensitive) == 0;
        def.styleStrategy &= ~(QFont::NoAntialias | QFont::PreferAntialias);
        def.styleStrategy |= (unifont || family->name.compare(QLatin1String("Tahoma"), Qt::CaseInsensitive) == 0)
            ? QFont::NoAntialias : QFont::PreferAntialias;
        if (unifont) {
            def.pixelSize = 16;
            def.hintingPreference = QFont::PreferFullHinting;
        }
    }
"""
if marker not in original and original.count(anchor) != 1:
    raise SystemExit("Unexpected Qt font database source; no changes applied.")
if marker not in original:
    updated = original.replace(anchor, replacement)
    path.with_suffix(".cpp.before-personal-font").write_bytes(path.read_bytes())
    path.write_bytes(updated.replace("\n", "\r\n").encode("utf-8"))
    print("Installed personal Qt font policy.")

path = Path(sys.argv[1]) / "src/gui/painting/qpainter.cpp"
original = path.read_text(encoding="utf-8")
marker = "TDESKTOP_BLACK_NEUTRAL_TEXT"
if marker not in original:
    anchor = "    const QPainter::RenderHints oldRenderHints = state->renderHints;"
    assert original.count(anchor) == 1, "Unexpected Qt painter source"
    replacement = '''    const QPen personalOriginalPen = q->pen();
    const QColor personalTextColor = personalOriginalPen.color();
    const bool personalBlackText = qEnvironmentVariableIsSet("TDESKTOP_BLACK_NEUTRAL_TEXT")
        && personalOriginalPen.brush().style() == Qt::SolidPattern
        && personalTextColor.red() == personalTextColor.green()
        && personalTextColor.green() == personalTextColor.blue()
        && personalTextColor.red() != 0;
    if (personalBlackText) {
        QPen blackPen = personalOriginalPen;
        blackPen.setColor(QColor(0, 0, 0, personalTextColor.alpha()));
        q->setPen(blackPen);
    }

''' + anchor
    updated = original.replace(anchor, replacement)
    anchor = "    if (state->renderHints != oldRenderHints) {"
    assert updated.count(anchor) == 1, "Unexpected Qt painter restore"
    updated = updated.replace(anchor, '''    if (personalBlackText)
        q->setPen(personalOriginalPen);

''' + anchor)
    anchor = "    if (d->extended == nullptr\n            || !d->state->matrix.isAffine()"
    assert updated.count(anchor) == 1, "Unexpected Qt static text painter"
    updated = updated.replace(anchor, '''    if (d->extended == nullptr
            || qEnvironmentVariableIsSet("TDESKTOP_BLACK_NEUTRAL_TEXT")
            || !d->state->matrix.isAffine()''')
    path.with_suffix(".cpp.before-personal-text").write_bytes(path.read_bytes())
    path.write_bytes(updated.replace("\n", "\r\n").encode("utf-8"))
    print("Installed text-only neutral color policy.")

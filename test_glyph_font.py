"""
Verify: Can we correctly identify which font is used for each character
in a string, using QTextLayout → QGlyphRun → QRawFont?

Key finding: In Qt 6, glyphRuns() requires explicit flags to populate
stringIndexes(). Without RetrieveStringIndexes, the mapping is empty.
"""
import sys
from PySide6.QtCore import Qt
from PySide6.QtGui import QFont, QTextLayout, QGuiApplication, QRawFont


def build_utf16_to_python_map(text: str):
    """Build a mapping from UTF-16 code unit offset to Python string index.

    Qt uses UTF-16 internally. Characters outside the BMP (like emoji)
    are stored as surrogate pairs (2 code units), but Python treats them
    as single characters. This mapping converts Qt's UTF-16 offsets back
    to Python string positions.
    """
    utf16_to_py = {}
    utf16_offset = 0
    for py_idx, ch in enumerate(text):
        utf16_to_py[utf16_offset] = py_idx
        # Characters above U+FFFF need a surrogate pair (2 UTF-16 units)
        if ord(ch) > 0xFFFF:
            utf16_to_py[utf16_offset + 1] = py_idx  # second surrogate maps too
            utf16_offset += 2
        else:
            utf16_offset += 1
    return utf16_to_py


def analyze_string(text: str, font_family: str, font_size: int = 24):
    """Analyze which physical font is used for each character in `text`."""
    font = QFont(font_family, font_size)

    # Step 1: QTextLayout → glyphRuns
    layout = QTextLayout(text, font)
    layout.beginLayout()
    line = layout.createLine()
    layout.endLayout()

    # CRITICAL: In Qt 6, must pass flags to get stringIndexes populated
    flags = (QTextLayout.GlyphRunRetrievalFlag.RetrieveGlyphIndexes
             | QTextLayout.GlyphRunRetrievalFlag.RetrieveGlyphPositions
             | QTextLayout.GlyphRunRetrievalFlag.RetrieveStringIndexes
             | QTextLayout.GlyphRunRetrievalFlag.RetrieveString)
    glyph_runs = layout.glyphRuns(-1, -1, flags)

    print(f"\n{'='*60}")
    print(f"Input text   : \"{text}\"")
    print(f"Requested font: {font_family}")
    print(f"Glyph runs   : {len(glyph_runs)}")
    print(f"{'='*60}")

    # Build UTF-16 → Python index mapping for surrogate pair handling
    utf16_to_py = build_utf16_to_python_map(text)

    # Build a per-character mapping
    char_font_map = {}  # position -> (char, font_name, glyph_index)

    for i, run in enumerate(glyph_runs):
        # Step 2: rawFont
        raw_font = run.rawFont()

        # Step 3: familyName
        family_name = raw_font.familyName()

        # Step 4: glyphIndexes + stringIndexes for mapping
        glyph_indexes = run.glyphIndexes()
        string_indexes = run.stringIndexes()

        print(f"\n--- Glyph Run #{i} ---")
        print(f"  Physical font : {family_name}")
        print(f"  Pixel size    : {raw_font.pixelSize()}")
        print(f"  Glyph count   : {len(glyph_indexes)}")
        print(f"  String indexes (UTF-16): {list(string_indexes)}")

        for j, glyph_idx in enumerate(glyph_indexes):
            if j < len(string_indexes):
                utf16_idx = string_indexes[j]
                # Convert UTF-16 offset to Python string index
                py_idx = utf16_to_py.get(utf16_idx)
                if py_idx is not None and py_idx < len(text):
                    char = text[py_idx]
                    char_font_map[py_idx] = (char, family_name, glyph_idx)

    # Print per-character result
    print(f"\n{'='*60}")
    print("Per-character font mapping:")
    print(f"{'='*60}")
    for pos in range(len(text)):
        if pos in char_font_map:
            char, font_name, glyph_idx = char_font_map[pos]
            print(f"  [{pos}] '{char}' (U+{ord(char):04X}) -> {font_name}  (glyph #{glyph_idx})")
        else:
            print(f"  [{pos}] '{text[pos]}' (U+{ord(text[pos]):04X}) -> <not mapped>")

    return char_font_map


def verify_correctness(char_font_map, text, expected_font, fallback_chars=None):
    """Check if the mapping makes sense."""
    print(f"\n{'='*60}")
    print("Verification:")
    print(f"{'='*60}")

    all_mapped = all(pos in char_font_map for pos in range(len(text)))
    print(f"  All characters mapped: {'YES' if all_mapped else 'NO'}")

    # Check that each glyph index is non-zero (0 often means .notdef / missing)
    has_notdef = False
    for pos in range(len(text)):
        if pos in char_font_map:
            char, font_name, glyph_idx = char_font_map[pos]
            if glyph_idx == 0:
                print(f"  WARNING: '{char}' has glyph index 0 (.notdef) in {font_name}")
                has_notdef = True

    if not has_notdef:
        print(f"  All glyph indexes are non-zero (no .notdef): YES")

    # Check font fallback happened for expected characters
    fonts_used = set()
    for pos in char_font_map:
        _, font_name, _ = char_font_map[pos]
        fonts_used.add(font_name)

    print(f"  Fonts used: {fonts_used}")

    if fallback_chars:
        for pos in range(len(text)):
            if pos in char_font_map:
                char, font_name, _ = char_font_map[pos]
                if char in fallback_chars:
                    is_fallback = font_name.lower() != expected_font.lower()
                    print(f"  '{char}' fallback to different font: "
                          f"{'YES' if is_fallback else 'NO (same as requested)'}")

    return all_mapped and not has_notdef


def cross_verify(text: str, font_family: str, font_size: int = 24):
    """Cross-verify: compare glyphIndexes from run vs rawFont.glyphIndexesForString()"""
    font = QFont(font_family, font_size)
    layout = QTextLayout(text, font)
    layout.beginLayout()
    layout.createLine()
    layout.endLayout()

    flags = (QTextLayout.GlyphRunRetrievalFlag.RetrieveGlyphIndexes
             | QTextLayout.GlyphRunRetrievalFlag.RetrieveStringIndexes)
    glyph_runs = layout.glyphRuns(-1, -1, flags)

    print(f"\nCross-verification for: \"{text}\"")
    all_match = True
    for run in glyph_runs:
        rf = run.rawFont()
        family = rf.familyName()
        glyph_indexes = run.glyphIndexes()
        string_indexes = run.stringIndexes()

        utf16_to_py = build_utf16_to_python_map(text)
        for j in range(min(len(glyph_indexes), len(string_indexes))):
            si = string_indexes[j]
            py_idx = utf16_to_py.get(si)
            if py_idx is not None and py_idx < len(text):
                ch = text[py_idx]
                from_run = glyph_indexes[j]
                indexes_for_char = rf.glyphIndexesForString(ch)
                if indexes_for_char:
                    from_raw = indexes_for_char[0]
                    match = (from_run == from_raw)
                    if not match:
                        all_match = False
                    print(f"  '{ch}': run_glyph={from_run}, rawFont_glyph={from_raw}, "
                          f"match={match}, font={family}")

    print(f"  All cross-checks match: {'YES' if all_match else 'NO'}")
    return all_match


def main():
    app = QGuiApplication(sys.argv)

    # --- Test 1: Mixed Latin + Japanese with a Latin-only font ---
    print("\n" + "#"*60)
    print("# TEST 1: Latin + Japanese with 'Arial'")
    print("#"*60)
    text1 = "Helloこんにちは"
    map1 = analyze_string(text1, "Arial")
    ok1 = verify_correctness(map1, text1, "Arial", fallback_chars="こんにちは")

    # --- Test 2: Pure ASCII with a common font ---
    print("\n" + "#"*60)
    print("# TEST 2: Pure ASCII with 'DejaVu Sans'")
    print("#"*60)
    text2 = "Hello World 123"
    map2 = analyze_string(text2, "DejaVu Sans")
    ok2 = verify_correctness(map2, text2, "DejaVu Sans")

    # --- Test 3: Emoji (likely fallback) ---
    print("\n" + "#"*60)
    print("# TEST 3: Text with emoji")
    print("#"*60)
    text3 = "Hi😀Bye"
    map3 = analyze_string(text3, "Arial")
    ok3 = verify_correctness(map3, text3, "Arial", fallback_chars="😀")

    # --- Test 4: Cross-verify with glyphIndexesForString ---
    print("\n" + "#"*60)
    print("# TEST 4: Cross-verify glyphIndexesForString()")
    print("#"*60)
    ok4 = cross_verify("ABCあいう", "Arial")

    # --- Summary ---
    print("\n" + "#"*60)
    print("# SUMMARY")
    print("#"*60)
    results = [("Test 1 (Latin+JP)", ok1),
               ("Test 2 (Pure ASCII)", ok2),
               ("Test 3 (Emoji)", ok3),
               ("Test 4 (Cross-verify)", ok4)]
    for name, ok in results:
        print(f"  {name}: {'PASS' if ok else 'FAIL'}")

    all_pass = all(ok for _, ok in results)
    print(f"\n  Overall: {'ALL PASS' if all_pass else 'SOME FAILED'}")


if __name__ == "__main__":
    main()

#!/usr/bin/env bash
# generate-manual.sh — Build a single-file PDF user manual from the wiki markdown.
#
# Usage:
#   ./scripts/generate-manual.sh [output.pdf] [--cover-image=PATH] [--prepend=FILE]
#
#   --cover-image=PATH   PNG/JPG to use as the cover page (e.g. packaging/icon.png)
#   --prepend=FILE       Markdown file prepended as chapter 1 (e.g. release notes)
#
# Requirements:
#   pandoc   https://pandoc.org/installing.html
#   A LaTeX engine (pdflatex / xelatex / lualatex) — installed automatically
#   on most systems via texlive-latex-base, or via pandoc's --pdf-engine flag.
#
# Requires xelatex for Unicode support (emoji/symbols in the wiki).
#
# On Debian/Ubuntu:
#   sudo apt-get install pandoc texlive-xetex texlive-fonts-recommended
# On Fedora/openSUSE:
#   sudo dnf install pandoc texlive-xetex
# On macOS (Homebrew):
#   brew install pandoc mactex
# On Windows (Scoop):
#   scoop install pandoc miktex

set -euo pipefail

OUTPUT="HamClock-Next-Manual.pdf"
COVER_IMAGE=""
PREPEND_FILE=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --cover-image=*) COVER_IMAGE="${1#*=}"; shift ;;
    --prepend=*)     PREPEND_FILE="${1#*=}";  shift ;;
    *)               OUTPUT="$1";             shift ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
WIKI="$REPO_ROOT/docs/wiki"

# ── Dependency check ──────────────────────────────────────────────────────────
if ! command -v pandoc &>/dev/null; then
  echo "ERROR: pandoc not found."
  echo ""
  echo "Install it with:"
  echo "  Debian/Ubuntu : sudo apt-get install pandoc texlive-xetex texlive-fonts-recommended"
  echo "  Fedora        : sudo dnf install pandoc texlive-xetex"
  echo "  macOS         : brew install pandoc mactex"
  echo "  Windows       : scoop install pandoc miktex"
  exit 1
fi

# ── PDF engine: prefer lualatex (modern/robust), xelatex (Unicode), pdflatex ───
PDF_ENGINE=""
for engine in lualatex xelatex pdflatex; do
  if command -v "$engine" &>/dev/null; then
    PDF_ENGINE="$engine"
    break
  fi
done
if [[ -z "$PDF_ENGINE" ]]; then
  echo "ERROR: No LaTeX PDF engine found (tried lualatex, xelatex, pdflatex)."
  echo "Install texlive-luatex (recommended) or texlive-xetex and re-run."
  exit 1
fi
echo "PDF engine : $PDF_ENGINE"

# ── Font selection for Unicode support (Gear, Star, Comet symbols) ────────────
# Default Latin Modern fonts lack the symbols used in the wiki.
# DejaVu fonts have high stability in LuaTeX.
# We prioritize DejaVu Serif for text and use Symbola as a fallback for icons.
MAIN_FONT=""
MONO_FONT=""
if [[ "$PDF_ENGINE" != "pdflatex" ]]; then
  # Try each preferred font in priority order using fc-list
  for f in "DejaVu Serif" "FreeSerif" "Noto Serif"; do
    if fc-list : family | grep -iq "$f"; then
      MAIN_FONT="$f"
      break
    fi
  done
  # Fallback: check filesystem directly if fc-list is inconsistent
  if [[ -z "$MAIN_FONT" ]]; then
    if [[ -f "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf" ]] || \
       [[ -f "/usr/share/fonts/texlive-dejavu/DejaVuSerif.ttf" ]]; then
      MAIN_FONT="DejaVu Serif"
    elif [[ -f "/usr/share/fonts/truetype/freefont/FreeSerif.ttf" ]] || \
         [[ -f "/usr/share/fonts/truetype/FreeSerif.ttf" ]]; then
      MAIN_FONT="FreeSerif"
    fi
  fi

  for f in "DejaVu Sans Mono" "FreeMono" "Noto Mono"; do
    if fc-list : family | grep -iq "$f"; then
      MONO_FONT="$f"
      break
    fi
  done
fi

if [[ -n "$MAIN_FONT" ]]; then
  echo "Main font  : $MAIN_FONT"
fi
if [[ -n "$MONO_FONT" ]]; then
  echo "Mono font  : $MONO_FONT"
fi
if [[ "$PDF_ENGINE" == "pdflatex" ]]; then
  echo "WARNING: pdflatex does not support all Unicode characters in the wiki."
  echo "         Install lualatex or xelatex for best results."
elif [[ -z "$MAIN_FONT" ]]; then
  echo "WARNING: No suitable Unicode font found (DejaVu Serif, FreeSerif)."
  echo "         Icons like ⚙, ★, and ☄ may not appear in the PDF."
fi

# ── Preamble for line-breaking and microtype ──────────────────────────────────
HEADER_FILE="/tmp/hc-manual-header.tex"
cat <<EOF > "$HEADER_FILE"
\usepackage{microtype}
\sloppy
% Symbol fallback for specific wiki icons (requires Symbola font)
\usepackage{newunicodechar}
\usepackage{fontspec}
\IfFontExistsTF{Symbola}{
  \newfontfamily\symbolafont{Symbola}
  \newunicodechar{⚙}{{\symbolafont ⚙}}
  \newunicodechar{★}{{\symbolafont ★}}
  \newunicodechar{☄}{{\symbolafont ☄}}
}{
  % Fallback if Symbola is missing
}
EOF

echo "Building HamClock-Next User Manual → $OUTPUT"
echo "Wiki source: $WIKI"

# ── Ordered chapter list ──────────────────────────────────────────────────────
# Intentionally excluded:
#   Widget-Gallery.md  — screenshot-only page, inline images make PDF too large
#   _Sidebar.md        — GitHub wiki navigation, not prose content
#   Developer-Guide.md — internal architecture/MCP docs, not user-facing
CHAPTERS=(
  "$WIKI/Home.md"
  "$WIKI/Getting-Started.md"
  "$WIKI/Building-from-Source.md"
  "$WIKI/Raspberry-Pi-Guide.md"
  "$WIKI/Migrating-from-HamClock.md"
  "$WIKI/New-Features.md"
  "$WIKI/Layout.md"
  "$WIKI/Widgets.md"
  "$WIKI/Widget-Setup.md"
  "$WIKI/Pane-Customization.md"
  "$WIKI/Presets.md"
  "$WIKI/Map-and-Overlays.md"
  "$WIKI/Configuration.md"
  "$WIKI/Keyboard-Shortcuts.md"
  "$WIKI/REST-API.md"
  "$WIKI/Data-Sources.md"
  "$WIKI/Troubleshooting-and-FAQ.md"
  "$WIKI/Glossary.md"
)

if [[ -n "$PREPEND_FILE" ]]; then
  CHAPTERS=("$PREPEND_FILE" "${CHAPTERS[@]}")
fi

# Verify all source files exist before starting
MISSING=0
for f in "${CHAPTERS[@]}"; do
  if [[ ! -f "$f" ]]; then
    echo "WARNING: Missing chapter file: $f"
    MISSING=$((MISSING + 1))
  fi
done
if [[ $MISSING -gt 0 ]]; then
  echo "WARNING: $MISSING chapter file(s) not found — they will be skipped."
fi

# ── Cover page ────────────────────────────────────────────────────────────────
COVER_FILE=""
if [[ -n "$COVER_IMAGE" ]]; then
  if [[ ! -f "$COVER_IMAGE" ]]; then
    echo "WARNING: --cover-image file not found: $COVER_IMAGE (skipping cover page)"
    COVER_IMAGE=""
  else
    VERSION_LABEL=$(cat "$REPO_ROOT/VERSION" 2>/dev/null || echo "")
    DATE_LABEL=$(date "+%B %Y")
    COVER_FILE="$(mktemp /tmp/hc-cover-XXXXXX.tex)"
    VERSION_LINE=""
    if [[ -n "$VERSION_LABEL" ]]; then
      VERSION_LINE="{\\large Version ${VERSION_LABEL}}\\\\[0.5cm]"
    fi
    cat > "$COVER_FILE" << LATEX_EOF
\begin{titlepage}
\centering
\vspace*{2.5cm}
\includegraphics[width=5cm]{${COVER_IMAGE}}\\\\[2cm]
{\Huge\bfseries HamClock-Next}\\\\[0.5cm]
{\Large\itshape User Manual}\\\\[1.5cm]
${VERSION_LINE}
{\large ${DATE_LABEL}}
\vfill
{\normalsize HamClock-Next Contributors}
\end{titlepage}
\setcounter{page}{2}
LATEX_EOF
    echo "Cover page : $COVER_IMAGE"
  fi
fi

# ── Build ─────────────────────────────────────────────────────────────────────
# Pass font variables if using a Unicode engine
PANDOC_ARGS=(
  -V title-meta="HamClock-Next User Manual"
  -V author-meta="HamClock-Next Contributors"
  --metadata date="$(date +%Y-%m-%d)"
  --metadata lang="en-US"
  --pdf-engine="$PDF_ENGINE"
  --lua-filter="$SCRIPT_DIR/fix-wiki-links.lua"
  --include-in-header="$HEADER_FILE"
  --resource-path="$WIKI:$WIKI/images"
  --toc
  --toc-depth=2
  --number-sections
  -V geometry:margin=1in
  -V colorlinks=true
  -V linkcolor=blue
  -V urlcolor=blue
)

if [[ -n "$MAIN_FONT" ]]; then
  PANDOC_ARGS+=(-V mainfont="$MAIN_FONT")
fi
if [[ -n "$MONO_FONT" ]]; then
  PANDOC_ARGS+=(-V monofont="$MONO_FONT")
fi
if [[ -n "$COVER_FILE" ]]; then
  PANDOC_ARGS+=(--include-before-body="$COVER_FILE")
fi

# Run pandoc
set +e
pandoc "${PANDOC_ARGS[@]}" -o "$OUTPUT" "${CHAPTERS[@]}"
EXIT_CODE=$?
set -e

# Cleanup
rm -f "$HEADER_FILE" "${COVER_FILE:-}"

if [[ $EXIT_CODE -eq 0 ]]; then
  echo ""
  echo "Done: $OUTPUT"
else
  echo ""
  echo "ERROR: Pandoc failed with exit code $EXIT_CODE"
  exit $EXIT_CODE
fi

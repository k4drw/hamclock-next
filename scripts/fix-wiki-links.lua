-- fix-wiki-links.lua
-- Pandoc Lua filter: rewrite inter-wiki markdown links so they become
-- internal PDF anchors rather than broken file references.
--
-- Handles:
--   [text](Page.md)         → [text](#section-id)
--   [text](Page.md#anchor)  → [text](#anchor)
--   http/https URLs         → unchanged
--   images / non-.md links  → unchanged

-- Mapping from wiki filename → pandoc section ID.
-- IDs are derived from each file's first H1 heading using pandoc's algorithm:
--   strip non-alphanumeric/non-hyphen/non-underscore/non-period chars,
--   collapse to words, join with hyphens, lowercase.
local WIKI_MAP = {
  ["Building-from-Source.md"]   = "building-from-source",
  ["Configuration.md"]          = "setup-configuration",
  ["Data-Sources.md"]           = "data-sources-network",
  ["Getting-Started.md"]        = "getting-started",
  ["Glossary.md"]               = "glossary-of-technical-terms",
  ["Home.md"]                   = "hamclock-next",
  ["Keyboard-Shortcuts.md"]     = "keyboard-shortcuts",
  ["Layout.md"]                 = "screen-layout",
  ["Map-and-Overlays.md"]       = "map-overlays",
  ["Migrating-from-HamClock.md"]= "migrating-from-original-hamclock",
  ["New-Features.md"]           = "new-features-in-hamclock-next",
  ["Pane-Customization.md"]     = "pane-customization-rotation",
  ["Presets.md"]                = "configuration-presets",
  ["REST-API.md"]               = "hamclock-next-rest-api",
  ["Widget-Gallery.md"]         = "widget-gallery",
  ["Widget-Setup.md"]           = "widget-setup-guide",
  ["Widgets.md"]                = "widgets-reference",
}

-- Normalize heading identifiers: collapse -- (and longer runs) to a single -
-- to avoid LaTeX hyperref's -- -> en-dash ligature mangling anchor names.
function Header(el)
  if el.identifier and el.identifier ~= "" then
    el.identifier = el.identifier:gsub("%-%-+", "-")
  end
  return el
end

function Link(el)
  local target = el.target

  -- Leave absolute URLs alone
  if target:match("^https?://") then
    return el
  end

  -- For internal #anchor links, normalize -- to - for the same reason.
  if target:match("^#") then
    el.target = target:gsub("%-%-+", "-")
    return el
  end

  -- Extract filename and optional in-page anchor
  local file, anchor = target:match("^([^#]+%.md)#?(.*)$")
  if not file then
    return el  -- not a .md link
  end

  -- Strip any leading path (images/ etc. won't match WIKI_MAP anyway)
  local basename = file:match("([^/]+)$") or file

  if anchor and anchor ~= "" then
    -- Explicit in-page anchor: use it directly
    el.target = "#" .. anchor
  elseif WIKI_MAP[basename] then
    el.target = "#" .. WIKI_MAP[basename]
  else
    -- Fallback: derive from filename (lowercase, hyphens)
    local stem = basename:match("^(.-)%.md$") or basename
    el.target = "#" .. stem:lower():gsub("[^%a%d%-]", "-"):gsub("%-+", "-")
  end

  return el
end

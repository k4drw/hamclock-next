-- Insert \clearpage before each level-1 heading except the first.
-- Each chapter starts at the top of a fresh page, which also flushes any
-- pending floats from the previous chapter before the page break.
function Header(el)
  if el.level == 1 then
    return {pandoc.RawBlock('latex', '\\clearpage'), el}
  end
end

-- Drop markdown horizontal rules (---) — numbered sections make them redundant in PDF.
function HorizontalRule()
  return {}
end

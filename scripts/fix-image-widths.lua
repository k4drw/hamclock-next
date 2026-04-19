-- Constrain images inside table cells to fill the cell width.
-- Prevents large screenshots from overflowing 2-column table layouts in the PDF.
function Table(t)
  return t:walk({
    Image = function(img)
      if not img.attributes.width then
        img.attributes.width = "100%"
      end
      return img
    end
  })
end

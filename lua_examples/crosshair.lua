-- ============================================================
--  crosshair.lua
-- ============================================================

local COLOR_LINE = { r = 255, g = 255, b = 255, a = 230 }
local COLOR_DOT  = { r = 0,   g = 200, b = 255, a = 255 }

local GAP    = 6   
local LENGTH = 10  
local THICK  = 1   

local function onRender()
    if not engine.IsInGame() then return end

    local w, h = engine.GetScreenSize()
    local cx, cy = math.floor(w / 2), math.floor(h / 2)


    draw.Rect(cx - GAP - LENGTH, cy - THICK, LENGTH, THICK * 2 + 1, COLOR_LINE, true) 
    draw.Rect(cx + GAP,          cy - THICK, LENGTH, THICK * 2 + 1, COLOR_LINE, true)
    draw.Rect(cx - THICK, cy - GAP - LENGTH, THICK * 2 + 1, LENGTH, COLOR_LINE, true) 
    draw.Rect(cx - THICK, cy + GAP,          THICK * 2 + 1, LENGTH, COLOR_LINE, true) 

    draw.Circle(cx, cy, 1.5, COLOR_DOT, 8, true)
end

callbacks.Register("OnRender", "crosshair", onRender)
print("[crosshair] loaded")

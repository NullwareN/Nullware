-- ============================================================
--  crosshair.lua
--  วาด crosshair กลางจอแบบ custom (dot + lines)
-- ============================================================

local COLOR_LINE = { r = 255, g = 255, b = 255, a = 230 }
local COLOR_DOT  = { r = 0,   g = 200, b = 255, a = 255 }

local GAP    = 6   -- ช่องว่างตรงกลาง
local LENGTH = 10  -- ความยาวเส้น
local THICK  = 1   -- ความหนา

local function onRender()
    if not engine.IsInGame() then return end

    local w, h = engine.GetScreenSize()
    local cx, cy = math.floor(w / 2), math.floor(h / 2)

    -- เส้น
    draw.Rect(cx - GAP - LENGTH, cy - THICK, LENGTH, THICK * 2 + 1, COLOR_LINE, true) -- ซ้าย
    draw.Rect(cx + GAP,          cy - THICK, LENGTH, THICK * 2 + 1, COLOR_LINE, true) -- ขวา
    draw.Rect(cx - THICK, cy - GAP - LENGTH, THICK * 2 + 1, LENGTH, COLOR_LINE, true) -- บน
    draw.Rect(cx - THICK, cy + GAP,          THICK * 2 + 1, LENGTH, COLOR_LINE, true) -- ล่าง

    -- จุดกลาง
    draw.Circle(cx, cy, 1.5, COLOR_DOT, 8, true)
end

callbacks.Register("OnRender", "crosshair", onRender)
print("[crosshair] loaded")

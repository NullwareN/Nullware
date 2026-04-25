-- ============================================================
--  esp_healthbar.lua (FIXED)
-- ============================================================

local function lerp(a, b, t)
    return a + (b - a) * t
end

local function clamp(v, min, max)
    if v < min then return min end
    if v > max then return max end
    return v
end

local function healthColor(hp, maxhp)
    if maxhp <= 0 then return { r = 255, g = 0, b = 0, a = 220 } end

    local pct = clamp(hp / maxhp, 0, 1)

    local r = math.floor(lerp(255, 50, pct))
    local g = math.floor(lerp(50, 200, pct))

    return { r = r, g = g, b = 30, a = 220 }
end

local function onRender()
    if not engine.IsInGame() then return end

    local enemies = entities.GetEnemyPlayers()
    if not enemies or #enemies == 0 then return end

    for _, ent in ipairs(enemies) do
        if not ent:IsAlive() or ent:IsDormant() then goto continue end

        local origin = ent:GetOrigin()
        local head   = ent:GetEyePosition()

        local fx, fy = engine.WorldToScreen(origin.x, origin.y, origin.z)
        local hx, hy = engine.WorldToScreen(head.x,   head.y,   head.z)

        -- FIX: strict check
        if not fx or not fy or not hx or not hy then goto continue end

        local hp    = ent:GetHealth()
        local maxhp = ent:GetMaxHealth()
        if maxhp <= 0 then goto continue end

        local pct = clamp(hp / maxhp, 0, 1)

        local height = math.abs(fy - hy)
        if height < 5 then goto continue end -- prevent tiny glitch bars

        local barW = 4
        local barX = math.floor(hx - height * 0.35) - barW - 2

        -- background
        draw.Rect(barX, hy, barW, height, { r=0, g=0, b=0, a=180 }, true)

        -- fill
        local fillH = math.floor(height * pct)
        draw.Rect(barX, hy + (height - fillH), barW, fillH, healthColor(hp, maxhp), true)

        -- hp text
        local txt = tostring(hp)
        local tw, th = draw.GetTextSize(txt)
        draw.Text(math.floor(hx - tw / 2), math.floor(hy - th - 2), txt, { r=255, g=255, b=255, a=255 }, true)

        ::continue::
    end
end

callbacks.Register("OnRender", "esp_healthbar", onRender)

print("[esp_healthbar] loaded (fixed)")
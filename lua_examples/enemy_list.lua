-- ============================================================
--  enemy_list.lua
--  แสดงรายชื่อศัตรูทางซ้ายมือ พร้อม HP และ class
-- ============================================================

local CLASS_NAMES = {
    [1]="Scout", [2]="Sniper", [3]="Soldier", [4]="Demo",
    [5]="Medic", [6]="Heavy",  [7]="Pyro",   [8]="Spy",
    [9]="Engineer"
}

local PANEL_X = 10
local PANEL_Y = 200
local LINE_H  = 16
local PADDING = 6

local function hpToColor(hp, maxhp)
    local p = hp / maxhp
    if p > 0.6 then return { r=80, g=220, b=80, a=255 }
    elseif p > 0.3 then return { r=230, g=200, b=30, a=255 }
    else return { r=230, g=60, b=60, a=255 } end
end

local function onRender()
    if not engine.IsInGame() then return end

    local enemies = entities.GetEnemyPlayers()
    if #enemies == 0 then return end

    -- กรองเฉพาะ alive + not dormant
    local alive = {}
    for _, e in ipairs(enemies) do
        if e:IsAlive() and not e:IsDormant() then
            alive[#alive + 1] = e
        end
    end
    if #alive == 0 then return end

    -- วาดพื้นหลังพาเนล
    local panelH = #alive * LINE_H + PADDING * 2
    draw.Rect(PANEL_X, PANEL_Y, 140, panelH, { r=0, g=0, b=0, a=160 }, true)

    for i, e in ipairs(alive) do
        local name    = e:GetName() or "Unknown"
        local hp      = e:GetHealth()
        local maxhp   = e:GetMaxHealth()
        local cls     = CLASS_NAMES[e:GetClass()] or "?"
        local y       = PANEL_Y + PADDING + (i - 1) * LINE_H

        -- ชื่อ + class
        local label = string.format("[%s] %s", cls, name)
        if #label > 18 then label = label:sub(1, 17) .. "…" end
        draw.Text(PANEL_X + PADDING, y, label, { r=220, g=220, b=220, a=255 }, false)

        -- HP สี
        local hpTxt = string.format("%d/%d", hp, maxhp)
        local tw = draw.GetTextSize(hpTxt)
        draw.Text(PANEL_X + 140 - tw - PADDING, y, hpTxt, hpToColor(hp, maxhp), false)
    end
end

callbacks.Register("OnRender", "enemy_list", onRender)
print("[enemy_list] loaded — แสดงศัตรู " .. #(entities.GetEnemyPlayers()) .. " คน")

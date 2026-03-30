--[[ 
    Auto Queue for Amalgam V2
    Author: tom lua
]]

-- Configuration
local bAutoQueue = true
local fLastTime = 0
local nCasualGroup = 7 -- k_eTFMatchGroup_Casual_12v12
local bWasInLobby = false

-- Auto Queue Logic
local function Draw_AutoQueue()
    local bInLobby = Amalgam.GC.InLobby() or engine.IsInGame()

    -- Sound effect when match is found or joined
    if bInLobby and not bWasInLobby then
        local mapName = engine.GetLevelName()
        engine.Notification("Match Found!", "Joining: " .. mapName)
        engine.PlaySound("ui/matchmaking_game_found.wav")
        bWasInLobby = true
    elseif not bInLobby then
        bWasInLobby = false
    end

    -- If disabled, or in lobby, or connecting: skip
    if not bAutoQueue or bInLobby or engine.IsConnected() then
        return
    end

    -- 5 second delay to avoid server spam
    local curTime = engine.GetPlatFloatTime()
    if curTime - fLastTime < 5 then
        return
    end
    fLastTime = curTime

    -- If not in queue, request to join casual matchmaking
    if not Amalgam.Party.InQueue(nCasualGroup) then
        print("[AutoQueue] Searching for a new match...")
        Amalgam.Party.QueueUp(nCasualGroup)
        engine.PlaySound("ui/buttonclick.wav")
    end
end

-- UI and Callbacks
callbacks.Register("OnRender", "AutoQueueUI", function()
    -- Create settings menu in Amalgam UI
    if ui.Begin("Auto Queue Settings") then
        local pressed, active = ui.Checkbox("Enable AutoQueue", bAutoQueue)
        if pressed then 
            bAutoQueue = active 
            print("[AutoQueue] Status: " .. (bAutoQueue and "ON" or "OFF"))
            engine.PlaySound("ui/buttonclickrelease.wav")
        end
        ui.End()
    end
    
    Draw_AutoQueue()
end)

-- Startup Notification
engine.Notification("AutoQueue System", "By tom lua - Ready to search!")
engine.PlaySound("vo/heavy_ka_boom01.wav")
print("[AutoQueue] Enhanced version by tom lua loaded.")
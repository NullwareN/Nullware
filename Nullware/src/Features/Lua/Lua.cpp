#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include "WinHeaders.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../Players/PlayerUtils.h"
#include "../../../include/ImGui/imgui.h"
#include "../../../include/ImGui/imgui_internal.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "../../SDK/SDK.h"
#include "LuaFeature.h"
#include "../Visuals/Notifications/Notifications.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#if defined(_MSVC_LANG) && _MSVC_LANG < 201703L
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif


/* -----------------------------------------------------------------------
** Output redirection
** --------------------------------------------------------------------- */
static std::string g_LuaOutputBuffer;
static std::mutex g_LuaOutputMutex;
extern "C" void lua_output_redirect(const char* s, size_t l) {
    std::lock_guard<std::mutex> lock(g_LuaOutputMutex);
    g_LuaOutputBuffer.append(s, l);
}

extern "C" void lua_line_redirect() {
    std::lock_guard<std::mutex> lock(g_LuaOutputMutex);
    if (g_LuaOutputBuffer.empty()) return;
    SDK::Output("Lua", g_LuaOutputBuffer.c_str(), { 175, 150, 255, 255 }, OUTPUT_CONSOLE | OUTPUT_MENU);
    g_LuaOutputBuffer.clear();
}

static ImU32 GetImColor(lua_State *L, int idx) {
  if (lua_istable(L, idx)) {
    lua_getfield(L, idx, "r");
    int r = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : 255;
    lua_getfield(L, idx, "g");
    int g = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : 255;
    lua_getfield(L, idx, "b");
    int b = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : 255;
    lua_getfield(L, idx, "a");
    int a = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : 255;
    lua_pop(L, 4);
    return IM_COL32(r, g, b, a);
  } else if (lua_isnumber(L, idx)) {
    return (ImU32)lua_tointeger(L, idx);
  }
  return IM_COL32(255, 255, 255, 255);
}

static int lua_print(lua_State *L) {
  int n = lua_gettop(L);
  std::string out = "";
  for (int i = 1; i <= n; i++) {
    const char *s = lua_tostring(L, i);
    if (s) {
      if (i > 1)
        out += "    ";
      out += s;
    }
  }
  SDK::Output("Lua", out.c_str(), {255, 255, 255, 255}, OUTPUT_CONSOLE);
  return 0;
}

static int lua_error_handler(lua_State *L) {
  const char *err = lua_tostring(L, 1);
  if (err) {
    luaL_traceback(L, L, err, 1);
    const char *trace = lua_tostring(L, -1);
    SDK::Output("Lua Error", trace, {255, 50, 50, 255}, OUTPUT_CONSOLE);
    lua_pop(L, 1);
  }
  return 0;
}

// --- Vector Object ---
static int lua_vector_new(lua_State *L) {
  float x = (float)luaL_optnumber(L, 1, 0.0);
  float y = (float)luaL_optnumber(L, 2, 0.0);
  float z = (float)luaL_optnumber(L, 3, 0.0);
  lua_createtable(L, 0, 3);
  lua_pushnumber(L, x);
  lua_setfield(L, -2, "x");
  lua_pushnumber(L, y);
  lua_setfield(L, -2, "y");
  lua_pushnumber(L, z);
  lua_setfield(L, -2, "z");
  luaL_getmetatable(L, "Vector");
  lua_setmetatable(L, -2);
  return 1;
}

static int lua_vector_add(lua_State *L) {
  if (!lua_istable(L, 1) || !lua_istable(L, 2)) return 0;
  lua_getfield(L, 1, "x"); float x1 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 1, "y"); float y1 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 1, "z"); float z1 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 2, "x"); float x2 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 2, "y"); float y2 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 2, "z"); float z2 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_pushcfunction(L, lua_vector_new);
  lua_pushnumber(L, x1 + x2);
  lua_pushnumber(L, y1 + y2);
  lua_pushnumber(L, z1 + z2);
  lua_call(L, 3, 1);
  return 1;
}

static int lua_vector_sub(lua_State *L) {
  if (!lua_istable(L, 1) || !lua_istable(L, 2)) return 0;
  lua_getfield(L, 1, "x"); float x1 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 1, "y"); float y1 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 1, "z"); float z1 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 2, "x"); float x2 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 2, "y"); float y2 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 2, "z"); float z2 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_pushcfunction(L, lua_vector_new);
  lua_pushnumber(L, x1 - x2);
  lua_pushnumber(L, y1 - y2);
  lua_pushnumber(L, z1 - z2);
  lua_call(L, 3, 1);
  return 1;
}

static int lua_vector_mul(lua_State *L) {
  if (!lua_istable(L, 1) || !lua_isnumber(L, 2)) return 0;
  lua_getfield(L, 1, "x"); float x1 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 1, "y"); float y1 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  lua_getfield(L, 1, "z"); float z1 = (float)lua_tonumber(L, -1); lua_pop(L, 1);
  float m = (float)lua_tonumber(L, 2);
  lua_pushcfunction(L, lua_vector_new);
  lua_pushnumber(L, x1 * m);
  lua_pushnumber(L, y1 * m);
  lua_pushnumber(L, z1 * m);
  lua_call(L, 3, 1);
  return 1;
}

// --- Entity Object ---
static CBaseEntity *GetEntityFromLua(lua_State *L, int idx) {
  if (!I::EngineClient->IsInGame() || !lua_istable(L, idx))
    return nullptr;
  
  // Fast raw fetch using integer lookup
  lua_rawgeti(L, idx, 1);
  if (!lua_isnumber(L, -1)) {
    lua_pop(L, 1);
    return nullptr;
  }
  int entindex = (int)lua_tointeger(L, -1);
  lua_pop(L, 1);
  
  if (entindex <= 0 || entindex > 2048)
    return nullptr;
  auto pClientEnt = I::ClientEntityList->GetClientEntity(entindex);
  if (!pClientEnt)
    return nullptr;
  return pClientEnt->As<CBaseEntity>();
}

static int lua_entity_get_index(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt)
    return 0;
  lua_pushinteger(L, pEnt->entindex());
  return 1;
}

static int lua_entity_get_class_id(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt)
    return 0;
  lua_pushinteger(L, (int)pEnt->GetClassID());
  return 1;
}

static int lua_entity_get_team(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt)
    return 0;
  lua_pushinteger(L, pEnt->m_iTeamNum());
  return 1;
}

static int lua_entity_get_health(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt)
    return 0;
  if (pEnt->IsPlayer())
    lua_pushinteger(L, pEnt->As<CBasePlayer>()->m_iHealth());
  else if (pEnt->IsBaseObject())
    lua_pushinteger(L, pEnt->As<CBaseObject>()->m_iHealth());
  else
    lua_pushinteger(L, 0);
  return 1;
}

static int lua_entity_get_max_health(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt)
    return 0;
  if (pEnt->IsPlayer())
    lua_pushinteger(L, pEnt->As<CTFPlayer>()->GetMaxHealth());
  else if (pEnt->IsBaseObject())
    lua_pushinteger(L, pEnt->As<CBaseObject>()->m_iMaxHealth());
  else
    lua_pushinteger(L, 0);
  return 1;
}

static int lua_entity_get_origin(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt)
    return 0;
  Vec3 origin = pEnt->m_vecOrigin();
  lua_pushcfunction(L, lua_vector_new);
  lua_pushnumber(L, origin.x);
  lua_pushnumber(L, origin.y);
  lua_pushnumber(L, origin.z);
  lua_call(L, 3, 1);
  return 1;
}

static int lua_entity_is_alive(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt)
    return 0;
  int health = 0;
  if (pEnt->IsPlayer())
    health = pEnt->As<CBasePlayer>()->m_iHealth();
  else if (pEnt->IsBaseObject())
    health = pEnt->As<CBaseObject>()->m_iHealth();
  lua_pushboolean(L, health > 0);
  return 1;
}

static int lua_entity_is_dormant(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt)
    return 0;
  lua_pushboolean(L, pEnt->IsDormant());
  return 1;
}

static int lua_entity_get_name(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt || !pEnt->IsPlayer()) {
    lua_pushnil(L);
    return 1;
  }
  
  const char* pName = F::PlayerUtils.GetPlayerName(pEnt->entindex());
  if (pName) {
    lua_pushstring(L, pName);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int lua_entity_get_velocity(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt || !pEnt->IsPlayer())
    return 0;
  Vec3 vel = pEnt->As<CTFPlayer>()->m_vecVelocity();
  lua_pushcfunction(L, lua_vector_new);
  lua_pushnumber(L, vel.x);
  lua_pushnumber(L, vel.y);
  lua_pushnumber(L, vel.z);
  lua_call(L, 3, 1);
  return 1;
}

static int lua_entity_get_view_offset(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt || !pEnt->IsPlayer())
    return 0;
  Vec3 vo = pEnt->As<CTFPlayer>()->GetViewOffset();
  lua_pushcfunction(L, lua_vector_new);
  lua_pushnumber(L, vo.x);
  lua_pushnumber(L, vo.y);
  lua_pushnumber(L, vo.z);
  lua_call(L, 3, 1);
  return 1;
}

static int lua_entity_get_class(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt || !pEnt->IsPlayer()) {
    lua_pushinteger(L, 0);
    return 1;
  }
  lua_pushinteger(L, pEnt->As<CTFPlayer>()->m_iClass());
  return 1;
}

static int lua_entity_is_player(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  lua_pushboolean(L, pEnt && pEnt->IsPlayer());
  return 1;
}

static int lua_entity_is_building(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  lua_pushboolean(L, pEnt && pEnt->IsBuilding());
  return 1;
}

static int lua_entity_is_on_ground(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt || !pEnt->IsPlayer()) {
    lua_pushboolean(L, false);
    return 1;
  }
  lua_pushboolean(L, pEnt->As<CTFPlayer>()->IsOnGround());
  return 1;
}

static int lua_entity_in_cond(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (lua_gettop(L) < 2 || !lua_isnumber(L, 2)) {
    lua_pushboolean(L, false);
    return 1;
  }
  int cond = (int)lua_tointeger(L, 2);
  if (!pEnt || !pEnt->IsPlayer()) {
    lua_pushboolean(L, false);
    return 1;
  }
  lua_pushboolean(L, pEnt->As<CTFPlayer>()->InCond((ETFCond)cond));
  return 1;
}

static int lua_entity_get_netvar(lua_State *L) {
  auto pEnt = GetEntityFromLua(L, 1);
  if (!pEnt)
    return 0;
  if (lua_gettop(L) < 3 || !lua_isstring(L, 2) || !lua_isstring(L, 3))
    return 0;
  const char *table = lua_tostring(L, 2);
  const char *var = lua_tostring(L, 3);
  const char *type = lua_isstring(L, 4) ? lua_tostring(L, 4) : "int";

  int offset = U::NetVars.GetNetVar(table, var);
  if (!offset)
    return 0;

  void *addr = (void *)(uintptr_t(pEnt) + offset);
  if (strcmp(type, "int") == 0)
    lua_pushinteger(L, *(int *)addr);
  else if (strcmp(type, "float") == 0)
    lua_pushnumber(L, *(float *)addr);
  else if (strcmp(type, "bool") == 0)
    lua_pushboolean(L, *(bool *)addr);
  else if (strcmp(type, "string") == 0)
    lua_pushstring(L, (const char *)addr);
  else if (strcmp(type, "vector") == 0) {
    Vec3 v = *(Vec3 *)addr;
    lua_pushcfunction(L, lua_vector_new);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    lua_call(L, 3, 1);
  } else
    return 0;

  return 1;
}

static char s_EntityCacheKey;

static int PushEntity(lua_State *L, CBaseEntity *pEnt) {
  if (!pEnt || !I::EngineClient->IsInGame()) {
    lua_pushnil(L);
    return 1;
  }
  int idx = pEnt->entindex();
  if (idx <= 0) {
    lua_pushnil(L);
    return 1;
  }
  
  // Try to push from cache
  lua_pushlightuserdata(L, &s_EntityCacheKey);
  lua_rawget(L, LUA_REGISTRYINDEX);
  
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushlightuserdata(L, &s_EntityCacheKey);
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
  }
  
  lua_rawgeti(L, -1, idx);
  if (lua_istable(L, -1)) {
    lua_remove(L, -2); // remove cache table
    return 1;
  }
  lua_pop(L, 1); // pop nil
  
  // Create table
  lua_createtable(L, 1, 1);
  lua_pushinteger(L, idx);
  lua_rawseti(L, -2, 1); // Fast C++ index
  lua_pushinteger(L, idx);
  lua_setfield(L, -2, "__index_id"); // Backward compat
  luaL_getmetatable(L, "Entity");
  lua_setmetatable(L, -2);
  
  // Cache it
  lua_pushvalue(L, -1);
  lua_rawseti(L, -3, idx);
  lua_remove(L, -2); // remove cache table
  
  return 1;
}

static int lua_entity_set_netvar(lua_State *L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) return 0;
    if (lua_gettop(L) < 4 || !lua_isstring(L, 2) || !lua_isstring(L, 3))
        return 0;
    const char* table = lua_tostring(L, 2);
    const char* var = lua_tostring(L, 3);
    const char* type = lua_isstring(L, 5) ? lua_tostring(L, 5) : "int";

    int offset = U::NetVars.GetNetVar(table, var);
    if (!offset) return 0;

    void* addr = (void*)(uintptr_t(pEnt) + offset);
    if (strcmp(type, "int") == 0)
        *(int*)addr = (int)lua_tointeger(L, 4);
    else if (strcmp(type, "float") == 0)
        *(float*)addr = (float)lua_tonumber(L, 4);
    else if (strcmp(type, "bool") == 0)
        *(bool*)addr = lua_toboolean(L, 4);
    else if (strcmp(type, "vector") == 0) {
        if (lua_istable(L, 4)) {
            lua_getfield(L, 4, "x"); float x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, 4, "y"); float y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_getfield(L, 4, "z"); float z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            *(Vec3*)addr = { x, y, z };
        }
    }
    return 0;
}

static int lua_entity_get_abs_origin(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) return 0;
    Vec3 pos = pEnt->GetAbsOrigin();
    lua_pushcfunction(L, lua_vector_new);
    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    lua_pushnumber(L, pos.z);
    lua_call(L, 3, 1);
    return 1;
}

// --- Entities Table ---
static int lua_entities_get_local(lua_State *L) {
  if (!I::EngineClient->IsInGame()) {
    lua_pushnil(L);
    return 1;
  }
  return PushEntity(L, (CBaseEntity *)H::Entities.GetLocal());
}

static int lua_entities_get_by_index(lua_State *L) {
  if (!I::EngineClient->IsInGame()) {
    lua_pushnil(L);
    return 1;
  }
  if (lua_gettop(L) < 1 || !lua_isnumber(L, 1)) {
    lua_pushnil(L);
    return 1;
  }
  int idx = (int)lua_tointeger(L, 1);
  auto pClientEnt = I::ClientEntityList->GetClientEntity(idx);
  if (!pClientEnt) {
    lua_pushnil(L);
    return 1;
  }
  return PushEntity(L, pClientEnt->As<CBaseEntity>());
}

static int lua_entities_get_players(lua_State *L) {
  if (!I::EngineClient->IsInGame()) { lua_newtable(L); return 1; }
  lua_createtable(L, I::EngineClient->GetMaxClients(), 0);
  int i = 1;
  for (int n = 1; n <= I::EngineClient->GetMaxClients(); n++) {
    auto pClientEnt = I::ClientEntityList->GetClientEntity(n);
    if (!pClientEnt) continue;
    auto pEnt = pClientEnt->As<CBaseEntity>();
    if (!pEnt->IsPlayer()) continue;
    PushEntity(L, pEnt);
    lua_rawseti(L, -2, i++);
  }
  return 1;
}

static int lua_entities_get_enemy_players(lua_State *L) {
  if (!I::EngineClient->IsInGame()) { lua_newtable(L); return 1; }
  auto pLocal = H::Entities.GetLocal();
  if (!pLocal) { lua_newtable(L); return 1; }
  lua_createtable(L, I::EngineClient->GetMaxClients(), 0);
  int i = 1;
  for (int n = 1; n <= I::EngineClient->GetMaxClients(); n++) {
    auto pClientEnt = I::ClientEntityList->GetClientEntity(n);
    if (!pClientEnt) continue;
    auto pEnt = pClientEnt->As<CBaseEntity>();
    if (!pEnt->IsPlayer() || pEnt == pLocal) continue;
    if (pEnt->As<CTFPlayer>()->m_iTeamNum() == pLocal->m_iTeamNum()) continue;
    PushEntity(L, pEnt);
    lua_rawseti(L, -2, i++);
  }
  return 1;
}

static int lua_entities_get_buildings(lua_State *L) {
  if (!I::EngineClient->IsInGame()) { lua_newtable(L); return 1; }
  auto pLocal = H::Entities.GetLocal();
  if (!pLocal) { lua_newtable(L); return 1; }

  bool bEnemyOnly = lua_isboolean(L, 1) ? lua_toboolean(L, 1) : true;
  lua_createtable(L, 64, 0); // Preallocate size mapping
  int i = 1;

  for (int n = I::EngineClient->GetMaxClients() + 1; n <= I::ClientEntityList->GetHighestEntityIndex(); n++) {
    auto pClientEnt = I::ClientEntityList->GetClientEntity(n);
    if (!pClientEnt) continue;
    auto pEnt = pClientEnt->As<CBaseEntity>();
    if (!pEnt->IsBaseObject()) continue;
    if (bEnemyOnly && pEnt->m_iTeamNum() == pLocal->m_iTeamNum()) continue;
    PushEntity(L, pEnt);
    lua_rawseti(L, -2, i++);
  }
  return 1;
}

// --- Draw Table ---
static int lua_draw_line(lua_State *L) {
  if (lua_gettop(L) < 4 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) return 0;
  float x1 = (float)lua_tonumber(L, 1);
  float y1 = (float)lua_tonumber(L, 2);
  float x2 = (float)lua_tonumber(L, 3);
  float y2 = (float)lua_tonumber(L, 4);
  ImU32 color = GetImColor(L, 5);
  ImGui::GetBackgroundDrawList()->AddLine({x1, y1}, {x2, y2}, color);
  return 0;
}

static int lua_draw_rect(lua_State *L) {
  if (lua_gettop(L) < 4 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) return 0;
  float x = (float)lua_tonumber(L, 1);
  float y = (float)lua_tonumber(L, 2);
  float w = (float)lua_tonumber(L, 3);
  float h = (float)lua_tonumber(L, 4);
  ImU32 color = GetImColor(L, 5);
  bool filled = lua_toboolean(L, 6);
  if (filled)
    ImGui::GetBackgroundDrawList()->AddRectFilled({x, y}, {x + w, y + h},
                                                  color);
  else
    ImGui::GetBackgroundDrawList()->AddRect({x, y}, {x + w, y + h}, color);
  return 0;
}

static int lua_draw_text(lua_State *L) {
  if (lua_gettop(L) < 3 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isstring(L, 3)) return 0;
  float x = (float)lua_tonumber(L, 1);
  float y = (float)lua_tonumber(L, 2);
  const char *text = lua_tostring(L, 3);
  ImU32 color = GetImColor(L, 4);
  bool center = lua_toboolean(L, 5);
  if (center) {
    ImVec2 size = ImGui::CalcTextSize(text);
    ImGui::GetBackgroundDrawList()->AddText({x - size.x / 2.f, y}, color, text);
  } else {
    ImGui::GetBackgroundDrawList()->AddText({x, y}, color, text);
  }
  return 0;
}

static int lua_draw_circle(lua_State *L) {
  if (lua_gettop(L) < 3 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)) return 0;
  float x = (float)lua_tonumber(L, 1);
  float y = (float)lua_tonumber(L, 2);
  float radius = (float)lua_tonumber(L, 3);
  ImU32 color = GetImColor(L, 4);
  int segs = lua_isnumber(L, 5) ? (int)lua_tonumber(L, 5) : 32;
  bool filled = lua_toboolean(L, 6);
  if (filled)
    ImGui::GetBackgroundDrawList()->AddCircleFilled({x, y}, radius, color,
                                                     segs);
  else
    ImGui::GetBackgroundDrawList()->AddCircle({x, y}, radius, color, segs);
  return 0;
}

static int lua_draw_triangle(lua_State *L) {
  if (lua_gettop(L) < 6 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5) || !lua_isnumber(L, 6)) return 0;
  ImVec2 a = {(float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2)};
  ImVec2 b = {(float)lua_tonumber(L, 3), (float)lua_tonumber(L, 4)};
  ImVec2 c = {(float)lua_tonumber(L, 5), (float)lua_tonumber(L, 6)};
  ImU32 color = GetImColor(L, 7);
  bool filled = lua_toboolean(L, 8);
  if (filled)
    ImGui::GetBackgroundDrawList()->AddTriangleFilled(a, b, c, color);
  else
    ImGui::GetBackgroundDrawList()->AddTriangle(a, b, c, color);
  return 0;
}

static int lua_draw_get_text_size(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
  const char *text = lua_tostring(L, 1);
  ImVec2 sz = ImGui::CalcTextSize(text);
  lua_pushnumber(L, sz.x);
  lua_pushnumber(L, sz.y);
  return 2;
}

// --- Engine Table ---
static int lua_engine_get_local_player(lua_State *L) {
  lua_pushinteger(L, I::EngineClient->GetLocalPlayer());
  return 1;
}

static int lua_engine_is_in_game(lua_State *L) {
  lua_pushboolean(L, I::EngineClient->IsInGame());
  return 1;
}

static int lua_engine_is_connected(lua_State *L) {
  lua_pushboolean(L, I::EngineClient->IsConnected());
  return 1;
}

static int lua_engine_execute_cmd(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
  const char *cmd = lua_tostring(L, 1);
  I::EngineClient->ExecuteClientCmd(cmd);
  return 0;
}

static int lua_engine_get_screen_size(lua_State *L) {
  lua_pushinteger(L, H::Draw.m_nScreenW);
  lua_pushinteger(L, H::Draw.m_nScreenH);
  return 2;
}

static int lua_engine_get_view_angles(lua_State *L) {
  Vec3 va;
  I::EngineClient->GetViewAngles(va);
  lua_pushnumber(L, va.x);
  lua_pushnumber(L, va.y);
  lua_pushnumber(L, va.z);
  return 3;
}

static int lua_engine_set_view_angles(lua_State *L) {
  if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2)) return 0;
  Vec3 va;
  va.x = (float)lua_tonumber(L, 1);
  va.y = (float)lua_tonumber(L, 2);
  va.z = lua_isnumber(L, 3) ? (float)lua_tonumber(L, 3) : 0.0f;
  I::EngineClient->SetViewAngles(va);
  return 0;
}

static int lua_engine_get_tickcount(lua_State *L) {
  lua_pushinteger(L, I::GlobalVars->tickcount);
  return 1;
}

static int lua_engine_get_curtime(lua_State *L) {
  lua_pushnumber(L, I::GlobalVars->curtime);
  return 1;
}

static int lua_engine_get_max_clients(lua_State *L) {
  lua_pushinteger(L, I::EngineClient->GetMaxClients());
  return 1;
}

static int lua_engine_get_plat_float_time(lua_State *L) {
  lua_pushnumber(L, (double)SDK::PlatFloatTime());
  return 1;
}

static int lua_engine_con_command(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
    const char* cmd = lua_tostring(L, 1);
    I::EngineClient->ClientCmd_Unrestricted(cmd);
    return 0;
}

static int lua_engine_trace_line(lua_State* L) {
    if (lua_gettop(L) < 2 || !lua_istable(L, 1) || !lua_istable(L, 2)) return 0;
    Vec3 start, end;
    lua_getfield(L, 1, "x"); start.x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 1, "y"); start.y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 1, "z"); start.z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 2, "x"); end.x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 2, "y"); end.y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 2, "z"); end.z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    unsigned int mask = lua_isnumber(L, 3) ? (unsigned int)lua_tointeger(L, 3) : 0x4600400B; // MASK_SHOT
    
    class CLuaTraceFilter : public ITraceFilter {
    public:
        bool ShouldHitEntity(IHandleEntity* pServerEntity, int nContentsMask) override { return true; }
        TraceType_t GetTraceType() const override { return TRACE_EVERYTHING; }
    };

    CGameTrace trace;
    CLuaTraceFilter filter;
    SDK::Trace(start, end, mask, &filter, &trace);

    lua_createtable(L, 0, 3);
    lua_pushnumber(L, trace.fraction);
    lua_setfield(L, -2, "fraction");
    lua_pushboolean(L, trace.DidHit());
    lua_setfield(L, -2, "hit");
    if (trace.m_pEnt) {
        PushEntity(L, trace.m_pEnt->As<CBaseEntity>());
        lua_setfield(L, -2, "entity");
    }
    return 1;
}

static int lua_engine_world_to_screen(lua_State *L) {
  if (lua_gettop(L) < 3 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)) return 0;
  Vec3 world;
  world.x = (float)lua_tonumber(L, 1);
  world.y = (float)lua_tonumber(L, 2);
  world.z = (float)lua_tonumber(L, 3);
  Vec3 screen;
  if (!SDK::W2S(world, screen)) {
    lua_pushboolean(L, false);
    return 1;
  }
  lua_pushnumber(L, screen.x);
  lua_pushnumber(L, screen.y);
  return 2;
}

// --- Input Table ---
static std::map<int, bool> s_mKeyWasDown;

static int lua_input_is_key_down(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isnumber(L, 1)) return 0;
  int key = (int)lua_tointeger(L, 1);
  lua_pushboolean(L, U::KeyHandler.Down(key));
  return 1;
}

static int lua_input_is_key_pressed(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isnumber(L, 1)) return 0;
  int key = (int)lua_tointeger(L, 1);
  bool bDown = U::KeyHandler.Down(key);
  bool bPressed = bDown && !s_mKeyWasDown[key];
  s_mKeyWasDown[key] = bDown;
  lua_pushboolean(L, bPressed);
  return 1;
}

static int lua_input_get_mouse_pos(lua_State *L) {
    POINT p;
    GetCursorPos(&p);
    auto hwnd = SDK::GetTeamFortressWindow();
    if (hwnd) {
        ScreenToClient(hwnd, &p);
    }
    lua_pushinteger(L, p.x);
    lua_pushinteger(L, p.y);
    return 2;
}

// --- math3d Table ---
static int lua_math3d_calc_angle(lua_State *L) {
  if (lua_gettop(L) < 6 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5) || !lua_isnumber(L, 6)) return 0;
  Vec3 from, to;
  from.x = (float)lua_tonumber(L, 1);
  from.y = (float)lua_tonumber(L, 2);
  from.z = (float)lua_tonumber(L, 3);
  to.x = (float)lua_tonumber(L, 4);
  to.y = (float)lua_tonumber(L, 5);
  to.z = (float)lua_tonumber(L, 6);
  Vec3 ang = Math::CalcAngle(from, to);
  lua_pushnumber(L, ang.x);
  lua_pushnumber(L, ang.y);
  lua_pushnumber(L, ang.z);
  return 3;
}

static int lua_math3d_calc_fov(lua_State *L) {
  if (lua_gettop(L) < 4 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5)) return 0;
  Vec3 va, ang;
  va.x = (float)lua_tonumber(L, 1);
  va.y = (float)lua_tonumber(L, 2);
  va.z = lua_isnumber(L, 3) ? (float)lua_tonumber(L, 3) : 0.0f;
  ang.x = (float)lua_tonumber(L, 4);
  ang.y = (float)lua_tonumber(L, 5);
  ang.z = lua_isnumber(L, 6) ? (float)lua_tonumber(L, 6) : 0.0f;
  lua_pushnumber(L, Math::CalcFov(va, ang));
  return 1;
}

static int lua_math3d_vector_distance(lua_State *L) {
  if (lua_gettop(L) < 6 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5) || !lua_isnumber(L, 6)) return 0;
  Vec3 a, b;
  a.x = (float)lua_tonumber(L, 1);
  a.y = (float)lua_tonumber(L, 2);
  a.z = (float)lua_tonumber(L, 3);
  b.x = (float)lua_tonumber(L, 4);
  b.y = (float)lua_tonumber(L, 5);
  b.z = (float)lua_tonumber(L, 6);
  lua_pushnumber(L, a.DistTo(b));
  return 1;
}

static int lua_math3d_normalize_angle(lua_State *L) {
  if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2)) return 0;
  Vec3 ang;
  ang.x = (float)lua_tonumber(L, 1);
  ang.y = (float)lua_tonumber(L, 2);
  ang.z = lua_isnumber(L, 3) ? (float)lua_tonumber(L, 3) : 0.0f;
  Math::ClampAngles(ang);
  lua_pushnumber(L, ang.x);
  lua_pushnumber(L, ang.y);
  lua_pushnumber(L, ang.z);
  return 3;
}

static int lua_math3d_angle_vectors(lua_State* L) {
    if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2)) return 0;
    Vec3 ang;
    ang.x = (float)lua_tonumber(L, 1);
    ang.y = (float)lua_tonumber(L, 2);
    ang.z = lua_isnumber(L, 3) ? (float)lua_tonumber(L, 3) : 0.0f;
    Vec3 forward, right, up;
    Math::AngleVectors(ang, &forward, &right, &up);
    
    auto push_vec = [&](Vec3 v) {
        lua_pushcfunction(L, lua_vector_new);
        lua_pushnumber(L, v.x); lua_pushnumber(L, v.y); lua_pushnumber(L, v.z);
        lua_call(L, 3, 1);
    };
    push_vec(forward); push_vec(right); push_vec(up);
    return 3;
}

static int lua_math3d_vector_angles(lua_State* L) {
    if (lua_gettop(L) < 3 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)) return 0;
    Vec3 forward((float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3));
    Vec3 angles = Math::CalcAngle({ 0,0,0 }, forward);
    lua_pushnumber(L, angles.x); lua_pushnumber(L, angles.y); lua_pushnumber(L, angles.z);
    return 3;
}

// --- Vars Table ---
static int lua_vars_get(lua_State *L) {
  if (lua_gettop(L) < 2 || !lua_isstring(L, 1) || !lua_isstring(L, 2)) return 0;
  const char *section = lua_tostring(L, 1);
  const char *name = lua_tostring(L, 2);
  for (auto v : G::Vars) {
    if (strcmp(v->Section(), section) == 0 && strcmp(v->Name(), name) == 0) {
      if (v->m_iType == typeid(int).hash_code())
        lua_pushinteger(L, v->As<int>()->Value);
      else if (v->m_iType == typeid(float).hash_code())
        lua_pushnumber(L, v->As<float>()->Value);
      else if (v->m_iType == typeid(bool).hash_code())
        lua_pushboolean(L, v->As<bool>()->Value);
      else if (v->m_iType == typeid(std::string).hash_code())
        lua_pushstring(L, v->As<std::string>()->Value.c_str());
      else
        return 0;
      return 1;
    }
  }
  return 0;
}

static int lua_vars_set(lua_State *L) {
  if (lua_gettop(L) < 3 || !lua_isstring(L, 1) || !lua_isstring(L, 2)) return 0;
  const char *section = lua_tostring(L, 1);
  const char *name = lua_tostring(L, 2);
  for (auto v : G::Vars) {
    if (strcmp(v->Section(), section) == 0 && strcmp(v->Name(), name) == 0) {
      if (v->m_iType == typeid(int).hash_code())
        v->As<int>()->Value = (int)lua_tointeger(L, 3);
      else if (v->m_iType == typeid(float).hash_code())
        v->As<float>()->Value = (float)lua_tonumber(L, 3);
      else if (v->m_iType == typeid(bool).hash_code())
        v->As<bool>()->Value = lua_toboolean(L, 3);
      else if (v->m_iType == typeid(std::string).hash_code())
        v->As<std::string>()->Value = lua_tostring(L, 3) ? lua_tostring(L, 3) : "";
      return 0;
    }
  }
  return 0;
}

// --- UI Table ---
static int lua_ui_begin(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
  const char *name = lua_tostring(L, 1);
  bool *pOpen = nullptr;
  bool open = true;
  if (lua_gettop(L) >= 2) {
    open = lua_toboolean(L, 2);
    pOpen = &open;
  }
  bool result = ImGui::Begin(name, pOpen);
  lua_pushboolean(L, result);
  if (pOpen)
    lua_pushboolean(L, open);
  else
    lua_pushnil(L);
  return 2;
}

static int lua_ui_end(lua_State *L) {
  ImGui::End();
  return 0;
}

static int lua_ui_set_next_window_collapsed(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isboolean(L, 1)) return 0;
    bool collapsed = lua_toboolean(L, 1);
    int cond = lua_isnumber(L, 2) ? (int)lua_tointeger(L, 2) : 0;
    ImGui::SetNextWindowCollapsed(collapsed, cond);
    return 0;
}

static int lua_ui_text(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
  const char *text = lua_tostring(L, 1);
  ImGui::TextUnformatted(text);
  return 0;
}

static int lua_ui_button(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
  const char *text = lua_tostring(L, 1);
  float w = lua_isnumber(L, 2) ? (float)lua_tonumber(L, 2) : 0.0f;
  float h = lua_isnumber(L, 3) ? (float)lua_tonumber(L, 3) : 0.0f;
  lua_pushboolean(L, ImGui::Button(text, {w, h}));
  return 1;
}

static int lua_ui_checkbox(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
  const char *text = lua_tostring(L, 1);
  bool active = false;
  if (lua_isboolean(L, 2))
    active = lua_toboolean(L, 2);
  else if (lua_isnumber(L, 2))
    active = lua_tonumber(L, 2) != 0;

  bool pressed = ImGui::Checkbox(text, &active);
  lua_pushboolean(L, pressed);
  lua_pushboolean(L, active);
  return 2;
}

static int lua_ui_slider(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
  const char *text = lua_tostring(L, 1);
  float val = 0.0f;
  if (lua_isnumber(L, 2))
    val = (float)lua_tonumber(L, 2);

  float min = lua_isnumber(L, 3) ? (float)lua_tonumber(L, 3) : 0.0f;
  float max = lua_isnumber(L, 4) ? (float)lua_tonumber(L, 4) : 100.0f;

  bool pressed = ImGui::SliderFloat(text, &val, min, max);
  lua_pushboolean(L, pressed);
  lua_pushnumber(L, val);
  return 2;
}

static int lua_ui_separator(lua_State *L) {
  ImGui::Separator();
  return 0;
}

static int lua_ui_same_line(lua_State *L) {
  float offset = lua_isnumber(L, 1) ? (float)lua_tonumber(L, 1) : 0.0f;
  float spacing = lua_isnumber(L, 2) ? (float)lua_tonumber(L, 2) : -1.0f;
  ImGui::SameLine(offset, spacing);
  return 0;
}

// --- Timer Table ---
static int lua_timer_simple(lua_State *L) {
  if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isfunction(L, 2)) return 0;
  float delay = (float)lua_tonumber(L, 1);
  lua_pushvalue(L, 2);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  
  CLua::Timer_t t;
  t.sId = "simple_" + std::to_string(SDK::PlatFloatTime());
  t.fDelay = delay;
  t.fEndTime = (float)SDK::PlatFloatTime() + delay;
  t.iReps = 1;
  t.iCallbackRef = ref;
  
  // Get owner script from lua_getinfo
  lua_Debug ar;
  if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "S", &ar)) {
      t.sOwnerScript = ar.source;
  }

  F::Lua.m_vTimers.push_back(t);
  return 0;
}

static int lua_timer_create(lua_State *L) {
  if (lua_gettop(L) < 4 || !lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isfunction(L, 4)) return 0;
  const char* id = lua_tostring(L, 1);
  float delay = (float)lua_tonumber(L, 2);
  int reps = (int)lua_tointeger(L, 3);
  
  // Remove existing timer with same ID
  for (auto it = F::Lua.m_vTimers.begin(); it != F::Lua.m_vTimers.end(); ++it) {
      if (it->sId == id) {
          luaL_unref(L, LUA_REGISTRYINDEX, it->iCallbackRef);
          F::Lua.m_vTimers.erase(it);
          break;
      }
  }

  lua_pushvalue(L, 4);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);

  CLua::Timer_t t;
  t.sId = id;
  t.fDelay = delay;
  t.fEndTime = (float)SDK::PlatFloatTime() + delay;
  t.iReps = reps;
  t.iCallbackRef = ref;
  
  lua_Debug ar;
  if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "S", &ar)) {
      t.sOwnerScript = ar.source;
  }

  F::Lua.m_vTimers.push_back(t);
  return 0;
}

static int lua_timer_remove(lua_State *L) {
  if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
  const char* id = lua_tostring(L, 1);
  for (auto it = F::Lua.m_vTimers.begin(); it != F::Lua.m_vTimers.end(); ++it) {
      if (it->sId == id) {
          luaL_unref(L, LUA_REGISTRYINDEX, it->iCallbackRef);
          F::Lua.m_vTimers.erase(it);
          break;
      }
  }
  return 0;
}

// --- Database Table ---
static int lua_database_save(lua_State *L) {
    if (lua_gettop(L) < 2 || !lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* key = lua_tostring(L, 1);
    const char* val = lua_tostring(L, 2);
    
    std::string path = fs::current_path().string() + "\\Nullware\\LuaData\\";
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        fs::create_directories(path, ec);
    }
    
    std::ofstream file(path + key + ".txt");
    if (file.is_open()) {
        file << val;
        file.close();
        lua_pushboolean(L, true);
        return 1;
    }
    lua_pushboolean(L, false);
    return 1;
}

static int lua_database_load(lua_State *L) {
    if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
    const char* key = lua_tostring(L, 1);
    std::string path = fs::current_path().string() + "\\Nullware\\LuaData\\" + key + ".txt";
    std::error_code ec;
    if (fs::exists(path, ec)) {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        lua_pushstring(L, buffer.str().c_str());
        return 1;
    }
    return 0;
}

static int lua_database_exists(lua_State *L) {
    if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
    const char* key = lua_tostring(L, 1);
    std::string path = fs::current_path().string() + "\\Nullware\\LuaData\\" + key + ".txt";
    std::error_code ec;
    lua_pushboolean(L, fs::exists(path, ec));
    return 1;
}

static int lua_database_delete(lua_State *L) {
    if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
    const char* key = lua_tostring(L, 1);
    std::string path = fs::current_path().string() + "\\Nullware\\LuaData\\" + key + ".txt";
    std::error_code ec;
    lua_pushboolean(L, fs::remove(path, ec));
    return 1;
}

// --- Callbacks Table ---
static int lua_callbacks_register(lua_State *L) {
  const char *callback = luaL_checkstring(L, 1);
  const char *id = luaL_checkstring(L, 2);
  if (lua_isfunction(L, 3)) {
    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if (F::Lua.m_mCallbacks[callback].count(id)) {
      luaL_unref(L, LUA_REGISTRYINDEX, F::Lua.m_mCallbacks[callback][id]);
    }
    F::Lua.m_mCallbacks[callback][id] = ref;
  }
  return 0;
}

static int lua_callbacks_unregister(lua_State *L) {
  const char *callback = luaL_checkstring(L, 1);
  const char *id = luaL_checkstring(L, 2);
  if (F::Lua.m_mCallbacks.count(callback) &&
      F::Lua.m_mCallbacks[callback].count(id)) {
    luaL_unref(L, LUA_REGISTRYINDEX, F::Lua.m_mCallbacks[callback][id]);
    F::Lua.m_mCallbacks[callback].erase(id);
  }
  return 0;
}

// --- CLua Implementation ---
CLua::CLua() {
  char appdata[MAX_PATH] = {};
  if (GetEnvironmentVariableA("LOCALAPPDATA", appdata, MAX_PATH)) {
    m_sScriptsPath = std::string(appdata) + "\\Nullware\\Scripts\\";
    m_sDataPath    = std::string(appdata) + "\\Nullware\\LuaData\\";
  } else {
    m_sScriptsPath = fs::current_path().string() + "\\Nullware\\Scripts\\";
    m_sDataPath    = fs::current_path().string() + "\\Nullware\\LuaData\\";
  }
  m_pState = luaL_newstate();
  if (m_pState) {
    luaL_openlibs(m_pState);
    RegisterFunctions();
    RefreshScripts();
  }
}

CLua::~CLua() {
  if (m_pState) {
    for (auto &cb : m_mCallbacks) {
      for (auto &pair : cb.second) {
        luaL_unref(m_pState, LUA_REGISTRYINDEX, pair.second);
      }
    }
    lua_close(m_pState);
  }
}

/* -----------------------------------------------------------------------
** Nullware.Client Module
** --------------------------------------------------------------------- */

static int lua_client_get_cvar(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    ConVar* pVar = I::CVar->FindVar(name);
    if (pVar) {
        lua_pushstring(L, pVar->GetString());
        return 1;
    }
    return 0;
}

static int lua_client_set_cvar(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);
    ConVar* pVar = I::CVar->FindVar(name);
    if (pVar) {
        pVar->SetValue(value);
        lua_pushboolean(L, true);
        return 1;
    }
    lua_pushboolean(L, false);
    return 1;
}

static int lua_client_chat_say(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    char buf[512];
    sprintf(buf, "say %s", msg);
    I::EngineClient->ClientCmd_Unrestricted(buf);
    return 0;
}

/* -----------------------------------------------------------------------
** Nullware.Party Module
** --------------------------------------------------------------------- */

static int lua_party_queue_up(lua_State* L) {
    int group = lua_isnumber(L, 1) ? (int)lua_tointeger(L, 1) : 7; // k_eTFMatchGroup_Casual_12v12
    I::TFPartyClient->RequestQueueForMatch(group);
    return 0;
}

static int lua_party_in_queue(lua_State* L) {
    int group = lua_isnumber(L, 1) ? (int)lua_tointeger(L, 1) : 7;
    lua_pushboolean(L, I::TFPartyClient->BInQueueForMatchGroup(group));
    return 1;
}

static int lua_party_send_chat(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    I::TFPartyClient->SendPartyChat(msg);
    return 0;
}

static const luaL_Reg party_lib[] = {
    {"QueueUp", lua_party_queue_up},
    {"InQueue", lua_party_in_queue},
    {"SendChat", lua_party_send_chat},
    {NULL, NULL}
};

/* -----------------------------------------------------------------------
** Nullware.GameCoordinator Module
** --------------------------------------------------------------------- */

static int lua_gc_in_lobby(lua_State* L) {
    lua_pushboolean(L, I::TFGCClientSystem->GetLobby() != nullptr);
    return 1;
}

static const luaL_Reg gc_lib[] = {
    {"InLobby", lua_gc_in_lobby},
    {NULL, NULL}
};

static int lua_client_get_latency(lua_State* L) {
    auto pNetChannel = I::EngineClient->GetNetChannelInfo();
    if (pNetChannel) {
        lua_pushnumber(L, pNetChannel->GetLatency(0)); // Latency in seconds
        return 1;
    }
    lua_pushnumber(L, 0.0);
    return 1;
}

static const luaL_Reg client_lib[] = {
    {"GetCVar", lua_client_get_cvar},
    {"SetCVar", lua_client_set_cvar},
    {"GetLatency", lua_client_get_latency},
    {"ChatSay", lua_client_chat_say},
    {NULL, NULL}
};

static const luaL_Reg timer_lib[] = {
    {"Simple", lua_timer_simple},
    {"Create", lua_timer_create},
    {"Remove", lua_timer_remove},
    {NULL, NULL}
};

static const luaL_Reg database_lib[] = {
    {"Save", lua_database_save},
    {"Load", lua_database_load},
    {"Exists", lua_database_exists},
    {"Delete", lua_database_delete},
    {NULL, NULL}
};

static int lua_aimbot_get_target(lua_State* L) {
    if (G::AimTarget.m_iEntIndex) {
        lua_pushinteger(L, G::AimTarget.m_iEntIndex);
        return 1;
    }
    return 0;
}

static const luaL_Reg aimbot_lib[] = {
    {"GetTarget", lua_aimbot_get_target},
    {NULL, NULL}
};

static int lua_entity_get_model_name(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) return 0;
    const model_t* pModel = pEnt->GetModel();
    if (pModel) {
        lua_pushstring(L, I::ModelInfoClient->GetModelName(pModel));
        return 1;
    }
    return 0;
}

static int lua_engine_get_player_info(lua_State* L) {
    int index = (int)luaL_checknumber(L, 1);
    player_info_t info;
    if (I::EngineClient->GetPlayerInfo(index, &info)) {
        lua_newtable(L);
        lua_pushstring(L, info.name);
        lua_setfield(L, -2, "name");
        lua_pushstring(L, info.guid);
        lua_setfield(L, -2, "guid");
        lua_pushnumber(L, info.friendsID);
        lua_setfield(L, -2, "friendsID");
        lua_pushboolean(L, info.fakeplayer);
        lua_setfield(L, -2, "bot");
        return 1;
    }
    return 0;
}

static int lua_engine_play_sound(lua_State* L) {
    const char* file = luaL_checkstring(L, 1);
    I::MatSystemSurface->PlaySound(file);
    return 0;
}

static int lua_engine_get_level_name(lua_State* L) {
    lua_pushstring(L, I::EngineClient->GetLevelName());
    return 1;
}


// ======================================================================
// EXTENDED ENTITY API
// ======================================================================

static int lua_entity_get_active_weapon(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt || !pEnt->IsPlayer()) return 0;
    auto pActiveEnt = pEnt->As<CBaseCombatCharacter>()->m_hActiveWeapon().Get();
    if (!pActiveEnt) { lua_pushnil(L); return 1; }
    auto pWpn = pActiveEnt->As<CTFWeaponBase>();
    if (!pWpn) { lua_pushnil(L); return 1; }
    return PushEntity(L, (CBaseEntity*)pWpn);
}

static int lua_entity_get_eye_position(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt || !pEnt->IsPlayer()) return 0;
    Vec3 vo = pEnt->As<CTFPlayer>()->GetViewOffset();
    Vec3 org = pEnt->m_vecOrigin();
    lua_pushcfunction(L, lua_vector_new);
    lua_pushnumber(L, org.x + vo.x);
    lua_pushnumber(L, org.y + vo.y);
    lua_pushnumber(L, org.z + vo.z);
    lua_call(L, 3, 1);
    return 1;
}

static int lua_entity_get_flags(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt || !pEnt->IsPlayer()) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, pEnt->As<CTFPlayer>()->m_fFlags());
    return 1;
}

static int lua_entity_get_movetype(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, (int)pEnt->m_MoveType());
    return 1;
}

static int lua_entity_get_simtime(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) { lua_pushnumber(L, 0); return 1; }
    lua_pushnumber(L, pEnt->m_flSimulationTime());
    return 1;
}

static int lua_entity_is_enemy(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    auto pLocal = H::Entities.GetLocal();
    if (!pEnt || !pLocal) { lua_pushboolean(L, false); return 1; }
    lua_pushboolean(L, pEnt->m_iTeamNum() != pLocal->m_iTeamNum());
    return 1;
}

static int lua_entity_is_visible(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    auto pLocal = H::Entities.GetLocal();
    if (!pEnt || !pLocal || !I::EngineClient->IsInGame()) {
        lua_pushboolean(L, false); return 1;
    }
    Vec3 vFrom = pLocal->m_vecOrigin() + pLocal->As<CTFPlayer>()->GetViewOffset();
    Vec3 vTo   = pEnt->m_vecOrigin();
    lua_pushboolean(L, SDK::VisPos(pLocal, pEnt, vFrom, vTo));
    return 1;
}

static int lua_entity_get_class_name(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) { lua_pushnil(L); return 1; }
    auto pCC = pEnt->GetClientClass();
    if (pCC) lua_pushstring(L, pCC->m_pNetworkName);
    else lua_pushnil(L);
    return 1;
}

static int lua_entity_get_dist_to_local(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    auto pLocal = H::Entities.GetLocal();
    if (!pEnt || !pLocal) { lua_pushnumber(L, 0); return 1; }
    lua_pushnumber(L, pLocal->m_vecOrigin().DistTo(pEnt->m_vecOrigin()));
    return 1;
}

// ======================================================================
// WEAPON API  (entity:GetActiveWeapon() returns this metatable too)
// ======================================================================

static int lua_weapon_get_clip1(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) { lua_pushinteger(L, -1); return 1; }
    lua_pushinteger(L, pEnt->As<CTFWeaponBase>()->m_iClip1());
    return 1;
}

static int lua_weapon_get_next_primary_attack(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) { lua_pushnumber(L, 0); return 1; }
    lua_pushnumber(L, pEnt->As<CTFWeaponBase>()->m_flNextPrimaryAttack());
    return 1;
}

static int lua_weapon_get_item_def_index(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) { lua_pushinteger(L, -1); return 1; }
    lua_pushinteger(L, pEnt->As<CTFWeaponBase>()->m_iItemDefinitionIndex());
    return 1;
}

static int lua_weapon_get_weapon_id(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, (int)pEnt->As<CTFWeaponBase>()->GetWeaponID());
    return 1;
}

static int lua_weapon_can_attack(lua_State* L) {
    auto pEnt = GetEntityFromLua(L, 1);
    if (!pEnt || !I::GlobalVars) { lua_pushboolean(L, false); return 1; }
    float nextAtk = pEnt->As<CTFWeaponBase>()->m_flNextPrimaryAttack();
    lua_pushboolean(L, I::GlobalVars->curtime >= nextAtk);
    return 1;
}

// ======================================================================
// ENTITIES TABLE EXTRAS
// ======================================================================

static int lua_entities_get_all(lua_State* L) {
    lua_newtable(L);
    if (!I::EngineClient || !I::EngineClient->IsInGame()) return 1;
    int i = 1;
    int maxEnt = I::ClientEntityList->GetHighestEntityIndex();
    for (int n = 1; n <= maxEnt; n++) {
        auto pClientEnt = I::ClientEntityList->GetClientEntity(n);
        if (!pClientEnt) continue;
        auto pEnt = pClientEnt->As<CBaseEntity>();
        PushEntity(L, pEnt);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int lua_entities_get_projectiles(lua_State* L) {
    lua_newtable(L);
    if (!I::EngineClient || !I::EngineClient->IsInGame()) return 1;
    int i = 1;
    int maxEnt = I::ClientEntityList->GetHighestEntityIndex();
    for (int n = I::EngineClient->GetMaxClients() + 1; n <= maxEnt; n++) {
        auto pClientEnt = I::ClientEntityList->GetClientEntity(n);
        if (!pClientEnt) continue;
        auto pEnt = pClientEnt->As<CBaseEntity>();
        auto pCC  = pEnt->GetClientClass();
        if (!pCC) continue;
        std::string name = pCC->m_pNetworkName;
        if (name.find("Rocket") != std::string::npos ||
            name.find("Grenade") != std::string::npos ||
            name.find("Flare") != std::string::npos ||
            name.find("Arrow") != std::string::npos ||
            name.find("Ball") != std::string::npos ||
            name.find("Pipe") != std::string::npos) {
            PushEntity(L, pEnt);
            lua_rawseti(L, -2, i++);
        }
    }
    return 1;
}

// ======================================================================
// DRAW EXTRAS
// ======================================================================

static int lua_draw_filled_rect_gradient(lua_State* L) {
    if (lua_gettop(L) < 6) return 0;
    float x = (float)lua_tonumber(L, 1), y = (float)lua_tonumber(L, 2);
    float w = (float)lua_tonumber(L, 3), h = (float)lua_tonumber(L, 4);
    ImU32 col_tl = GetImColor(L, 5), col_br = GetImColor(L, 6);
    bool horiz = lua_isboolean(L, 7) && lua_toboolean(L, 7);
    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(
        {x, y}, {x + w, y + h},
        horiz ? col_tl : col_tl,
        horiz ? col_br : col_tl,
        horiz ? col_br : col_br,
        horiz ? col_tl : col_br);
    return 0;
}

static int lua_draw_progress_bar(lua_State* L) {
    if (lua_gettop(L) < 5) return 0;
    float x = (float)lua_tonumber(L, 1), y = (float)lua_tonumber(L, 2);
    float w = (float)lua_tonumber(L, 3), h = (float)lua_tonumber(L, 4);
    float frac = std::clamp((float)lua_tonumber(L, 5), 0.f, 1.f);
    ImU32 bg   = GetImColor(L, 6);
    ImU32 fill = GetImColor(L, 7);
    auto* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled({x, y}, {x + w, y + h}, bg);
    dl->AddRectFilled({x, y}, {x + w * frac, y + h}, fill);
    return 0;
}

static int lua_draw_world_text(lua_State* L) {
    if (lua_gettop(L) < 4) return 0;
    float wx = (float)lua_tonumber(L, 1);
    float wy = (float)lua_tonumber(L, 2);
    float wz = (float)lua_tonumber(L, 3);
    const char* text = lua_tostring(L, 4);
    ImU32 color = GetImColor(L, 5);
    Vec3 screen;
    if (!SDK::W2S({wx, wy, wz}, screen)) return 0;
    ImVec2 sz = ImGui::CalcTextSize(text);
    ImGui::GetBackgroundDrawList()->AddText({screen.x - sz.x / 2.f, screen.y}, color, text);
    return 0;
}

// ======================================================================
// MATH3D EXTRAS
// ======================================================================

static int lua_math3d_lerp(lua_State* L) {
    if (lua_gettop(L) < 3) return 0;
    double a = lua_tonumber(L, 1), b = lua_tonumber(L, 2), t = lua_tonumber(L, 3);
    lua_pushnumber(L, a + (b - a) * t);
    return 1;
}

static int lua_math3d_clamp(lua_State* L) {
    if (lua_gettop(L) < 3) return 0;
    double v = lua_tonumber(L, 1), lo = lua_tonumber(L, 2), hi = lua_tonumber(L, 3);
    lua_pushnumber(L, std::clamp(v, lo, hi));
    return 1;
}

static int lua_math3d_vector_length(lua_State* L) {
    float x=0,y=0,z=0;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "x"); x=(float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 1, "y"); y=(float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 1, "z"); z=(float)lua_tonumber(L,-1); lua_pop(L,1);
    } else {
        x=(float)luaL_optnumber(L,1,0);
        y=(float)luaL_optnumber(L,2,0);
        z=(float)luaL_optnumber(L,3,0);
    }
    lua_pushnumber(L, sqrtf(x*x+y*y+z*z));
    return 1;
}

static int lua_math3d_dot_product(lua_State* L) {
    if (lua_gettop(L) < 6) return 0;
    float ax=(float)lua_tonumber(L,1), ay=(float)lua_tonumber(L,2), az=(float)lua_tonumber(L,3);
    float bx=(float)lua_tonumber(L,4), by=(float)lua_tonumber(L,5), bz=(float)lua_tonumber(L,6);
    lua_pushnumber(L, ax*bx + ay*by + az*bz);
    return 1;
}

static int lua_math3d_normalize_vector(lua_State* L) {
    float x=(float)luaL_optnumber(L,1,0), y=(float)luaL_optnumber(L,2,0), z=(float)luaL_optnumber(L,3,0);
    float len = sqrtf(x*x+y*y+z*z);
    if (len < 0.0001f) { lua_pushnumber(L,0); lua_pushnumber(L,0); lua_pushnumber(L,0); return 3; }
    lua_pushnumber(L, x/len); lua_pushnumber(L, y/len); lua_pushnumber(L, z/len);
    return 3;
}

// ======================================================================
// ENGINE EXTRAS
// ======================================================================

static int lua_engine_get_server_ip(lua_State* L) {
    if (!I::EngineClient) { lua_pushstring(L, ""); return 1; }
    auto pNet = I::EngineClient->GetNetChannelInfo();
    if (pNet) lua_pushstring(L, pNet->GetAddress());
    else lua_pushstring(L, "");
    return 1;
}

static int lua_engine_get_ping_ms(lua_State* L) {
    if (!I::EngineClient) { lua_pushnumber(L, 0); return 1; }
    auto pNet = I::EngineClient->GetNetChannelInfo();
    if (pNet) lua_pushnumber(L, pNet->GetLatency(0) * 1000.0);
    else lua_pushnumber(L, 0);
    return 1;
}

static int lua_engine_is_local_server(lua_State* L) {
    lua_pushboolean(L, SDK::IsLoopback());
    return 1;
}

static int lua_engine_get_frame_time(lua_State* L) {
    lua_pushnumber(L, I::GlobalVars ? I::GlobalVars->frametime : 0.015);
    return 1;
}

// ======================================================================
// HTTP TABLE (wininet async)
// ======================================================================
static int lua_http_get(lua_State* L) {
    if (lua_gettop(L) < 2 || !lua_isstring(L, 1) || !lua_isfunction(L, 2)) return 0;
    std::string url = lua_tostring(L, 1);
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    std::thread([url, ref]() {
        HINTERNET hInet = InternetOpenA("Nullware/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInet) {
            std::lock_guard<std::mutex> lock(F::Lua.m_HttpMutex);
            F::Lua.m_vHttpResults.push_back({ref, -1, ""});
            return;
        }
        HINTERNET hUrl = InternetOpenUrlA(hInet, url.c_str(), NULL, 0,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
        std::string body;
        int statusCode = -1;
        if (hUrl) {
            DWORD statusSize = sizeof(DWORD);
            DWORD dwStatus = 0;
            HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                &dwStatus, &statusSize, NULL);
            statusCode = (int)dwStatus;
            char buf[4096];
            DWORD read;
            while (InternetReadFile(hUrl, buf, sizeof(buf)-1, &read) && read > 0) {
                buf[read] = 0;
                body += buf;
            }
            InternetCloseHandle(hUrl);
        }
        InternetCloseHandle(hInet);
        std::lock_guard<std::mutex> lock(F::Lua.m_HttpMutex);
        F::Lua.m_vHttpResults.push_back({ref, statusCode, body});
    }).detach();

    return 0;
}

// ======================================================================
// NOTIFICATION / OUTPUT
// ======================================================================
static int lua_notify(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    ImU32 col32 = lua_gettop(L) >= 2 ? GetImColor(L, 2) : IM_COL32(175,150,255,255);
    Color_t c((col32) & 0xFF, (col32 >> 8) & 0xFF, (col32 >> 16) & 0xFF, (col32 >> 24) & 0xFF);
    F::Notifications.Add(msg, c);
    return 0;
}

// ======================================================================
// DATABASE FIX — use %LOCALAPPDATA%\Nullware\LuaData\
// ======================================================================
static std::string LuaDataPath() {
    char path[MAX_PATH];
    if (GetEnvironmentVariableA("LOCALAPPDATA", path, MAX_PATH))
        return std::string(path) + "\\Nullware\\LuaData\\";
    return fs::current_path().string() + "\\Nullware\\LuaData\\";
}

static int lua_database_save2(lua_State* L) {
    if (lua_gettop(L) < 2 || !lua_isstring(L, 1)) { lua_pushboolean(L, false); return 1; }
    std::string path = LuaDataPath();
    std::error_code ec;
    if (!fs::exists(path, ec)) fs::create_directories(path, ec);
    std::ofstream f(path + lua_tostring(L, 1) + ".txt");
    if (!f.is_open()) { lua_pushboolean(L, false); return 1; }
    f << (lua_isstring(L, 2) ? lua_tostring(L, 2) : "");
    lua_pushboolean(L, true);
    return 1;
}

static int lua_database_load2(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) return 0;
    std::string p = LuaDataPath() + lua_tostring(L, 1) + ".txt";
    std::error_code ec;
    if (!fs::exists(p, ec)) { lua_pushnil(L); return 1; }
    std::ifstream f(p);
    std::stringstream buf; buf << f.rdbuf();
    lua_pushstring(L, buf.str().c_str());
    return 1;
}

static int lua_database_exists2(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) { lua_pushboolean(L, false); return 1; }
    std::error_code ec;
    lua_pushboolean(L, fs::exists(LuaDataPath() + lua_tostring(L, 1) + ".txt", ec));
    return 1;
}

static int lua_database_delete2(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isstring(L, 1)) { lua_pushboolean(L, false); return 1; }
    std::error_code ec;
    lua_pushboolean(L, fs::remove(LuaDataPath() + lua_tostring(L, 1) + ".txt", ec));
    return 1;
}

static int lua_database_list2(lua_State* L) {
    lua_newtable(L);
    std::string path = LuaDataPath();
    std::error_code ec;
    if (!fs::exists(path, ec)) return 1;
    int i = 1;
    for (auto& e : fs::directory_iterator(path, ec)) {
        if (e.path().extension() == ".txt") {
            std::string stem = e.path().stem().string();
            lua_pushstring(L, stem.c_str());
            lua_rawseti(L, -2, i++);
        }
    }
    return 1;
}

void CLua::RegisterFunctions() {
  // Vector Metatable
  luaL_newmetatable(m_pState, "Vector");
  lua_pushcfunction(m_pState, lua_vector_add);
  lua_setfield(m_pState, -2, "__add");
  lua_pushcfunction(m_pState, lua_vector_sub);
  lua_setfield(m_pState, -2, "__sub");
  lua_pushcfunction(m_pState, lua_vector_mul);
  lua_setfield(m_pState, -2, "__mul");
  lua_pushvalue(m_pState, -1);
  lua_setfield(m_pState, -2, "__index");
  lua_pop(m_pState, 1);

  // Entity Metatable
  luaL_newmetatable(m_pState, "Entity");
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_entity_get_index);
  lua_setfield(m_pState, -2, "GetIndex");
  lua_pushcfunction(m_pState, lua_entity_get_class_id);
  lua_setfield(m_pState, -2, "GetClassID");
  lua_pushcfunction(m_pState, lua_entity_get_team);
  lua_setfield(m_pState, -2, "GetTeam");
  lua_pushcfunction(m_pState, lua_entity_get_health);
  lua_setfield(m_pState, -2, "GetHealth");
  lua_pushcfunction(m_pState, lua_entity_get_max_health);
  lua_setfield(m_pState, -2, "GetMaxHealth");
  lua_pushcfunction(m_pState, lua_entity_get_origin);
  lua_setfield(m_pState, -2, "GetOrigin");
  lua_pushcfunction(m_pState, lua_entity_get_velocity);
  lua_setfield(m_pState, -2, "GetVelocity");
  lua_pushcfunction(m_pState, lua_entity_get_model_name);
  lua_setfield(m_pState, -2, "GetModelName");
  lua_pushcfunction(m_pState, lua_entity_get_view_offset);
  lua_setfield(m_pState, -2, "GetViewOffset");
  lua_pushcfunction(m_pState, lua_entity_get_class);
  lua_setfield(m_pState, -2, "GetClass");
  lua_pushcfunction(m_pState, lua_entity_is_player);
  lua_setfield(m_pState, -2, "IsPlayer");
  lua_pushcfunction(m_pState, lua_entity_is_building);
  lua_setfield(m_pState, -2, "IsBuilding");
  lua_pushcfunction(m_pState, lua_entity_is_alive);
  lua_setfield(m_pState, -2, "IsAlive");
  lua_pushcfunction(m_pState, lua_entity_is_dormant);
  lua_setfield(m_pState, -2, "IsDormant");
  lua_pushcfunction(m_pState, lua_entity_is_on_ground);
  lua_setfield(m_pState, -2, "IsOnGround");
  lua_pushcfunction(m_pState, lua_entity_in_cond);
  lua_setfield(m_pState, -2, "InCond");
  lua_pushcfunction(m_pState, lua_entity_get_name);
  lua_setfield(m_pState, -2, "GetName");
  lua_pushcfunction(m_pState, lua_entity_get_netvar);
  lua_setfield(m_pState, -2, "GetNetVar");
  lua_pushcfunction(m_pState, lua_entity_set_netvar);
  lua_setfield(m_pState, -2, "SetNetVar");
  lua_pushcfunction(m_pState, lua_entity_get_abs_origin);
  lua_setfield(m_pState, -2, "GetAbsOrigin");
  // Extended entity API
  lua_pushcfunction(m_pState, lua_entity_get_active_weapon);
  lua_setfield(m_pState, -2, "GetActiveWeapon");
  lua_pushcfunction(m_pState, lua_entity_get_eye_position);
  lua_setfield(m_pState, -2, "GetEyePosition");
  lua_pushcfunction(m_pState, lua_entity_get_flags);
  lua_setfield(m_pState, -2, "GetFlags");
  lua_pushcfunction(m_pState, lua_entity_get_movetype);
  lua_setfield(m_pState, -2, "GetMoveType");
  lua_pushcfunction(m_pState, lua_entity_get_simtime);
  lua_setfield(m_pState, -2, "GetSimulationTime");
  lua_pushcfunction(m_pState, lua_entity_is_enemy);
  lua_setfield(m_pState, -2, "IsEnemy");
  lua_pushcfunction(m_pState, lua_entity_is_visible);
  lua_setfield(m_pState, -2, "IsVisible");
  lua_pushcfunction(m_pState, lua_entity_get_class_name);
  lua_setfield(m_pState, -2, "GetClassName");
  lua_pushcfunction(m_pState, lua_entity_get_dist_to_local);
  lua_setfield(m_pState, -2, "GetDistToLocal");
  // Weapon API (accessible on any entity)
  lua_pushcfunction(m_pState, lua_weapon_get_clip1);
  lua_setfield(m_pState, -2, "GetClip1");
  lua_pushcfunction(m_pState, lua_weapon_get_next_primary_attack);
  lua_setfield(m_pState, -2, "GetNextPrimaryAttack");
  lua_pushcfunction(m_pState, lua_weapon_get_item_def_index);
  lua_setfield(m_pState, -2, "GetItemDefIndex");
  lua_pushcfunction(m_pState, lua_weapon_get_weapon_id);
  lua_setfield(m_pState, -2, "GetWeaponID");
  lua_pushcfunction(m_pState, lua_weapon_can_attack);
  lua_setfield(m_pState, -2, "CanAttack");
  lua_setfield(m_pState, -2, "__index");
  lua_pop(m_pState, 1);

  // Globals
  lua_pushcfunction(m_pState, lua_vector_new);
  lua_setglobal(m_pState, "Vector");
  lua_pushcfunction(m_pState, lua_print);
  lua_setglobal(m_pState, "print");
  lua_pushcfunction(m_pState, lua_error_handler);
  lua_setglobal(m_pState, "error");

  // Engine
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_engine_get_local_player);
  lua_setfield(m_pState, -2, "GetLocalPlayer");
  lua_pushcfunction(m_pState, lua_engine_is_in_game);
  lua_setfield(m_pState, -2, "IsInGame");
  lua_pushcfunction(m_pState, lua_engine_is_connected);
  lua_setfield(m_pState, -2, "IsConnected");
  lua_pushcfunction(m_pState, lua_engine_execute_cmd);
  lua_setfield(m_pState, -2, "ExecuteClientCmd");
  lua_pushcfunction(m_pState, lua_engine_get_screen_size);
  lua_setfield(m_pState, -2, "GetScreenSize");
  lua_pushcfunction(m_pState, lua_engine_get_view_angles);
  lua_setfield(m_pState, -2, "GetViewAngles");
  lua_pushcfunction(m_pState, lua_engine_set_view_angles);
  lua_setfield(m_pState, -2, "SetViewAngles");
  lua_pushcfunction(m_pState, lua_engine_get_tickcount);
  lua_setfield(m_pState, -2, "GetTickCount");
  lua_pushcfunction(m_pState, lua_engine_get_curtime);
  lua_setfield(m_pState, -2, "GetCurtime");
  lua_pushcfunction(m_pState, lua_engine_get_max_clients);
  lua_setfield(m_pState, -2, "GetMaxClients");
  lua_pushcfunction(m_pState, lua_engine_world_to_screen);
  lua_setfield(m_pState, -2, "WorldToScreen");
  lua_pushcfunction(m_pState, lua_engine_get_player_info);
  lua_setfield(m_pState, -2, "GetPlayerInfo");
  lua_pushcfunction(m_pState, lua_engine_trace_line);
  lua_setfield(m_pState, -2, "TraceLine");
  lua_pushcfunction(m_pState, lua_engine_play_sound);
  lua_setfield(m_pState, -2, "PlaySound");
  lua_pushcfunction(m_pState, lua_engine_get_level_name);
  lua_setfield(m_pState, -2, "GetLevelName");
  lua_pushcfunction(m_pState, lua_engine_get_plat_float_time);
  lua_setfield(m_pState, -2, "GetPlatFloatTime");
  lua_pushcfunction(m_pState, lua_engine_con_command);
  lua_setfield(m_pState, -2, "ConCommand");
  lua_pushcfunction(m_pState, lua_engine_get_server_ip);
  lua_setfield(m_pState, -2, "GetServerIP");
  lua_pushcfunction(m_pState, lua_engine_get_ping_ms);
  lua_setfield(m_pState, -2, "GetPingMS");
  lua_pushcfunction(m_pState, lua_engine_is_local_server);
  lua_setfield(m_pState, -2, "IsLocalServer");
  lua_pushcfunction(m_pState, lua_engine_get_frame_time);
  lua_setfield(m_pState, -2, "GetFrameTime");
  lua_pushcfunction(m_pState, [](lua_State* L) -> int {
      const char* title = luaL_checkstring(L, 1);
      const char* msg = luaL_checkstring(L, 2);
      SDK::Output(title, msg, { 255, 255, 255, 255 }, OUTPUT_TOAST);
      return 0;
  });
  lua_setfield(m_pState, -2, "Notification");
  lua_setglobal(m_pState, "engine");

  // UI
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_ui_begin);
  lua_setfield(m_pState, -2, "Begin");
  lua_pushcfunction(m_pState, lua_ui_end);
  lua_setfield(m_pState, -2, "End");
  lua_pushcfunction(m_pState, lua_ui_text);
  lua_setfield(m_pState, -2, "Text");
  lua_pushcfunction(m_pState, lua_ui_button);
  lua_setfield(m_pState, -2, "Button");
  lua_pushcfunction(m_pState, lua_ui_checkbox);
  lua_setfield(m_pState, -2, "Checkbox");
  lua_pushcfunction(m_pState, lua_ui_slider);
  lua_setfield(m_pState, -2, "Slider");
  lua_pushcfunction(m_pState, lua_ui_separator);
  lua_setfield(m_pState, -2, "Separator");
  lua_pushcfunction(m_pState, lua_ui_same_line);
  lua_setfield(m_pState, -2, "SameLine");
  lua_pushcfunction(m_pState, lua_ui_set_next_window_collapsed);
  lua_setfield(m_pState, -2, "SetNextWindowCollapsed");
  lua_setglobal(m_pState, "ui");

  // Entities
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_entities_get_local);
  lua_setfield(m_pState, -2, "GetLocalPlayer");
  lua_pushcfunction(m_pState, lua_entities_get_by_index);
  lua_setfield(m_pState, -2, "GetByIndex");
  lua_pushcfunction(m_pState, lua_entities_get_players);
  lua_setfield(m_pState, -2, "GetPlayers");
  lua_pushcfunction(m_pState, lua_entities_get_enemy_players);
  lua_setfield(m_pState, -2, "GetEnemyPlayers");
  lua_pushcfunction(m_pState, lua_entities_get_buildings);
  lua_setfield(m_pState, -2, "GetBuildings");
  lua_pushcfunction(m_pState, lua_entities_get_all);
  lua_setfield(m_pState, -2, "GetAll");
  lua_pushcfunction(m_pState, lua_entities_get_projectiles);
  lua_setfield(m_pState, -2, "GetProjectiles");
  lua_setglobal(m_pState, "entities");

  // Draw
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_draw_line);
  lua_setfield(m_pState, -2, "Line");
  lua_pushcfunction(m_pState, lua_draw_rect);
  lua_setfield(m_pState, -2, "Rect");
  lua_pushcfunction(m_pState, lua_draw_text);
  lua_setfield(m_pState, -2, "Text");
  lua_pushcfunction(m_pState, lua_draw_circle);
  lua_setfield(m_pState, -2, "Circle");
  lua_pushcfunction(m_pState, lua_draw_triangle);
  lua_setfield(m_pState, -2, "Triangle");
  lua_pushcfunction(m_pState, lua_draw_get_text_size);
  lua_setfield(m_pState, -2, "GetTextSize");
  lua_pushcfunction(m_pState, lua_draw_filled_rect_gradient);
  lua_setfield(m_pState, -2, "RectGradient");
  lua_pushcfunction(m_pState, lua_draw_progress_bar);
  lua_setfield(m_pState, -2, "ProgressBar");
  lua_pushcfunction(m_pState, lua_draw_world_text);
  lua_setfield(m_pState, -2, "WorldText");
  lua_setglobal(m_pState, "draw");

  // Input
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_input_is_key_down);
  lua_setfield(m_pState, -2, "IsKeyDown");
  lua_pushcfunction(m_pState, lua_input_is_key_pressed);
  lua_setfield(m_pState, -2, "IsKeyPressed");
  lua_pushcfunction(m_pState, lua_input_get_mouse_pos);
  lua_setfield(m_pState, -2, "GetMousePos");
  lua_setglobal(m_pState, "input");

  // math3d
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_math3d_calc_angle);
  lua_setfield(m_pState, -2, "CalcAngle");
  lua_pushcfunction(m_pState, lua_math3d_calc_fov);
  lua_setfield(m_pState, -2, "CalcFov");
  lua_pushcfunction(m_pState, lua_math3d_vector_distance);
  lua_setfield(m_pState, -2, "VectorDistance");
  lua_pushcfunction(m_pState, lua_math3d_normalize_angle);
  lua_setfield(m_pState, -2, "NormalizeAngle");
  lua_pushcfunction(m_pState, lua_math3d_angle_vectors);
  lua_setfield(m_pState, -2, "AngleVectors");
  lua_pushcfunction(m_pState, lua_math3d_vector_angles);
  lua_setfield(m_pState, -2, "VectorAngles");
  lua_pushcfunction(m_pState, lua_math3d_lerp);
  lua_setfield(m_pState, -2, "Lerp");
  lua_pushcfunction(m_pState, lua_math3d_clamp);
  lua_setfield(m_pState, -2, "Clamp");
  lua_pushcfunction(m_pState, lua_math3d_vector_length);
  lua_setfield(m_pState, -2, "VectorLength");
  lua_pushcfunction(m_pState, lua_math3d_dot_product);
  lua_setfield(m_pState, -2, "DotProduct");
  lua_pushcfunction(m_pState, lua_math3d_normalize_vector);
  lua_setfield(m_pState, -2, "NormalizeVector");
  lua_setglobal(m_pState, "math3d");

  // Vars
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_vars_get);
  lua_setfield(m_pState, -2, "Get");
  lua_pushcfunction(m_pState, lua_vars_set);
  lua_setfield(m_pState, -2, "Set");
  lua_setglobal(m_pState, "vars");

  // Callbacks
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_callbacks_register);
  lua_setfield(m_pState, -2, "Register");
  lua_pushcfunction(m_pState, lua_callbacks_unregister);
  lua_setfield(m_pState, -2, "Unregister");
  lua_setglobal(m_pState, "callbacks");

  // --- Nullware Namespace (Modern API) ---
  lua_newtable(m_pState); // Nullware table
  
  // Create sub-tables and move globals into them
  auto MoveGlobal = [&](const char* name) {
      lua_getglobal(m_pState, name);
      lua_setfield(m_pState, -2, name);
  };

  MoveGlobal("engine");
  MoveGlobal("draw");
  MoveGlobal("ui");
  MoveGlobal("entities");
  MoveGlobal("input");
  MoveGlobal("math3d");
  MoveGlobal("vars");
  MoveGlobal("callbacks");

  // Add new modules to Nullware
  luaL_newlib(m_pState, timer_lib);
  lua_setfield(m_pState, -2, "Timer");

  // Updated Database (uses %LOCALAPPDATA%)
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_database_save2);   lua_setfield(m_pState, -2, "Save");
  lua_pushcfunction(m_pState, lua_database_load2);   lua_setfield(m_pState, -2, "Load");
  lua_pushcfunction(m_pState, lua_database_exists2); lua_setfield(m_pState, -2, "Exists");
  lua_pushcfunction(m_pState, lua_database_delete2); lua_setfield(m_pState, -2, "Delete");
  lua_pushcfunction(m_pState, lua_database_list2);   lua_setfield(m_pState, -2, "List");
  lua_setfield(m_pState, -2, "Database");

  // HTTP module
  lua_newtable(m_pState);
  lua_pushcfunction(m_pState, lua_http_get); lua_setfield(m_pState, -2, "Get");
  lua_setfield(m_pState, -2, "Http");

  // Notify helper
  lua_pushcfunction(m_pState, lua_notify);
  lua_setfield(m_pState, -2, "Notify");

  luaL_newlib(m_pState, client_lib);
  lua_setfield(m_pState, -2, "Client");

  luaL_newlib(m_pState, aimbot_lib);
  lua_setfield(m_pState, -2, "Aimbot");

  luaL_newlib(m_pState, party_lib);
  lua_setfield(m_pState, -2, "Party");

  luaL_newlib(m_pState, gc_lib);
  lua_setfield(m_pState, -2, "GC");

  lua_setglobal(m_pState, "Nullware");

  // --- Constants ---
  lua_getglobal(m_pState, "Nullware");
  lua_newtable(m_pState); // Constants
  
  auto AddButton = [&](const char* n, int v) { lua_pushinteger(m_pState, v); lua_setfield(m_pState, -2, n); };
  lua_newtable(m_pState); // Buttons
  AddButton("IN_ATTACK", 1); AddButton("IN_JUMP", 2); AddButton("IN_DUCK", 4); AddButton("IN_FORWARD", 8); AddButton("IN_BACK", 16);
  AddButton("IN_USE", 32); AddButton("IN_CANCEL", 64); AddButton("IN_LEFT", 128); AddButton("IN_RIGHT", 256); AddButton("IN_MOVELEFT", 512);
  AddButton("IN_MOVERIGHT", 1024); AddButton("IN_ATTACK2", 2048); AddButton("IN_RUN", 4096); AddButton("IN_RELOAD", 8192); AddButton("IN_ALT1", 16384);
  AddButton("IN_ALT2", 32768); AddButton("IN_SCORE", 65536); AddButton("IN_SPEED", 131072); AddButton("IN_WALK", 262144); AddButton("IN_ZOOM", 524288);
  AddButton("IN_WEAPON1", 1048576); AddButton("IN_WEAPON2", 2097152); AddButton("IN_BULLRUSH", 4194304); AddButton("IN_GRENADE1", 8388608);
  AddButton("IN_GRENADE2", 16777216); AddButton("IN_ATTACK3", 33554432);
  lua_setfield(m_pState, -2, "Buttons");

  lua_newtable(m_pState); // Conds
  lua_pushinteger(m_pState, 1); lua_setfield(m_pState, -2, "AIMING");
  lua_pushinteger(m_pState, 16); lua_setfield(m_pState, -2, "CLOAKED");
  lua_pushinteger(m_pState, 32); lua_setfield(m_pState, -2, "UBERCHARGED");
  lua_pushinteger(m_pState, 8192); lua_setfield(m_pState, -2, "TAUNTING");
  lua_setfield(m_pState, -2, "Conds");

  lua_setfield(m_pState, -2, "Constants");
  lua_pop(m_pState, 1);

  // Store error handler reference
  lua_pushcfunction(m_pState, lua_error_handler);
  m_iErrorHandlerRef = luaL_ref(m_pState, LUA_REGISTRYINDEX);
}

void CLua::RefreshScripts() {
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  m_vScripts.clear();
  if (!fs::is_directory(m_sScriptsPath))
    fs::create_directories(m_sScriptsPath);
  try {
    for (const auto &entry : fs::directory_iterator(m_sScriptsPath)) {
      if (fs::is_regular_file(entry.path()) && entry.path().extension() == ".lua") {
        std::string sName = entry.path().filename().string();
        m_vScripts.push_back(sName);
        if (m_mActiveScripts.find(sName) == m_mActiveScripts.end())
          m_mActiveScripts[sName] = false;
      }
    }
  } catch (...) {
  }
}

void CLua::OpenDirectory() {
  ShellExecuteA(NULL, "open", m_sScriptsPath.c_str(), NULL, NULL,
                SW_SHOWDEFAULT);
}

void CLua::LoadScript(std::string sName) {
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  if (sName.empty() || m_mActiveScripts[sName])
    return;
  std::ifstream scriptFile(m_sScriptsPath + "\\" + sName);
  if (!scriptFile.is_open())
    return;
  std::stringstream scriptBuffer;
  scriptBuffer << scriptFile.rdbuf();
  m_mActiveScripts[sName] = true;
  RunScript(sName, scriptBuffer.str());
}

void CLua::UnloadScript(std::string sName) {
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  if (sName.empty() || !m_mActiveScripts[sName])
    return;

  // Remove callbacks belonging to this script
  for (auto &cb : m_mCallbacks) {
    for (auto it = cb.second.begin(); it != cb.second.end(); ) {
      lua_rawgeti(m_pState, LUA_REGISTRYINDEX, it->second);
      lua_Debug ar;
      if (lua_getinfo(m_pState, ">S", &ar)) {
        if (std::string(ar.source) == sName || std::string(ar.short_src) == sName) {
          luaL_unref(m_pState, LUA_REGISTRYINDEX, it->second);
          it = cb.second.erase(it);
          continue;
        }
      }
      ++it;
    }
  }

  m_mActiveScripts[sName] = false;
  if (m_mScriptEnvs.count(sName)) {
    luaL_unref(m_pState, LUA_REGISTRYINDEX, m_mScriptEnvs[sName]);
    m_mScriptEnvs.erase(sName);
  }

  // Remove timers belonging to this script
  for (auto it = m_vTimers.begin(); it != m_vTimers.end(); ) {
      if (it->sOwnerScript == sName || it->sOwnerScript == "@" + sName) {
          luaL_unref(m_pState, LUA_REGISTRYINDEX, it->iCallbackRef);
          it = m_vTimers.erase(it);
      } else {
          ++it;
      }
  }
}

void CLua::RunScript(std::string sName, const std::string &sScript) {
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  if (!m_pState)
    return;

  // Load the chunk — pushes function onto stack
  if (luaL_loadbuffer(m_pState, sScript.c_str(), sScript.size(),
                      sName.c_str())) {
    SDK::Output("Lua Error", lua_tostring(m_pState, -1), {255, 50, 50, 255},
                OUTPUT_CONSOLE);
    lua_pop(m_pState, 1);
    UnloadScript(sName);
    return;
  }

  // Create per-script sandbox environment: env.__index = _G
  lua_newtable(m_pState);                // env
  lua_newtable(m_pState);                // mt
  lua_pushglobaltable(m_pState);         // _G
  lua_setfield(m_pState, -2, "__index"); // mt.__index = _G
  lua_setmetatable(m_pState, -2);        // setmetatable(env, mt)

  // Store reference to env for reading back callbacks later
  lua_pushvalue(m_pState, -1);                                  // dup env
  m_mScriptEnvs[sName] = luaL_ref(m_pState, LUA_REGISTRYINDEX); // pops dup

  // Set env as _ENV upvalue (upvalue #1) of the loaded chunk
  // Stack: [..., fn, env] — setupvalue pops env, fn stays
  lua_setupvalue(m_pState, -2, 1);

  // Execute
  if (lua_pcall(m_pState, 0, 0, 0)) {
    SDK::Output("Lua Error", lua_tostring(m_pState, -1), {255, 50, 50, 255},
                OUTPUT_CONSOLE);
    lua_pop(m_pState, 1);
    UnloadScript(sName);
    return;
  }

  // Auto-register callbacks defined at script top-level
  lua_rawgeti(m_pState, LUA_REGISTRYINDEX, m_mScriptEnvs[sName]);
  const char *hooks[] = {"OnRender", "CreateMove", "FrameStageNotify",
      "OnLevelInit", "OnLevelShutdown", "OnEntityCreated", "OnPlayerHurt",
      "OnPlayerDeath", "OnUnload"};
  for (const char *g : hooks) {
    lua_getfield(m_pState, -1, g);
    if (lua_isfunction(m_pState, -1))
      m_mCallbacks[g][sName] = luaL_ref(m_pState, LUA_REGISTRYINDEX);
    else
      lua_pop(m_pState, 1);
  }
  lua_pop(m_pState, 1);
}

void CLua::Update() {
    std::lock_guard<std::recursive_mutex> lock(m_Mut);
    if (!m_pState) return;

    // Dispatch HTTP results
    {
        std::lock_guard<std::mutex> hlock(m_HttpMutex);
        for (auto& r : m_vHttpResults) {
            lua_rawgeti(m_pState, LUA_REGISTRYINDEX, r.iCallbackRef);
            lua_pushinteger(m_pState, r.iStatusCode);
            lua_pushstring(m_pState, r.sBody.c_str());
            if (lua_pcall(m_pState, 2, 0, m_iErrorHandlerRef))
                lua_pop(m_pState, 1);
            luaL_unref(m_pState, LUA_REGISTRYINDEX, r.iCallbackRef);
        }
        m_vHttpResults.clear();
    }

    float curtime = (float)SDK::PlatFloatTime();
    for (auto it = m_vTimers.begin(); it != m_vTimers.end(); ) {
        if (curtime >= it->fEndTime) {
            lua_rawgeti(m_pState, LUA_REGISTRYINDEX, it->iCallbackRef);
            if (lua_pcall(m_pState, 0, 0, m_iErrorHandlerRef)) {
                lua_pop(m_pState, 1);
            }

            if (it->iReps > 0) {
                it->iReps--;
                if (it->iReps == 0) {
                    luaL_unref(m_pState, LUA_REGISTRYINDEX, it->iCallbackRef);
                    it = m_vTimers.erase(it);
                    continue;
                }
            }
            it->fEndTime = curtime + it->fDelay;
        }
        ++it;
    }
}

void CLua::OnRender() {
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  if (!m_pState)
    return;
  auto &cbs = m_mCallbacks["OnRender"];
  for (auto &pair : cbs) {
    lua_rawgeti(m_pState, LUA_REGISTRYINDEX, pair.second);
    if (lua_pcall(m_pState, 0, 0, m_iErrorHandlerRef)) {
      lua_pop(m_pState, 1);
    }
  }
}

void CLua::OnCreateMove(CUserCmd* pCmd) {
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  if (!m_pState || !pCmd)
    return;
  auto &cbs = m_mCallbacks["CreateMove"];

  lua_newtable(m_pState);
  int tableIdx = lua_gettop(m_pState);

  lua_pushnumber(m_pState, (double)pCmd->buttons);
  lua_setfield(m_pState, tableIdx, "buttons");
  lua_pushnumber(m_pState, (double)pCmd->viewangles.x);
  lua_setfield(m_pState, tableIdx, "viewangles_x");
  lua_pushnumber(m_pState, (double)pCmd->viewangles.y);
  lua_setfield(m_pState, tableIdx, "viewangles_y");
  lua_pushnumber(m_pState, (double)pCmd->forwardmove);
  lua_setfield(m_pState, tableIdx, "forwardmove");
  lua_pushnumber(m_pState, (double)pCmd->sidemove);
  lua_setfield(m_pState, tableIdx, "sidemove");
  lua_pushnumber(m_pState, (double)pCmd->upmove);
  lua_setfield(m_pState, tableIdx, "upmove");
  lua_pushnumber(m_pState, (double)pCmd->tick_count);
  lua_setfield(m_pState, tableIdx, "tick_count");
  lua_pushnumber(m_pState, (double)pCmd->command_number);
  lua_setfield(m_pState, tableIdx, "command_number");
  lua_pushnumber(m_pState, (double)pCmd->mousedx);
  lua_setfield(m_pState, tableIdx, "mousedx");
  lua_pushnumber(m_pState, (double)pCmd->mousedy);
  lua_setfield(m_pState, tableIdx, "mousedy");

  for (auto &pair : cbs) {
    lua_rawgeti(m_pState, LUA_REGISTRYINDEX, pair.second);
    lua_pushvalue(m_pState, tableIdx);
    if (lua_pcall(m_pState, 1, 0, m_iErrorHandlerRef)) {
      lua_pop(m_pState, 1);
    }
  }

  lua_getfield(m_pState, tableIdx, "buttons");
  pCmd->buttons = (int)lua_tonumber(m_pState, -1);
  lua_pop(m_pState, 1);

  lua_getfield(m_pState, tableIdx, "viewangles_x");
  pCmd->viewangles.x = (float)lua_tonumber(m_pState, -1);
  lua_pop(m_pState, 1);

  lua_getfield(m_pState, tableIdx, "viewangles_y");
  pCmd->viewangles.y = (float)lua_tonumber(m_pState, -1);
  lua_pop(m_pState, 1);

  lua_getfield(m_pState, tableIdx, "forwardmove");
  pCmd->forwardmove = (float)lua_tonumber(m_pState, -1);
  lua_pop(m_pState, 1);

  lua_getfield(m_pState, tableIdx, "sidemove");
  pCmd->sidemove = (float)lua_tonumber(m_pState, -1);
  lua_pop(m_pState, 1);

  lua_getfield(m_pState, tableIdx, "upmove");
  pCmd->upmove = (float)lua_tonumber(m_pState, -1);
  lua_pop(m_pState, 1);

  lua_pop(m_pState, 1); // Pop original table
}

void CLua::OnFrameStageNotify(ClientFrameStage_t curStage) {
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  if (!m_pState)
    return;
  auto &cbs = m_mCallbacks["FrameStageNotify"];
  for (auto &pair : cbs) {
    lua_rawgeti(m_pState, LUA_REGISTRYINDEX, pair.second);
    lua_pushinteger(m_pState, (int)curStage);
    if (lua_pcall(m_pState, 1, 0, m_iErrorHandlerRef)) {
      lua_pop(m_pState, 1);
    }
  }
}

void CLua::FireCallback(const char* name, int nArgs) {
  // nArgs values already pushed on stack before calling this
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  if (!m_pState) {
    if (nArgs > 0) lua_pop(m_pState, nArgs);
    return;
  }
  auto it = m_mCallbacks.find(name);
  if (it == m_mCallbacks.end()) {
    if (nArgs > 0 && m_pState) lua_pop(m_pState, nArgs);
    return;
  }
  // Save args into temps
  std::vector<int> argRefs;
  for (int i = 0; i < nArgs; i++) {
    lua_pushvalue(m_pState, -(nArgs - i));
    argRefs.push_back(luaL_ref(m_pState, LUA_REGISTRYINDEX));
  }
  if (nArgs > 0) lua_pop(m_pState, nArgs);

  for (auto& pair : it->second) {
    lua_rawgeti(m_pState, LUA_REGISTRYINDEX, pair.second);
    for (int ref : argRefs)
      lua_rawgeti(m_pState, LUA_REGISTRYINDEX, ref);
    if (lua_pcall(m_pState, nArgs, 0, m_iErrorHandlerRef))
      lua_pop(m_pState, 1);
  }
  for (int ref : argRefs)
    luaL_unref(m_pState, LUA_REGISTRYINDEX, ref);
}

void CLua::OnLevelInit() {
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  if (!m_pState) return;
  auto& cbs = m_mCallbacks["OnLevelInit"];
  for (auto& pair : cbs) {
    lua_rawgeti(m_pState, LUA_REGISTRYINDEX, pair.second);
    if (lua_pcall(m_pState, 0, 0, m_iErrorHandlerRef))
      lua_pop(m_pState, 1);
  }
}

void CLua::OnLevelShutdown() {
  std::lock_guard<std::recursive_mutex> lock(m_Mut);
  if (!m_pState) return;
  auto& cbs = m_mCallbacks["OnLevelShutdown"];
  for (auto& pair : cbs) {
    lua_rawgeti(m_pState, LUA_REGISTRYINDEX, pair.second);
    if (lua_pcall(m_pState, 0, 0, m_iErrorHandlerRef))
      lua_pop(m_pState, 1);
  }
}

#pragma once
#include "../../SDK/SDK.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

class CLua {
public:
	CLua();
	~CLua();

	void RegisterFunctions();
	void RefreshScripts();
	void OpenDirectory();
	void LoadScript(std::string sName);
	void UnloadScript(std::string sName);
	void RunScript(std::string sName, const std::string& sScript);

	void OnRender();
	void OnCreateMove(CUserCmd* pCmd);
	void OnFrameStageNotify(ClientFrameStage_t curStage);
	void OnLevelInit();
	void OnLevelShutdown();
	void FireCallback(const char* name, int nArgs = 0);

    struct Timer_t {
        std::string sId;
        float fEndTime;
        float fDelay;
        int iReps;
        int iCallbackRef;
        std::string sOwnerScript;
    };

	struct HttpResult_t {
		int iCallbackRef;
		int iStatusCode;
		std::string sBody;
	};

    void Update();

	lua_State* m_pState = nullptr;
    int m_iErrorHandlerRef = 0;
	std::string m_sScriptsPath;
	std::string m_sDataPath;
	std::vector<std::string> m_vScripts;
	std::map<std::string, bool> m_mActiveScripts;
	std::map<std::string, int> m_mScriptEnvs;
    std::map<std::string, std::map<std::string, int>> m_mCallbacks;
    std::vector<Timer_t> m_vTimers;
	std::vector<HttpResult_t> m_vHttpResults;
	std::mutex m_HttpMutex;

    std::recursive_mutex m_Mut;
};

ADD_FEATURE(CLua, Lua);

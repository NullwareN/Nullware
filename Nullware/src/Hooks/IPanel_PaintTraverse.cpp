#include "../SDK/SDK.h"

MAKE_HOOK(IPanel_PaintTraverse, U::Memory.GetVirtual(I::Panel, 41), void,
	void* rcx, VPANEL vguiPanel, bool forceRepaint, bool allowForce)
{
	DEBUG_RETURN(IPanel_PaintTraverse, rcx, vguiPanel, forceRepaint, allowForce);

	if (!Vars::Visuals::UI::StreamerMode.Value)
		return CALL_ORIGINAL(rcx, vguiPanel, forceRepaint, allowForce);

	if (!I::Panel)
		return CALL_ORIGINAL(rcx, vguiPanel, forceRepaint, allowForce);

	const char* szPanelName = I::Panel->GetName(vguiPanel);
	if (!szPanelName)
		return CALL_ORIGINAL(rcx, vguiPanel, forceRepaint, allowForce);

	switch (FNV1A::Hash32(szPanelName))
	{
	case FNV1A::Hash32Const("SteamFriendsList"):
	case FNV1A::Hash32Const("avatar"):
	case FNV1A::Hash32Const("RankPanel"):
	case FNV1A::Hash32Const("ModelContainer"):
	case FNV1A::Hash32Const("ServerLabelNew"):
		return;
	}

	CALL_ORIGINAL(rcx, vguiPanel, forceRepaint, allowForce);
}

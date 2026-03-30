#include "../SDK/SDK.h"

MAKE_HOOK(ISteamNetworkingUtils_GetPingToDataCenter, U::Memory.GetVirtual(I::SteamNetworkingUtils, 8), int,
	void* rcx, SteamNetworkingPOPID popID, SteamNetworkingPOPID* pViaRelayPoP)
{
	return CALL_ORIGINAL(rcx, popID, pViaRelayPoP);
}

MAKE_HOOK(CTFPartyClient_RequestQueueForMatch, S::CTFPartyClient_RequestQueueForMatch(), void,
	void* rcx, int eMatchGroup)
{
	CALL_ORIGINAL(rcx, eMatchGroup);
}
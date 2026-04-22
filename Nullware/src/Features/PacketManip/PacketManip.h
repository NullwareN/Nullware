#pragma once
#include "../../SDK/SDK.h"
#include "AntiAim/AntiAim.h"
#include "FakeLag/FakeLag.h"

class CPacketManip {
public:
  void Run(CTFPlayer *pLocal, CTFWeaponBase *pWeapon, CUserCmd *pCmd,
           bool *pSendPacket);
};

ADD_FEATURE(CPacketManip, PacketManip);
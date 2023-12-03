#include <game/server/entities/character.h>
#include <game/server/player.h>

#include "carry.h"

// Exchange this to a string that identifies your game mode.
// DM, TDM and CTF are reserved for teeworlds original modes.
// DDraceNetwork and TestDDraceNetwork are used by DDNet.
#define GAME_TYPE_NAME "carry"
#define TEST_TYPE_NAME "test-carry"

CGameControllerCarry::CGameControllerCarry(class CGameContext *pGameServer) :
	IGameController(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_TYPE_NAME : GAME_TYPE_NAME;

	//m_GameFlags = GAMEFLAG_TEAMS; // GAMEFLAG_TEAMS makes it a two-team gamemode
}

CGameControllerCarry::~CGameControllerCarry() = default;

void CGameControllerCarry::ColorBody(CPlayer *pPlayer, EColor Color)
{
	int ColorInt = 0;
	switch(Color)
	{
	case COLOR_BLACK:
		ColorInt = 0;
		break;
	case COLOR_GREEN:
		ColorInt = 5552404;
		break;
	case COLOR_RED:
		ColorInt = 16776960;
		break;
	}
	pPlayer->m_TeeInfos.m_ColorBody = ColorInt;
	pPlayer->m_TeeInfos.m_UseCustomColor = true;
}

void CGameControllerCarry::OnBotCharacterTick(CCharacter *pChr)
{
	if(pChr->m_FreezeTime)
		pChr->m_TouchedFreeze = true;

	// check if tee is cleanly unfrozen
	// no velocity and no freeze
	const int MaxVel = 6;
	if(pChr->Core()->m_Vel.y < MaxVel && pChr->Core()->m_Vel.y > -MaxVel && !pChr->m_FreezeTime && pChr->m_TouchedFreeze)
	{
		ColorBody(pChr->GetPlayer(), COLOR_GREEN);
		// dbg_msg("carry", "helped i=%d %.2f", pChr->GetPlayer()->GetCID(), pChr->Core()->m_Vel.y);
		if(!pChr->m_HelpedSince)
			pChr->m_HelpedSince = time_get();

		// tee has to be unfrozen at low velocity for 0.4 sec to count as helped
		// this magic value was discovered by try and error
		// using MaxVel 6 the time 0.5 is unreachable with a regular hammer. Would need hook.
		// 0.45 is possible but a bit long
		// 0.4 is possible in a 2 tile tunnel
		const int64_t UnforzenSince = time_get() - pChr->m_HelpedSince;
		const int64_t MinUnfreeze = 0.4 * time_freq();
		// dbg_msg("carry", "unfrozen since %ld diff to min %ld", UnforzenSince, UnforzenSince - MinUnfreeze);
		if(UnforzenSince > MinUnfreeze)
			pChr->Die(pChr->GetPlayer()->GetCID(), WEAPON_SELF);
	}
	else if(pChr->m_HelpedSince)
	{
		// dbg_msg("carry", "failed helped i=%d", pChr->GetPlayer()->GetCID());
		ColorBody(pChr->GetPlayer(), COLOR_BLACK);
		pChr->m_HelpedSince = 0;
	}
}

void CGameControllerCarry::OnCharacterTick(CCharacter *pChr)
{
	if(Server()->IsBot(pChr->GetPlayer()->GetCID()))
		OnBotCharacterTick(pChr);
}

void CGameControllerCarry::OnCharacterSpawn(class CCharacter *pChr)
{
	IGameController::OnCharacterSpawn(pChr);

	if(Server()->IsBot(pChr->GetPlayer()->GetCID()))
	{
		CNetObj_PlayerInput Input = {0};
		Server()->SetInput(pChr->GetPlayer()->GetCID(), &Input);
		ColorBody(pChr->GetPlayer(), COLOR_BLACK);
		pChr->m_TouchedFreeze = false;
		pChr->m_HelpedSince = 0;
	}
}

void CGameControllerCarry::Tick()
{
	IGameController::Tick();

	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(pPlayer->GetCharacter())
			OnCharacterTick(pPlayer->GetCharacter());
		// dbg_msg("color", "%s %d", Server()->ClientName(pPlayer->GetCID()), pPlayer->m_TeeInfos.m_ColorBody);
	}
}

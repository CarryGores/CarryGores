#include <game/mapitems.h>
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

int CGameControllerCarry::GetSpawnTarget()
{
	int Target = -1;
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(!pPlayer->GetCharacter())
			continue;

		// TODO: actually rotate players
		//       or even load balance who has the least tees around
		Target = pPlayer->GetCID();
		break;
	}
	return Target;
}

bool CGameControllerCarry::IsValidSpawnPos(vec2 Pos)
{
	int Game = GameServer()->Collision()->GetCollisionAt(Pos.x, Pos.y);
	int Front = GameServer()->Collision()->GetFCollisionAt(Pos.x, Pos.y);
	return (Game == TILE_AIR || Game == TILE_FREEZE) &&
	       (Front == TILE_AIR || Front == TILE_FREEZE);
}

vec2 CGameControllerCarry::GetFirstNonIntersectOrPushIntersect(std::vector<vec2> &ClosestFreeTile, vec2 Pos, int Start, int Max)
{
	for(int X = -Start; X < Max; X++)
	{
		for(int Y = -Start; Y < Max; Y++)
		{
			vec2 Check = vec2(Pos.x + X * 32, Pos.y + Y * 32);
			if(IsValidSpawnPos(Check))
			{
				ClosestFreeTile.emplace_back(Check);
				int _UnusedTeleNr;
				// vec2 WHERE;
				int Hit = GameServer()->Collision()->IntersectLineTeleHook(Pos, Check, 0x0, 0x0, &_UnusedTeleNr);
				// dbg_msg("carry", "hit=%d x=%d y=%d", Hit, (int)(Check.x / 32), (int)(Check.y / 32));
				// if(Hit)
				// 	dbg_msg("carry", "  at x=%d y=%d", (int)(WHERE.x / 32), (int)(WHERE.y / 32));
				if(!Hit)
				{
					dbg_msg("carry", "we do not intersect at %.2f %.2f", Check.x, Check.y);
					// ClosestFreeTileNonIntersect.emplace_back(Check);
					return Check;
				}
			}
		}
	}
	return vec2(-1, -1);
}

vec2 CGameControllerCarry::GetClosestFreeTile(vec2 Pos)
{
	std::vector<vec2> ClosestFreeTile;
	const int Radius = 20;

	vec2 Candidate = GetFirstNonIntersectOrPushIntersect(ClosestFreeTile, Pos, rand() % Radius, Radius);
	if(Candidate != vec2(-1, -1))
		return Candidate;
	Candidate = GetFirstNonIntersectOrPushIntersect(ClosestFreeTile, Pos, Radius, Radius);
	if(Candidate != vec2(-1, -1))
		return Candidate;

	dbg_msg("carry", "fallback to random intersect. Num options: %ld", ClosestFreeTile.size());
	return ClosestFreeTile.empty() ? vec2(-1, -1) : ClosestFreeTile[rand() % ClosestFreeTile.size()];
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

		int Target = GetSpawnTarget();
		if(Target != -1)
		{
			CPlayer *pTarget = GameServer()->m_apPlayers[Target];
			if(pTarget->GetCharacter())
			{
				vec2 TeeCenter = pTarget->GetCharacter()->Core()->m_Pos;
				TeeCenter.x = round_truncate(TeeCenter.x / 32) * 32 + 16;
				TeeCenter.y = round_truncate(TeeCenter.y / 32) * 32 + 16;
				vec2 SpawnPos = GetClosestFreeTile(TeeCenter);
				if(SpawnPos != vec2(-1, -1))
				{
					dbg_msg("carry", "spawn %.2f %.2f", SpawnPos.x, SpawnPos.y);
					pChr->Core()->m_Pos = SpawnPos;
				}
			}
		}
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

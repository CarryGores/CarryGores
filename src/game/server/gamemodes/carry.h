#ifndef GAME_SERVER_GAMEMODES_CARRY_H
#define GAME_SERVER_GAMEMODES_CARRY_H

#include <game/server/gamecontroller.h>

class CGameControllerCarry : public IGameController
{
public:
	CGameControllerCarry(class CGameContext *pGameServer);
	~CGameControllerCarry();

	void OnBotCharacterTick(class CCharacter *pChr);
	void OnCharacterTick(class CCharacter *pChr);

	enum EColor
	{
		COLOR_BLACK,
		COLOR_RED,
		COLOR_GREEN,
	};

	void ColorBody(class CPlayer *pPlayer, EColor Color);
	/*
		Function: GetSpawnTarget
			Picks a target player to spawn the dummy next to

		Returns:
			ClientID of target to spawn next to
			or -1 on error
	*/
	int GetSpawnTarget();
	/*
		Function: GetClosestFreeTile
			Picks a spawn position close to the given pos
			valid spawn positions are air and freeze

		Returns:
			vec2 with the valid spawn pos
			returns vec2(-1, -1) or error
	*/
	vec2 GetClosestFreeTile(vec2 Pos);
	vec2 GetFirstNonIntersectOrPushIntersect(std::vector<vec2> &ClosestFreeTile, vec2 Pos, int Start, int Max);
	bool IsValidSpawnPos(vec2 Pos);

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_CARRY_H

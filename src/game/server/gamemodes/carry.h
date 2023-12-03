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

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_CARRY_H

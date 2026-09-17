#ifndef __LAWNVERSUS_H__
#define __LAWNVERSUS_H__

#include "../GameConstants.h"

#ifdef _HAS_LOCAL_MULTIPLAYER

#include "../ConstEnums.h"

class Board;
class LawnApp;
class Zombie;
class GridItem;
namespace Sexy
{
	class Graphics;
};

// ====================================================================================================
// ▲ 本地对战（同机双人）
// ----------------------------------------------------------------------------------------------------
// 主机版的对战模式：一块草坪被切成两半，玩家一（鼠标）种植物，玩家二（手柄/键盘，复用合作模式
// 的 LawnPlayer 输入层）放僵尸。
//   · 植物一方占左边 VERSUS_PLANT_COLUMNS 列，照常靠阳光运作，向日葵仍是产出单位；
//   · 僵尸一方占右边的几列，货币是脑子。脑子按与阳光完全相同的节奏从天上掉在僵尸一侧，
//     墓碑（僵尸一方的产出单位，同时也是一堵墙）则按向日葵的节奏产出脑子；
//   · 僵尸只能放在已经立起来的墓碑上，并从墓碑中爬出来；
//   · 植物一方消灭僵尸一方最后一列上的三个靶子僵尸即获胜；
//   · 僵尸一方吃掉植物一方家门口的脑子即获胜。
// 玩家二的光标、卡槽与“阳光”（这里即脑子）仍然沿用合作模式中 Board::mPlayer2 的那一套互换机制，
// 因此 Board 里只认识“当前玩家”的旧代码无须改写。本模式同样不含任何网络代码。
// ====================================================================================================
class LawnVersus
{
public:
	LawnApp*			mApp;
	Board*				mBoard;
	int					mBrainCountDown;								// 天降脑子的倒计时，与 Board::mSunCountDown 对应
	int					mNumBrainsFallen;
	ZombieID			mTargetZombieID[VERSUS_TARGET_ZOMBIE_COUNT];	// 靶子僵尸
	float				mTargetZombieX[VERSUS_TARGET_ZOMBIE_COUNT];		// 靶子僵尸被钉住的横坐标
	int					mTargetZombiesLeft;
	bool				mTargetZombiesPlaced;							// 靶子僵尸是否已经布置好（布置之前不判定胜负）
	bool				mGameOver;

public:
	LawnVersus();

	void				Reset(Board* theBoard);
	void				StartLevel();
	void				Update();
	void				InitPlayer2SeedBank();
	PlantingReason		CanPlantAt(int theGridX, int theGridY, SeedType theSeedType);
	void				MouseDownWithZombie(int theX, int theY, int theClickCount);
	bool				EatHouseBrain(Zombie* theZombie);
	void				DrawDivider(Sexy::Graphics* g);

	static bool			IsVersusZombieSeed(SeedType theSeedType);
	static int			GetZombieSeedCost(SeedType theSeedType);
	static int			GetZombieSeedRefreshTime(SeedType theSeedType);
	static bool			IsZombieCardUnlocked(SeedType theSeedType, LawnApp* theApp);
	static ZombieType	SeedTypeToZombieType(SeedType theSeedType);
	/*inline*/ bool		IsZombieSideColumn(int theGridX);
	bool				IsUsableSquare(int theGridX, int theGridY);

private:
	void				PlaceHouseBrains();
	void				PlaceTargetZombies();
	void				UpdateBrainSpawning();
	void				UpdateGraveStoneBrains();
	void				UpdateTargetZombies();
	void				PlantPlayerWins();
	void				ZombiePlayerWins(Zombie* theZombie);
	GridItem*			GetHouseBrainAt(int theGridY);
	void				PlaceZombieOnGrave(SeedType theSeedType, int theGridX, int theGridY);
};

#endif

#endif

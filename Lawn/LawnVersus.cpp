#include "LawnVersus.h"

#ifdef _HAS_LOCAL_MULTIPLAYER

#include "Board.h"
#include "Zombie.h"
#include "Plant.h"
#include "Coin.h"
#include "GridItem.h"
#include "Challenge.h"
#include "SeedPacket.h"
#include "CursorObject.h"
#include "../LawnApp.h"
#include "../Resources.h"
#include "../Sexy.TodLib/TodFoley.h"
#include "../Sexy.TodLib/TodCommon.h"
#include "../SexyAppFramework/Graphics.h"

//0x000000（新增内容，原版无对应实现）
LawnVersus::LawnVersus()
{
	mApp = (LawnApp*)gSexyAppBase;
	mBoard = nullptr;
	mBrainCountDown = 0;
	mNumBrainsFallen = 0;
	mTargetZombiesLeft = 0;
	mTargetZombiesPlaced = false;
	mGameOver = false;
	for (int i = 0; i < VERSUS_TARGET_ZOMBIE_COUNT; i++)
	{
		mTargetZombieID[i] = ZombieID::ZOMBIEID_NULL;
		mTargetZombieX[i] = 0.0f;
	}
}

void LawnVersus::Reset(Board* theBoard)
{
	mBoard = theBoard;
	mBrainCountDown = 0;
	mNumBrainsFallen = 0;
	mTargetZombiesLeft = 0;
	mTargetZombiesPlaced = false;
	mGameOver = false;
	for (int i = 0; i < VERSUS_TARGET_ZOMBIE_COUNT; i++)
	{
		mTargetZombieID[i] = ZombieID::ZOMBIEID_NULL;
		mTargetZombieX[i] = 0.0f;
	}
}

bool LawnVersus::IsZombieSideColumn(int theGridX)
{
	return theGridX >= VERSUS_PLANT_COLUMNS && theGridX < MAX_GRID_SIZE_X;
}

//	该格子是否是一块能站人、能立墓碑的地面
bool LawnVersus::IsUsableSquare(int theGridX, int theGridY)
{
	if (theGridX < 0 || theGridX >= MAX_GRID_SIZE_X || theGridY < 0 || theGridY >= MAX_GRID_SIZE_Y)
		return false;

	GridSquareType aGridSquare = mBoard->mGridSquareType[theGridX][theGridY];
	return aGridSquare != GridSquareType::GRIDSQUARE_NONE && aGridSquare != GridSquareType::GRIDSQUARE_DIRT;
}

//	僵尸一方在整个游戏生涯中可能用到的全部卡牌。墓碑既是产出单位又是一堵墙，其余是进攻单位。
//	是否已经解锁（是否会被 InitPlayer2SeedBank 实际发到卡槽里）另见 IsZombieCardUnlocked，
//	这里只负责“这个 SeedType 属不属于对战僵尸卡”这一分类，供 CanPlantAt/Board 等处判定用，
//	与解锁进度无关（哪怕还没解锁，被误传进来也要能识别成僵尸卡而不是植物卡）。
bool LawnVersus::IsVersusZombieSeed(SeedType theSeedType)
{
	return
		theSeedType == SeedType::SEED_ZOMBIE_GRAVESTONE ||
		theSeedType == SeedType::SEED_ZOMBIE_NORMAL ||
		theSeedType == SeedType::SEED_ZOMBIE_TRAFFIC_CONE ||
		theSeedType == SeedType::SEED_ZOMBIE_SCREEN_DOOR ||
		theSeedType == SeedType::SEED_ZOMBIE_PAIL ||
		theSeedType == SeedType::SEED_ZOMBIE_FOOTBALL ||
		theSeedType == SeedType::SEED_ZOMBIE_IMP ||
		theSeedType == SeedType::SEED_ZOMBIE_LADDER ||
		theSeedType == SeedType::SEED_ZOMBIE_DIGGER ||
		theSeedType == SeedType::SEED_ZOMBIE_GARGANTUAR;
}

//	对战模式中的价格以脑子计，与“我是僵尸”中的阳光价格无关
int LawnVersus::GetZombieSeedCost(SeedType theSeedType)
{
	switch (theSeedType)
	{
	case SeedType::SEED_ZOMBIE_GRAVESTONE:		return 50;
	case SeedType::SEED_ZOMBIE_NORMAL:			return 25;
	case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:	return 75;
	case SeedType::SEED_ZOMBIE_SCREEN_DOOR:		return 100;
	case SeedType::SEED_ZOMBIE_PAIL:			return 125;
	case SeedType::SEED_ZOMBIE_FOOTBALL:		return 200;
	case SeedType::SEED_ZOMBIE_IMP:				return 50;
	case SeedType::SEED_ZOMBIE_LADDER:			return 150;
	case SeedType::SEED_ZOMBIE_DIGGER:			return 125;
	case SeedType::SEED_ZOMBIE_GARGANTUAR:		return 300;
	default:									return 9990;
	}
}

// ====================================================================================================
// ▲ 卡组解锁进度
// ----------------------------------------------------------------------------------------------------
// 主机版里“我是僵尸”的卡组会随冒险模式的推进逐步解锁；这里的存档（PlayerInfo）没有逐关记录哪些
// 僵尸已经解锁，只有 mPlayerInfo->mLevel（当前打到冒险模式第几关）和 mFinishedAdventure（是否通关，
// 0=没通关，1=普通难度通关一次，2=通关两次后的高难度）。LawnApp::CanShowStore/CanShowZenGarden 也是
// 用同样的 mLevel 阈值（25、45）来控制解锁，这里沿用同一套模式：
//   · 墓碑、普通僵尸：一开始就有（否则没法开局）
//   · 之后每解锁若干关卡，多给一种僵尸，直到把卡槽（SEEDBANK_MAX=10：1 张墓碑 + 9 种攻击僵尸）填满
//   · 撑杆僵尸初始化时会被直接扔进 PHASE_POLEVAULTER_PRE_VAULT（Lawn/Zombie.cpp 的
//     ZOMBIE_POLEVAULTER 分支），跳过“从墓碑里爬出来”能识别的 PHASE_ZOMBIE_NORMAL 状态；
//     实测挖地鼠僵尸在场上出生时也会被立刻扔进 PHASE_DIGGER_TUNNELING，同样绕开了正常出生阶段，
//     PlaceZombieOnGrave 里那条“非 PHASE_ZOMBIE_NORMAL 就跳过爬出动画、直接摆放”的退路本来就是
//     为这种情况准备的，两者会走相同的代码路径、都不会崩溃。所以撑杆僵尸并非“不兼容”到不能用，
//     只是没有爬出墓碑的动画（挖地鼠正好相反，直接出现更符合“挖出来”的主题，所以观感没问题）。
//     鉴于此，这里继续保留上一版用屏风门僵尸替换撑杆僵尸的选择（动画观感更好），
//     但把挖地鼠僵尸纳入解锁卡组，证明同一条退路确实是安全的。
bool LawnVersus::IsZombieCardUnlocked(SeedType theSeedType, LawnApp* theApp)
{
	if (theApp == nullptr || theApp->mPlayerInfo == nullptr)
	{
		return theSeedType == SeedType::SEED_ZOMBIE_GRAVESTONE || theSeedType == SeedType::SEED_ZOMBIE_NORMAL;
	}

	int aLevel = theApp->mPlayerInfo->mLevel;

	switch (theSeedType)
	{
	case SeedType::SEED_ZOMBIE_GRAVESTONE:		return true;
	case SeedType::SEED_ZOMBIE_NORMAL:			return true;
	case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:	return aLevel >= 5;
	case SeedType::SEED_ZOMBIE_SCREEN_DOOR:		return aLevel >= 10;
	case SeedType::SEED_ZOMBIE_PAIL:			return aLevel >= 15;
	case SeedType::SEED_ZOMBIE_IMP:				return aLevel >= 20;
	case SeedType::SEED_ZOMBIE_LADDER:			return aLevel >= 25;
	case SeedType::SEED_ZOMBIE_DIGGER:			return aLevel >= 30;
	case SeedType::SEED_ZOMBIE_FOOTBALL:		return aLevel >= 35;
	case SeedType::SEED_ZOMBIE_GARGANTUAR:		return theApp->HasFinishedAdventure();
	default:									return false;
	}
}

//	对战模式的僵尸卡牌沿用向日葵/豌豆射手式的冷却机制（“我是僵尸”里僵尸卡是不冷却的，脑子是唯一限制）。
//	冷却时间与造价成正比（约 15 ticks/脑子），再夹到与普通植物卡相同的“短/长/很长”区间
//	（AlmanacDialog 里的 750 / 3000 / 5000），这样便宜的路障/铁桶僵尸能像豌豆射手一样连续铺人，
//	昂贵的橄榄球僵尸则和坚果墙一样要攒好一阵子。
int LawnVersus::GetZombieSeedRefreshTime(SeedType theSeedType)
{
	const int VERSUS_REFRESH_MIN = 750;
	const int VERSUS_REFRESH_MAX = 5000;
	const int VERSUS_REFRESH_PER_BRAIN = 15;

	int aCost = GetZombieSeedCost(theSeedType);
	int aRefreshTime = aCost * VERSUS_REFRESH_PER_BRAIN;

	if (aRefreshTime < VERSUS_REFRESH_MIN)
		aRefreshTime = VERSUS_REFRESH_MIN;
	if (aRefreshTime > VERSUS_REFRESH_MAX)
		aRefreshTime = VERSUS_REFRESH_MAX;

	return aRefreshTime;
}

ZombieType LawnVersus::SeedTypeToZombieType(SeedType theSeedType)
{
	return Challenge::IZombieSeedTypeToZombieType(theSeedType);
}

// ====================================================================================================
// ▲ 关卡的布置
// ====================================================================================================
void LawnVersus::StartLevel()
{
	Reset(mBoard);

	mBoard->mEnableGraveStones = true;  // 墓碑需要能够正常升起
	mBoard->mSunMoney = VERSUS_STARTING_SUN;
	mBrainCountDown = RandRangeInt(VERSUS_BRAIN_COUNTDOWN, VERSUS_BRAIN_COUNTDOWN + VERSUS_BRAIN_COUNTDOWN_RANGE);

	PlaceHouseBrains();
	PlaceTargetZombies();
}

//	植物一方家门口的脑子：每行一颗，僵尸吃掉任意一颗即获胜。沿用“我是僵尸”中的格子物件。
void LawnVersus::PlaceHouseBrains()
{
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (!IsUsableSquare(0, aRow))
			continue;

		GridItem* aBrain = mBoard->mGridItems.DataArrayAlloc();
		aBrain->mGridItemType = GridItemType::GRIDITEM_IZOMBIE_BRAIN;
		aBrain->mGridX = 0;
		aBrain->mGridY = aRow;
		aBrain->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, aRow, 0);
		aBrain->mGridItemCounter = 70;
		aBrain->mPosX = mBoard->GridToPixelX(0, aRow) - 40;
		aBrain->mPosY = mBoard->GridToPixelY(0, aRow) + 40;
	}
}

//	靶子僵尸：站在僵尸一方最后一列上不动的三个僵尸，植物一方把它们全部消灭即获胜。
//	本资源包中没有专门的“靶子僵尸”美术，这里改用旗帜僵尸，它外形上足够醒目且无须新增任何资源。
void LawnVersus::PlaceTargetZombies()
{
	const int aRows[VERSUS_TARGET_ZOMBIE_COUNT] = { 0, 2, 4 };

	mTargetZombiesLeft = 0;
	for (int i = 0; i < VERSUS_TARGET_ZOMBIE_COUNT; i++)
	{
		int aRow = aRows[i];
		if (aRow >= MAX_GRID_SIZE_Y || !IsUsableSquare(VERSUS_TARGET_ZOMBIE_COLUMN, aRow))
			continue;

		// 波次用 0 而不是 ZOMBIE_WAVE_UI：后者会让 Zombie::IsOnBoard() 返回 false，
		// 那样植物就打不到它了
		Zombie* aZombie = mBoard->AddZombieInRow(ZombieType::ZOMBIE_FLAG, aRow, 0);
		if (aZombie == nullptr)
			continue;

		aZombie->mPosX = mBoard->GridToPixelX(VERSUS_TARGET_ZOMBIE_COLUMN, aRow) + 20.0f;
		aZombie->mPosY = aZombie->GetPosYBasedOnRow(aRow);
		aZombie->mX = (int)aZombie->mPosX;
		aZombie->mY = (int)aZombie->mPosY;
		aZombie->mVelX = 0.0f;
		aZombie->mBodyMaxHealth = VERSUS_TARGET_ZOMBIE_HEALTH;
		aZombie->mBodyHealth = VERSUS_TARGET_ZOMBIE_HEALTH;

		mTargetZombieID[mTargetZombiesLeft] = (ZombieID)mBoard->mZombies.DataArrayGetID(aZombie);
		mTargetZombieX[mTargetZombiesLeft] = aZombie->mPosX;
		mTargetZombiesLeft++;
	}

	mTargetZombiesPlaced = mTargetZombiesLeft > 0;
}

//	玩家二的卡槽：一张墓碑加最多九种按冒险模式进度解锁的进攻僵尸（见 IsZombieCardUnlocked）。
//	玩家二的卡槽与“阳光”（脑子）本身由合作模式中的 Board::mPlayer2 提供，这里只负责把卡牌
//	换成已解锁的僵尸并给出初始脑子数。
void LawnVersus::InitPlayer2SeedBank()
{
	// 完整卡池，按解锁顺序排列；墓碑永远排第一（供给僵尸一方的产出）
	const SeedType aAllZombieSeeds[] = {
		SeedType::SEED_ZOMBIE_GRAVESTONE,
		SeedType::SEED_ZOMBIE_NORMAL,
		SeedType::SEED_ZOMBIE_TRAFFIC_CONE,
		SeedType::SEED_ZOMBIE_SCREEN_DOOR,
		SeedType::SEED_ZOMBIE_PAIL,
		SeedType::SEED_ZOMBIE_IMP,
		SeedType::SEED_ZOMBIE_LADDER,
		SeedType::SEED_ZOMBIE_DIGGER,
		SeedType::SEED_ZOMBIE_FOOTBALL,
		SeedType::SEED_ZOMBIE_GARGANTUAR
	};

	SeedType aZombieSeeds[LENGTH(aAllZombieSeeds)];
	int aNumUnlocked = 0;
	for (int i = 0; i < (int)LENGTH(aAllZombieSeeds); i++)
	{
		if (IsZombieCardUnlocked(aAllZombieSeeds[i], mApp))
		{
			aZombieSeeds[aNumUnlocked++] = aAllZombieSeeds[i];
		}
	}

	SeedBank* aSeedBank = mBoard->mPlayer2.mSeedBank;
	if (aSeedBank == nullptr)
		return;

	// Board::InitPlayer2Cursor 在调用本函数之前，通过 SwapPlayerContext + UpdateWidth() 的把戏
	// 把玩家二这份卡槽的 mNumPackets 设成了 GetNumSeedsInBank()（对战模式固定是 6，那是历史上
	// 卡组还没有随进度扩展时的值）。这里按实际解锁数量重新赋值，让卡槽随进度一起变宽——
	// 这是玩家二私有的字段，不会影响玩家一（植物）那边的卡槽大小。
	aSeedBank->mNumPackets = min(aNumUnlocked, SEEDBANK_MAX);

	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		if (i < aSeedBank->mNumPackets)
		{
			aSeedBank->mSeedPackets[i].SetPacketType(aZombieSeeds[i]);
		}
		else
		{
			aSeedBank->mSeedPackets[i].mPacketType = SeedType::SEED_NONE;
		}
	}

	mBoard->mPlayer2.mSunMoney = VERSUS_STARTING_BRAINS;
}

// ====================================================================================================
// ▲ 种植（放置）的判定
// ====================================================================================================
PlantingReason LawnVersus::CanPlantAt(int theGridX, int theGridY, SeedType theSeedType)
{
	if (IsVersusZombieSeed(theSeedType))
	{
		// 僵尸只能放在僵尸一侧
		if (!IsZombieSideColumn(theGridX))
			return PlantingReason::PLANTING_NOT_HERE;

		if (!IsUsableSquare(theGridX, theGridY))
			return PlantingReason::PLANTING_NOT_HERE;

		GridItem* aGraveStone = mBoard->GetGraveStoneAt(theGridX, theGridY);
		if (theSeedType == SeedType::SEED_ZOMBIE_GRAVESTONE)
		{
			// 墓碑只能立在僵尸一侧的空格子上，且不能立在靶子僵尸所在的那一列
			if (aGraveStone || theGridX == VERSUS_TARGET_ZOMBIE_COLUMN)
				return PlantingReason::PLANTING_NOT_HERE;

			return mBoard->GetTopPlantAt(theGridX, theGridY, PlantPriority::TOPPLANT_ANY) ?
				PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
		}

		// 其余僵尸都是从墓碑里爬出来的，因此必须有一块已经完全升起的墓碑
		if (aGraveStone == nullptr || aGraveStone->mGridItemCounter < 100)
			return PlantingReason::PLANTING_ONLY_ON_GRAVES;

		return PlantingReason::PLANTING_OK;
	}

	// 植物只能种在植物一侧
	if (theGridX >= VERSUS_PLANT_COLUMNS)
		return PlantingReason::PLANTING_NOT_PASSED_LINE;

	return PlantingReason::PLANTING_OK;
}

//	把一个僵尸从墓碑中放出来
void LawnVersus::PlaceZombieOnGrave(SeedType theSeedType, int theGridX, int theGridY)
{
	if (theSeedType == SeedType::SEED_ZOMBIE_GRAVESTONE)
	{
		GridItem* aGraveStone = mBoard->AddAGraveStone(theGridX, theGridY);
		aGraveStone->mGridItemCounter = 0;
		aGraveStone->mSunCount = VERSUS_GRAVE_BRAIN_FIRST;  // 距离产出第一颗脑子的时间
		mApp->PlayFoley(FoleyType::FOLEY_THUMP);
		return;
	}

	Zombie* aZombie = mBoard->AddZombieInRow(SeedTypeToZombieType(theSeedType), theGridY, 0);
	if (aZombie == nullptr)
		return;

	// 撑杆僵尸等在初始化时就处于特殊阶段的僵尸不能走爬出墓碑的动画，
	// 本模式的卡组里没有这种僵尸，但这里仍然留一条退路
	if (aZombie->mZombiePhase == ZombiePhase::PHASE_ZOMBIE_NORMAL)
	{
		aZombie->RiseFromGrave(theGridX, theGridY);
	}
	else
	{
		aZombie->mPosX = mBoard->GridToPixelX(theGridX, theGridY) - 25.0f;
		aZombie->mX = (int)aZombie->mPosX;
	}
	mApp->PlaySample(Sexy::SOUND_GRAVESTONE_RUMBLE);
}

//	玩家二在草坪上按下时的处理，与 Challenge::IZombieMouseDownWithZombie 对应
void LawnVersus::MouseDownWithZombie(int theX, int theY, int theClickCount)
{
	if (theClickCount >= 0)
	{
		SeedType aSeedType = mBoard->mCursorObject->mType;
		int aGridX = mBoard->PlantingPixelToGridX(theX, theY, aSeedType);
		int aGridY = mBoard->PlantingPixelToGridY(theX, theY, aSeedType);
		if (aGridX != -1 && aGridY != -1 && theClickCount)
		{
			PlantingReason aReason = CanPlantAt(aGridX, aGridY, aSeedType);
			if (aReason == PlantingReason::PLANTING_OK)
			{
				if (mApp->mEasyPlantingCheat || mBoard->TakeSunMoney(mBoard->GetCurrentPlantCost(aSeedType, SeedType::SEED_NONE)))
				{
					PlaceZombieOnGrave(aSeedType, aGridX, aGridY);

					if (mBoard->mCursorObject->mSeedBankIndex >= 0 && mBoard->mCursorObject->mSeedBankIndex < mBoard->mSeedBank->mNumPackets)
					{
						mBoard->mSeedBank->mSeedPackets[mBoard->mCursorObject->mSeedBankIndex].WasPlanted();
					}
					mApp->PlayFoley(FoleyType::FOLEY_PLANT);
					mBoard->ClearCursor();
				}
			}
			else if (aReason == PlantingReason::PLANTING_ONLY_ON_GRAVES)
			{
				mBoard->DisplayAdvice(_S("[ADVICE_VERSUS_ONLY_ON_GRAVES]"), MessageStyle::MESSAGE_STYLE_HINT_LONG, AdviceType::ADVICE_PLANT_GRAVEBUSTERS_ON_GRAVES);
			}
			else
			{
				mBoard->DisplayAdvice(_S("[ADVICE_VERSUS_ZOMBIE_SIDE_ONLY]"), MessageStyle::MESSAGE_STYLE_HINT_LONG, AdviceType::ADVICE_I_ZOMBIE_NOT_PASSED_LINE);
			}

			return;
		}
	}

	mBoard->RefreshSeedPacketFromCursor();
	mApp->PlayFoley(FoleyType::FOLEY_DROP);
}

// ====================================================================================================
// ▲ 每帧的更新
// ====================================================================================================
void LawnVersus::Update()
{
	if (mGameOver)
		return;

	// 靶子僵尸在过场动画期间也会被 Board 更新，所以钉回原位这件事每帧都要做
	UpdateTargetZombies();

	if (mApp->mGameScene != GameScenes::SCENE_PLAYING || mBoard->mPaused)
		return;

	UpdateBrainSpawning();
	UpdateGraveStoneBrains();
}

//	天降脑子：节奏与 Board::UpdateSunSpawning 中的阳光完全一致，只是掉在僵尸一侧的几列上
void LawnVersus::UpdateBrainSpawning()
{
	if (mBoard->HasLevelAwardDropped())
		return;

	mBrainCountDown--;
	if (mBrainCountDown > 0)
		return;

	mNumBrainsFallen++;
	mBrainCountDown = min(VERSUS_BRAIN_COUNTDOWN_MAX, VERSUS_BRAIN_COUNTDOWN + mNumBrainsFallen * 10) + Rand(VERSUS_BRAIN_COUNTDOWN_RANGE);

	int aMinX = mBoard->GridToPixelX(VERSUS_PLANT_COLUMNS, 0);
	int aMaxX = mBoard->GridToPixelX(MAX_GRID_SIZE_X - 1, 0) + 40;
	mBoard->AddCoin(RandRangeInt(aMinX, aMaxX), 60, CoinType::COIN_BRAIN, CoinMotion::COIN_MOTION_FROM_SKY);
}

//	墓碑按向日葵的节奏产出脑子。每块墓碑的计时器借用 GridItem::mSunCount，
//	该字段在墓碑上原本没有任何用途。
void LawnVersus::UpdateGraveStoneBrains()
{
	if (mBoard->HasLevelAwardDropped())
		return;

	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType != GridItemType::GRIDITEM_GRAVESTONE)
			continue;

		// 尚未完全升起的墓碑还不能产出
		if (aGridItem->mGridItemCounter < 100)
			continue;

		if (aGridItem->mSunCount > 0)
		{
			aGridItem->mSunCount--;
			continue;
		}

		aGridItem->mSunCount = VERSUS_GRAVE_BRAIN_RATE;
		float aPosX = mBoard->GridToPixelX(aGridItem->mGridX, aGridItem->mGridY) + 20.0f;
		float aPosY = mBoard->GridToPixelY(aGridItem->mGridX, aGridItem->mGridY) + 20.0f;
		mBoard->AddCoin(aPosX, aPosY, CoinType::COIN_BRAIN, CoinMotion::COIN_MOTION_FROM_PLANT);
	}
}

//	靶子僵尸每帧都被钉回原位：它们不前进，只等着被植物一方打掉
void LawnVersus::UpdateTargetZombies()
{
	if (!mTargetZombiesPlaced)
		return;

	int aAliveCount = 0;
	for (int i = 0; i < VERSUS_TARGET_ZOMBIE_COUNT; i++)
	{
		if (mTargetZombieID[i] == ZombieID::ZOMBIEID_NULL)
			continue;

		Zombie* aZombie = mBoard->mZombies.DataArrayTryToGet((unsigned int)mTargetZombieID[i]);
		if (aZombie == nullptr || aZombie->mDead || aZombie->IsDeadOrDying())
		{
			mTargetZombieID[i] = ZombieID::ZOMBIEID_NULL;
			continue;
		}

		aZombie->mVelX = 0.0f;
		aZombie->mPosX = mTargetZombieX[i];
		aZombie->mX = (int)aZombie->mPosX;
		aAliveCount++;
	}

	mTargetZombiesLeft = aAliveCount;
	if (aAliveCount == 0 && mApp->mGameScene == GameScenes::SCENE_PLAYING)
	{
		PlantPlayerWins();
	}
}

// ====================================================================================================
// ▲ 胜负
// ====================================================================================================
GridItem* LawnVersus::GetHouseBrainAt(int theGridY)
{
	GridItem* aBrain = mBoard->GetGridItemAt(GridItemType::GRIDITEM_IZOMBIE_BRAIN, 0, theGridY);
	return (aBrain && aBrain->mGridItemState != GridItemState::GRIDITEM_STATE_BRAIN_SQUISHED) ? aBrain : nullptr;
}

//	僵尸啃掉植物一方家门口的脑子，与 Challenge::IZombieEatBrain 对应，但胜负判定不同：
//	对战模式中只要有一颗脑子被吃掉，僵尸一方就赢了
bool LawnVersus::EatHouseBrain(Zombie* theZombie)
{
	if (mGameOver || theZombie->IsWalkingBackwards())
		return false;

	Rect aZombieRect = theZombie->GetZombieAttackRect();
	if (aZombieRect.mX > 20)
		return false;

	GridItem* aBrain = GetHouseBrainAt(theZombie->mRow);
	if (aBrain == nullptr)
		return false;

	theZombie->StartEating();
	aBrain->mGridItemCounter--;
	if (aBrain->mGridItemCounter <= 0)
	{
		mApp->PlaySample(Sexy::SOUND_GULP);
		aBrain->GridItemDie();
		ZombiePlayerWins(theZombie);
	}
	return true;
}

void LawnVersus::PlantPlayerWins()
{
	if (mGameOver)
		return;

	mGameOver = true;
	// 明确点出“植物方胜利”：借用现成的长效提示条（DisplayAdvice/MESSAGE_STYLE_HINT_LONG，
	// 本文件其它地方判定不能种/不能放僵尸时也用它来提示玩家），不去碰 AwardScreen 那套
	// 复杂的过关流程本身——奖杯照常掉落、关卡照常按现有的“挑战关卡通关”流程结算
	mBoard->DisplayAdvice(_S("[VERSUS_PLANT_WIN_MESSAGE]"), MessageStyle::MESSAGE_STYLE_HINT_LONG, AdviceType::ADVICE_NONE);

	// 与其它挑战关卡一致：掉出奖杯，关卡即告通过
	mBoard->mChallenge->SpawnLevelAward(VERSUS_PLANT_COLUMNS - 1, 2);
}

void LawnVersus::ZombiePlayerWins(Zombie* theZombie)
{
	if (mGameOver)
		return;

	mGameOver = true;
	mBoard->ZombiesWon(theZombie);
}

// ====================================================================================================
// ▲ 绘制
// ====================================================================================================
//	两边的分界线。这里没有专门的分界美术，改用“我是僵尸”里同样用于示意分界的墓碑泥土贴图，
//	沿着分界画一列半透明的竖条。
void LawnVersus::DrawDivider(Graphics* g)
{
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING)
		return;

	int aLineX = mBoard->GridToPixelX(VERSUS_PLANT_COLUMNS, 0) - 4;
	g->SetColor(Color(0, 0, 0, 64));
	g->FillRect(aLineX, 0, 5, BOARD_HEIGHT);
}

#endif

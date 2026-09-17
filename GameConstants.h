#pragma once

#include <wtypes.h>
constexpr const double PI = 3.141592653589793;

// ============================================================
// ■ 常数
// ============================================================
const int			BOARD_WIDTH = 800;
const int			BOARD_HEIGHT = 600;
const int			WIDE_BOARD_WIDTH = 800;
const int			BOARD_OFFSET = 220;
const int			BOARD_EDGE = -100;
const int			BOARD_IMAGE_WIDTH_OFFSET = 1180;
const int           BOARD_ICE_START = 800;
const int           LAWN_XMIN = 40;
const int           LAWN_YMIN = 80;
const int           HIGH_GROUND_HEIGHT = 30;

const int           SEEDBANK_MAX = 10;
const int           SEED_BANK_OFFSET_X = 0;
const int           SEED_BANK_OFFSET_X_END = 10;
const int           SEED_CHOOSER_OFFSET_Y = 516;
const int           SEED_PACKET_WIDTH = 50;
const int           SEED_PACKET_HEIGHT = 70;
const int           IMITATER_DIALOG_WIDTH = 500;
const int           IMITATER_DIALOG_HEIGHT = 600;

// ============================================================
// ■ 本地双人（同机）相关
// ============================================================
const int           MAX_LAWN_PLAYERS = 2;
const int           VIRTUAL_CURSOR_SPEED = 9;			// 玩家二虚拟光标每帧移动的像素数
const int           GAMEPAD_STICK_DEADZONE = 8000;		// 摇杆死区，单位为 SDL 的轴量程
const int           GAMEPAD_RESCAN_INTERVAL = 100;		// 未检测到手柄时的重扫间隔（帧）
const int           LOCAL_PLAYER_2_START_X = 400;
const int           LOCAL_PLAYER_2_START_Y = 300;
const int           LOCAL_PLAYER_2_SEEDBANK_Y = 512;	// 玩家二的卡槽摆在屏幕底部

// 对战模式：植物一方占左边六列，僵尸一方占右边三列
const int           VERSUS_PLANT_COLUMNS = 6;
const int           VERSUS_TARGET_ZOMBIE_COLUMN = 8;	// 靶子僵尸所在的、僵尸一方的最后一列
const int           VERSUS_TARGET_ZOMBIE_COUNT = 3;		// 植物一方需要消灭的靶子僵尸数量
const int           VERSUS_TARGET_ZOMBIE_HEALTH = 1350;	// 与铁桶僵尸的整体血量相当
const int           VERSUS_STARTING_SUN = 50;
const int           VERSUS_STARTING_BRAINS = 50;
const int           VERSUS_BRAIN_VALUE = 25;			// 与一颗阳光等值
const int           VERSUS_BRAIN_COUNTDOWN = 425;		// 与 SUN_COUNTDOWN 相同，两边的天降产出节奏一致
const int           VERSUS_BRAIN_COUNTDOWN_RANGE = 275;
const int           VERSUS_BRAIN_COUNTDOWN_MAX = 950;
const int           VERSUS_GRAVE_BRAIN_RATE = 2500;		// 与向日葵的 mLaunchRate 相同
const int           VERSUS_GRAVE_BRAIN_FIRST = 900;		// 新墓碑产出第一颗脑子所需的时间

// 合作模式：主机版一共八个卡槽，两名玩家各四个，各自进一次选卡界面
const int           COOP_SEEDS_PER_PLAYER = 4;
// 合作模式的普通关只打两到三面旗（困难关与生存困难关完全相同，为十面旗）
const int           COOP_FLAGS_SHORT = 2;
const int           COOP_FLAGS_LONG = 3;
// 合作模式偶尔掉落的双人阳光：两名玩家的光标必须同时压在它上面才会被收走
const int           COOP_DOUBLE_SUN_CHANCE = 6;			// 每 N 颗天降阳光中约有一颗是双人阳光
const int           COOP_DOUBLE_SUN_VALUE = 50;			// 每名玩家各得这么多阳光
const int           COOP_DOUBLE_SUN_SCALE = 15;			// 贴图放大的百分比，用来与普通阳光区分
const int           COOP_DOUBLE_SUN_GRACE = 15;			// 两人各自压住它的时间差上限（帧）

const int			WIDESCREEN_OFFSETX = -240;
const int			WIDESCREEN_OFFSETY = -60;

// Aspect Ratio / Resolution modes, indexed into SexyAppBase::mResolutionMode
// (widthIndex + heightIndex * numWidths against the {800,1066,1280}/{600,720,800} tables).
const int			ASPECT_RATIO_STANDARD = 0;		// 800x600  (4:3, original)
const int			ASPECT_RATIO_WIDESCREEN = 1;		// 1066x600 (16:9)
const int			ASPECT_RATIO_WIDESCREEN_HD = 5;	// 1280x720 (16:9, taller board)

const int			STREET_ZOMBIE_START_X = 1030; // PC: 830 - Console : 1000
const int			STREET_ZOMBIE_ROOF_START_X = 900;
const int			STREET_ZOMBIE_START_Y = 70;
const int			STREET_ZOMBIE_GRID_SIZE_X = 30; // PC : 56 - Console : 30
const int			STREET_ZOMBIE_GRID_SIZE_Y = 90;
const int			STREET_ZOMBIE_ROOF_OFFSET = 30; // PC : 30 - Console : 15

// ============================================================
// ■ 关卡相关
// ============================================================
const int			ADVENTURE_AREAS = 5;
const int			LEVELS_PER_AREA = 10;
const int           NUM_LEVELS = ADVENTURE_AREAS * LEVELS_PER_AREA;
const int			FINAL_LEVEL = NUM_LEVELS;
const int           FLAG_RAISE_TIME = 100;
const int           LAST_STAND_FLAGS = 5;
const int           ZOMBIE_COUNTDOWN_FIRST_WAVE = 1800;
const int           ZOMBIE_COUNTDOWN = 2500;
const int           ZOMBIE_COUNTDOWN_RANGE = 600;
const int           ZOMBIE_COUNTDOWN_BEFORE_FLAG = 4500;
const int           ZOMBIE_COUNTDOWN_BEFORE_REPICK = 5499;
const int           ZOMBIE_COUNTDOWN_MIN = 400;
const int           FOG_BLOW_RETURN_TIME = 2000;
const int           SUN_COUNTDOWN = 425;
const int           SUN_COUNTDOWN_RANGE = 275;
const int           SUN_COUNTDOWN_MAX = 950;
const int           SURVIVAL_NORMAL_FLAGS = 5;
const int           SURVIVAL_HARD_FLAGS = 10;

// ============================================================
// ■ 商店相关
// ============================================================
const int           STORESCREEN_ITEMOFFSET_1_X = 422;
const int           STORESCREEN_ITEMOFFSET_1_Y = 206;
const int           STORESCREEN_ITEMOFFSET_2_X = 372;
const int           STORESCREEN_ITEMOFFSET_2_Y = 310;
const int           STORESCREEN_ITEMSIZE = 74;
const int           STORESCREEN_COINBANK_X = 650;
const int           STORESCREEN_COINBANK_Y = 559;
const int           STORESCREEN_PAGESTRING_X = 470;
const int           STORESCREEN_PAGESTRING_Y = 500;

// ============================================================
// ■ REMOVE THE // from the defines below to ENABLE them or INSERT // to DISABLE the feature.
// ============================================================

#ifdef _GOTY
#define _HAS_NEW_DANCERS
#define _HAS_ACHIEVEMENTS
#define _HAS_ZOMBATAR
#define _HAS_UNLOCK
// Unused or Restored
//#define _HAS_LEVELSELECTOR 
//#define _HAS_MORESCREEN
#endif

#ifdef _DEBUG
#define _SHOW_OUTPUT_CONSOLE
#define _SHOW_LIMBO_PAGE
#endif

// Resources
#define _ALLOW_RESOURCE_PACKS

// Unused or Restored
//#define _HAS_BLOOM_AND_DOOM_CONTENTS
#define _HAS_EXTENDED_MINIGAMES
//#define _HAS_UNUSED_ACHIEVEMENTS

//Ported from other editions
#define _MOBILE_MINIGAMES
#define _DS_MINIGAMES
#define _CONSOLE_MINIGAMES
#define _REPLANTED_SPEED_CONTROL
// 主机版的本地双人模式。只包含同机双人，不含任何联机代码。
#define _HAS_LOCAL_MULTIPLAYER
//#define _HAS_NEW_GIGA_ZOMBIES
//#define _HAS_SCORE_SYSTEM

// Quality Of Life
#define _HAS_HEALTHBAR_TOGGLE
//#define _ALLOW_SWIPE
//#define _HAS_GAMESELECTOR_SPOTLIGHT
#define _HAS_ANIMATED_WOOD_SIGN
#define _HAS_KERNELPULT_BUTTER_IDLE

// Quirky stuff
//#define _HAS_ROOF_SLOPE_ANGLE

// Rebalancing
//#define _PIERCING_CACTUS
//#define _SPLASH_SNOWPEA

#include "ConstEnums.h" // Include the enums at the end to properly setup the entries related to the defines
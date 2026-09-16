#ifndef __LAWNPLAYER_H__
#define __LAWNPLAYER_H__

#include "../GameConstants.h"

#ifdef _HAS_LOCAL_MULTIPLAYER

class CursorObject;
class SeedBank;

// ====================================================================================================
// ▲ 本地多人游戏：玩家与输入抽象层
// ----------------------------------------------------------------------------------------------------
// 每名玩家的输入都先被采样成一份 PlayerInputState 快照，再由 Board 转译成点击事件。
// 该快照是一个纯数据结构，不含任何设备句柄，因此日后若要接入网络传输，
// 只需换掉 LawnPlayerInput::PollDevice 的来源即可，无须改动 Board 一侧的逻辑。
// 本分支不含任何网络代码。
// ====================================================================================================
enum PlayerInputDevice
{
	INPUT_DEVICE_MOUSE,			// 玩家一：系统鼠标（原版操作）
	INPUT_DEVICE_GAMEPAD,		// 玩家二：SDL3 手柄驱动的虚拟光标
	INPUT_DEVICE_KEYBOARD		// 玩家二：无手柄时的键盘回退方案
};

struct PlayerInputState
{
	int					mPointerX;			// 虚拟光标位置，与真实鼠标处于同一坐标系
	int					mPointerY;
	bool				mPrimaryDown;		// 相当于鼠标左键
	bool				mSecondaryDown;		// 相当于鼠标右键（铲子）
	bool				mPrevPrimaryDown;
	bool				mPrevSecondaryDown;

	void				Reset(int thePointerX, int thePointerY);
	inline bool			PrimaryPressed() const { return mPrimaryDown && !mPrevPrimaryDown; }
	inline bool			PrimaryReleased() const { return !mPrimaryDown && mPrevPrimaryDown; }
	inline bool			SecondaryPressed() const { return mSecondaryDown && !mPrevSecondaryDown; }
};

class LawnPlayer
{
public:
	int					mPlayerIndex;
	PlayerInputDevice	mInputDevice;
	PlayerInputState	mInput;

	// 该玩家自己的一套游戏状态。玩家一仍然直接使用 Board 上的同名成员，
	// 只有玩家二的状态存放在这里，并在处理其输入/绘制时与 Board 的成员互换。
	CursorObject*		mCursorObject;
	SeedBank*			mSeedBank;
	int					mSunMoney;
	bool				mActive;

public:
	LawnPlayer();

	void				Reset(int thePlayerIndex);
	void				UpdateInput(int theBoardWidth, int theBoardHeight);
};

// 输入设备的采样。手柄按 SDL3 的 Gamepad API 打开，键盘用 SDL_GetKeyboardState 直接查询；
// 两者都只读取状态，不消费事件，因此不会干扰 LawnApp 现有的事件循环。
namespace LawnPlayerInput
{
	void				Initialize();
	void				Shutdown();
	bool				HasGamepad();
	PlayerInputDevice	PickDeviceForPlayer(int thePlayerIndex);
	void				PollDevice(PlayerInputDevice theDevice, PlayerInputState& theState, int theBoardWidth, int theBoardHeight);
};

#endif

#endif

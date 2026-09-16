#include "LawnPlayer.h"

#ifdef _HAS_LOCAL_MULTIPLAYER

#include <SDL3/SDL.h>

//0x000000（新增内容，原版无对应实现）
void PlayerInputState::Reset(int thePointerX, int thePointerY)
{
	mPointerX = thePointerX;
	mPointerY = thePointerY;
	mPrimaryDown = false;
	mSecondaryDown = false;
	mPrevPrimaryDown = false;
	mPrevSecondaryDown = false;
}

LawnPlayer::LawnPlayer()
{
	mPlayerIndex = 0;
	mInputDevice = PlayerInputDevice::INPUT_DEVICE_MOUSE;
	mCursorObject = nullptr;
	mSeedBank = nullptr;
	mSunMoney = 0;
	mActive = false;
	mInput.Reset(0, 0);
}

void LawnPlayer::Reset(int thePlayerIndex)
{
	mPlayerIndex = thePlayerIndex;
	mSunMoney = 0;
	mActive = false;
	mInputDevice = LawnPlayerInput::PickDeviceForPlayer(thePlayerIndex);
	mInput.Reset(LOCAL_PLAYER_2_START_X, LOCAL_PLAYER_2_START_Y);
}

void LawnPlayer::UpdateInput(int theBoardWidth, int theBoardHeight)
{
	// 手柄可能是在关卡开始之后才插上的，所以每帧都重新确认一次设备
	if (mPlayerIndex > 0)
	{
		mInputDevice = LawnPlayerInput::PickDeviceForPlayer(mPlayerIndex);
	}

	mInput.mPrevPrimaryDown = mInput.mPrimaryDown;
	mInput.mPrevSecondaryDown = mInput.mSecondaryDown;
	LawnPlayerInput::PollDevice(mInputDevice, mInput, theBoardWidth, theBoardHeight);
}

// ====================================================================================================
// ▲ 设备采样
// ====================================================================================================
namespace LawnPlayerInput
{
	static bool			gInitialized = false;
	static SDL_Gamepad*	gGamepad = nullptr;
	static int			gRescanCounter = 0;

	static int ClampToRange(int theValue, int theMin, int theMax)
	{
		if (theValue < theMin)	return theMin;
		if (theValue > theMax)	return theMax;
		return theValue;
	}

	// 若当前没有可用手柄，则（限频）重新扫描一次
	static void EnsureGamepad()
	{
		if (!gInitialized)
		{
			Initialize();
		}
		if (gGamepad != nullptr)
		{
			if (SDL_GamepadConnected(gGamepad))
				return;

			SDL_CloseGamepad(gGamepad);
			gGamepad = nullptr;
		}

		if (gRescanCounter > 0)
		{
			gRescanCounter--;
			return;
		}
		gRescanCounter = GAMEPAD_RESCAN_INTERVAL;

		int aCount = 0;
		SDL_JoystickID* aGamepads = SDL_GetGamepads(&aCount);
		if (aGamepads != nullptr)
		{
			if (aCount > 0)
			{
				gGamepad = SDL_OpenGamepad(aGamepads[0]);
			}
			SDL_free(aGamepads);
		}
	}

	void Initialize()
	{
		if (gInitialized)
			return;

		gInitialized = true;
		// SDL_Init 在 LawnApp 中只申请了 VIDEO 与 AUDIO，手柄子系统单独按需初始化，
		// 初始化失败（例如机器上没有任何手柄驱动）时静默回退到键盘操作。
		SDL_InitSubSystem(SDL_INIT_GAMEPAD);
	}

	void Shutdown()
	{
		if (gGamepad != nullptr)
		{
			SDL_CloseGamepad(gGamepad);
			gGamepad = nullptr;
		}
		if (gInitialized)
		{
			SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
			gInitialized = false;
		}
	}

	bool HasGamepad()
	{
		EnsureGamepad();
		return gGamepad != nullptr;
	}

	PlayerInputDevice PickDeviceForPlayer(int thePlayerIndex)
	{
		if (thePlayerIndex == 0)
			return PlayerInputDevice::INPUT_DEVICE_MOUSE;

		return HasGamepad() ? PlayerInputDevice::INPUT_DEVICE_GAMEPAD : PlayerInputDevice::INPUT_DEVICE_KEYBOARD;
	}

	static void PollGamepad(PlayerInputState& theState)
	{
		if (gGamepad == nullptr)
			return;

		const Sint16 aAxisX = SDL_GetGamepadAxis(gGamepad, SDL_GAMEPAD_AXIS_LEFTX);
		const Sint16 aAxisY = SDL_GetGamepadAxis(gGamepad, SDL_GAMEPAD_AXIS_LEFTY);

		float aMoveX = SDL_abs((int)aAxisX) > GAMEPAD_STICK_DEADZONE ? aAxisX / 32767.0f : 0.0f;
		float aMoveY = SDL_abs((int)aAxisY) > GAMEPAD_STICK_DEADZONE ? aAxisY / 32767.0f : 0.0f;

		// 十字键给出与摇杆推满等效的位移，方便精确地对准格子
		if (SDL_GetGamepadButton(gGamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))	aMoveX = -1.0f;
		if (SDL_GetGamepadButton(gGamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))	aMoveX = 1.0f;
		if (SDL_GetGamepadButton(gGamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))		aMoveY = -1.0f;
		if (SDL_GetGamepadButton(gGamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))	aMoveY = 1.0f;

		theState.mPointerX += (int)(aMoveX * VIRTUAL_CURSOR_SPEED);
		theState.mPointerY += (int)(aMoveY * VIRTUAL_CURSOR_SPEED);

		theState.mPrimaryDown = SDL_GetGamepadButton(gGamepad, SDL_GAMEPAD_BUTTON_SOUTH);
		theState.mSecondaryDown = SDL_GetGamepadButton(gGamepad, SDL_GAMEPAD_BUTTON_EAST);
	}

	static void PollKeyboard(PlayerInputState& theState)
	{
		const bool* aKeys = SDL_GetKeyboardState(nullptr);
		if (aKeys == nullptr)
			return;

		// 玩家一用鼠标，所以玩家二占用方向键与右侧的两个修饰键，不与调试快捷键冲突
		if (aKeys[SDL_SCANCODE_LEFT])	theState.mPointerX -= VIRTUAL_CURSOR_SPEED;
		if (aKeys[SDL_SCANCODE_RIGHT])	theState.mPointerX += VIRTUAL_CURSOR_SPEED;
		if (aKeys[SDL_SCANCODE_UP])		theState.mPointerY -= VIRTUAL_CURSOR_SPEED;
		if (aKeys[SDL_SCANCODE_DOWN])	theState.mPointerY += VIRTUAL_CURSOR_SPEED;

		theState.mPrimaryDown = aKeys[SDL_SCANCODE_RCTRL] || aKeys[SDL_SCANCODE_RETURN];
		theState.mSecondaryDown = aKeys[SDL_SCANCODE_RSHIFT];
	}

	void PollDevice(PlayerInputDevice theDevice, PlayerInputState& theState, int theBoardWidth, int theBoardHeight)
	{
		switch (theDevice)
		{
		case PlayerInputDevice::INPUT_DEVICE_GAMEPAD:
			PollGamepad(theState);
			break;

		case PlayerInputDevice::INPUT_DEVICE_KEYBOARD:
			PollKeyboard(theState);
			break;

		default:
			// 鼠标玩家的位置由 Board 直接从 WidgetManager 取得，这里无须采样
			break;
		}

		theState.mPointerX = ClampToRange(theState.mPointerX, 0, theBoardWidth - 1);
		theState.mPointerY = ClampToRange(theState.mPointerY, 0, theBoardHeight - 1);
	}
};

#endif

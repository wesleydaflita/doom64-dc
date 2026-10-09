#include "../pad.h"
#include "platform.h"

#define PSP_ANALOG_DEADZONE 24

int last_joyx;
int last_joyy;
int last_Ltrig;
int last_Rtrig;

int I_GetControllerData(void)
{
	SceCtrlData state;
	int buttons = 0;

	if (PSP_ReadController(&state) < 0)
		return 0;

	buttons |= (state.Buttons & PSP_CTRL_START) ? PAD_START : 0;
	buttons |= (state.Buttons & PSP_CTRL_UP) ? PAD_UP : 0;
	buttons |= (state.Buttons & PSP_CTRL_DOWN) ? PAD_DOWN : 0;
	buttons |= (state.Buttons & PSP_CTRL_LEFT) ? PAD_LEFT : 0;
	buttons |= (state.Buttons & PSP_CTRL_RIGHT) ? PAD_RIGHT : 0;
	buttons |= (state.Buttons & PSP_CTRL_CROSS) ? PAD_Z_TRIG : 0;
	buttons |= (state.Buttons & PSP_CTRL_CIRCLE) ? PAD_RIGHT_C : 0;
	buttons |= (state.Buttons & PSP_CTRL_SQUARE) ? PAD_A : 0;
	buttons |= (state.Buttons & PSP_CTRL_TRIANGLE) ? PAD_B : 0;
	buttons |= (state.Buttons & PSP_CTRL_LTRIGGER) ? PAD_L_TRIG : 0;
	buttons |= (state.Buttons & PSP_CTRL_RTRIGGER) ? PAD_R_TRIG : 0;
	buttons |= (state.Buttons & PSP_CTRL_SELECT) ? PAD_UP_C : 0;

	last_joyx = (int)state.Lx - 128;
	last_joyy = (int)state.Ly - 128;
	last_Ltrig = (state.Buttons & PSP_CTRL_LTRIGGER) ? 255 : 0;
	last_Rtrig = (state.Buttons & PSP_CTRL_RTRIGGER) ? 255 : 0;
	if (last_joyy == -128)
		last_joyy = -127;

	if (last_joyx > PSP_ANALOG_DEADZONE || last_joyx < -PSP_ANALOG_DEADZONE)
		buttons |= (last_joyx & 0xff) << 8;
	else
		last_joyx = 0;

	if (last_joyy > PSP_ANALOG_DEADZONE || last_joyy < -PSP_ANALOG_DEADZONE)
		buttons |= (int8_t)-last_joyy & 0xff;
	else
		last_joyy = 0;

	return buttons;
}

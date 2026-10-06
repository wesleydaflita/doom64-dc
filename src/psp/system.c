#include "../doomdef.h"
#include "platform.h"

#include <pspdebug.h>
#include <pspmoduleinfo.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("Doom 64 PSP", 0, 1, 0);
PSP_HEAP_SIZE_KB(-1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

extern void D_DoomMain(void);
extern void R_Init(void);
extern int globallump;
extern int globalcm;

volatile int32_t drawsync1 = 1;
volatile int32_t drawsync2;
volatile int32_t vsync;
uint32_t NextFrameIdx;
boolean disabledrawing;
atomic_int rdpmsg;
int early_error = 1;

void *memset(void *destination, int value, size_t size)
{
	uint8_t *bytes = destination;
	volatile uint8_t *memory = bytes;
	uint8_t byte = (uint8_t)value;

	for (size_t i = 0; i < size; i++)
		memory[i] = byte;

	return destination;
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	D_DoomMain();
	sceKernelExitGame();
	return 0;
}

void I_Init(void)
{
	PSP_GUInit();
	global_render_state.quality = q_ultra;
	global_render_state.fps_uncap = 1;
	drawsync1 = 1;
}

void I_ClearFrame(void)
{
	NextFrameIdx++;
	globallump = -1;
	globalcm = -2;
}

void I_DrawFrame(void) {}
void I_WIPE_MeltScreen(void) {}
void I_WIPE_FadeOutScreen(void) {}
void I_CheckGFX(void) {}
void I_GetScreenGrab(void) {}

void __attribute__((noreturn)) __I_Error(const char *funcname, char *error, ...)
{
	char message[512];
	va_list args;

	va_start(args, error);
	vsnprintf(message, sizeof(message), error, args);
	va_end(args);
	pspDebugScreenInit();
	pspDebugScreenPrintf("%s: %s\n", funcname, message);
	sceKernelExitGame();
	for (;;) {}
}

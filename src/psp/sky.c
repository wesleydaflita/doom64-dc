#include "../doomdef.h"
#include "../r_local.h"

fixed_t FogNear = 1000;
int FogColor;
int Skyfadeback;
skyfunc_t R_RenderSKY;
pvr_ptr_t pvrsky[2];
int lastlump[2] = { -1, -1 };
int add_lightning;

void R_SetupSky(void)
{
	R_RenderSKY = NULL;
	pvrsky[0] = NULL;
	pvrsky[1] = NULL;
	lastlump[0] = -1;
	lastlump[1] = -1;
	Skyfadeback = 0;
}

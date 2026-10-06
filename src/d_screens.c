/* D_screens.c */

#include "i_main.h"
#include "doomdef.h"
#include "r_local.h"
#include "st_main.h"

int D_RunDemo(char *name, skill_t skill, int map)
{
	int lump;
	int exit;

	demo_p = Z_Alloc(16000, PU_STATIC, NULL);
	memset(demo_p, 0, 16000);

	lump = W_GetNumForName(name);
	W_ReadLump(lump, demo_p, dec_d64);

	// demo data needs endian-swapping
	for (int i = 0; i < 4000; i++)
		demo_p[i] = Swap32(demo_p[i]);

	exit = G_PlayDemoPtr(skill, map);
	Z_Free(demo_p);

	return exit;
}

int D_TitleMap(void)
{
	int exit;

	demo_p = Z_Alloc(16000, PU_STATIC, NULL);
	memset(demo_p, 0, 16000);
	memcpy(demo_p, DefaultConfiguration, 13 * sizeof(int));
	exit = G_PlayDemoPtr(sk_medium, 33);
	Z_Free(demo_p);

	return exit;
}

int D_LegalTicker(void)
{
	if ((ticon - last_ticon) >= 150) { // 5 * TICRATE
		text_alpha -= 8;
		if (text_alpha < 0) {
			text_alpha = 0;
			return 8;
		}
	}
	return 0;
}

void D_DrawLegal(void)
{
	I_ClearFrame();

	M_DrawBackground(USLEGAL, text_alpha);

	I_DrawFrame();
}
void D_SplashScreen(void)
{
	text_alpha = 0xff;
	last_ticon = 0;
	MiniLoop(NULL, NULL, D_LegalTicker, D_DrawLegal);
}

static int cred_step;
static int cred1_alpha;
static int cred2_alpha;
static int cred_next;

int D_Credits(void)
{
	int exit;

	cred_next = 0;
	cred1_alpha = 0;
	cred2_alpha = 0;
	cred_step = 0;
	exit = MiniLoop(NULL, NULL, D_CreditTicker, D_CreditDrawer);

	return exit;
}

int D_CreditTicker(void)
{
	if (((uint32_t)ticbuttons[0] >> 16) != 0)
		return ga_exit;

	if ((cred_next == 0) || (cred_next == 1)) {
		if (cred_step == 0) {
			cred1_alpha += 8;
			if (cred1_alpha >= 255) {
				cred1_alpha = 0xff;
				cred_step = 1;
			}
		} else if (cred_step == 1) {
			cred2_alpha += 8;
			if (cred2_alpha >= 255) {
				cred2_alpha = 0xff;
				last_ticon = ticon;
				cred_step = 2;
			}
		} else if (cred_step == 2) {
			if ((ticon - last_ticon) >= 180) // 6 * TICRATE
				cred_step = 3;
		} else {
			cred1_alpha -= 8;
			cred2_alpha -= 8;
			if (cred1_alpha < 0) {
				cred_next += 1;
				cred1_alpha = 0;
				cred2_alpha = 0;
				cred_step = 0;
			}
		}
	} else if (cred_next == 2)
		return ga_exitdemo;

	return ga_nothing;
}

void D_CreditDrawer(void)
{
	I_ClearFrame();

	if (cred_next == 0) {
		M_DrawBackground(IDCRED1, cred1_alpha);
		M_DrawBackground(IDCRED2, cred2_alpha);
	} else {
		if ((cred_next == 1) || (cred_next == 2)) {
			M_DrawBackground(WMSCRED1, cred1_alpha);
			M_DrawBackground(WMSCRED2, cred2_alpha);
		}
	}

	I_DrawFrame();
}

#include "../doomdef.h"
#include "../r_local.h"

#include <stdlib.h>

fixed_t FogNear = 1000;
int FogColor;
int Skyfadeback;
skyfunc_t R_RenderSKY;
pvr_ptr_t pvrsky[2];
int lastlump[2] = { -1, -1 };
int add_lightning;

static pvr_ptr_t sky_texture;
static int sky_width;
static int sky_height;

static void R_RenderPSPSky(void)
{
	const float sky_depth = PSP_GU_FAR_Z - 1.0f;
	float u0 = -((viewangle >> 22) & 255) * recip256;
	float u1 = u0 + 1.0f;
	pvr_vertex_t vertices[4] = {
		{PVR_CMD_VERTEX, -sky_depth, sky_depth, sky_depth, 0xffffffff, 0, u0, 0.0f},
		{PVR_CMD_VERTEX, -sky_depth, -sky_depth, sky_depth, 0xffffffff, 0, u0, 1.0f},
		{PVR_CMD_VERTEX, sky_depth, sky_depth, sky_depth, 0xffffffff, 0, u1, 0.0f},
		{PVR_CMD_VERTEX_EOL, sky_depth, -sky_depth, sky_depth, 0xffffffff, 0, u1, 1.0f}
	};

	if (!sky_texture) {
		spriteN64_t *header;
		uint8_t *data = W_CacheLumpName("SPACE", PU_STATIC, dec_jag);
		uint8_t *source;
		uint16_t palette[256];
		uint8_t *pixels;
		short *palette_data;

		if (!data)
			I_Error("Could not load SPACE sky texture");
		header = (spriteN64_t *)data;
		source = data + sizeof(*header);
		sky_width = (SwapShort(header->width) + 7) & ~7;
		sky_height = SwapShort(header->height);
		if (sky_width < 2 || sky_height < 2 ||
				sky_width > 512 || sky_height > 512 ||
				(sky_width & (sky_width - 1)) ||
				(sky_height & (sky_height - 1)))
			I_Error("Invalid SPACE sky dimensions %d x %d",
				sky_width, sky_height);

		pixels = malloc((size_t)sky_width * sky_height);
		sky_texture = PSP_GUAllocTexture(
			(size_t)sky_width * sky_height * sizeof(uint16_t));
		if (!pixels || !sky_texture)
			I_Error("Could not allocate PSP sky texture");
		if (PSP_GUUntwiddle8(source, pixels, sky_width, sky_height))
			I_Error("Could not untwiddle SPACE sky texture");

		palette_data = (short *)(source + sky_width * sky_height);
		for (int i = 0; i < 256; i++) {
			short value = SwapShort(palette_data[i]);
			uint8_t red = (value & 0xf800) >> 8;
			uint8_t green = (value & 0x07c0) >> 3;
			uint8_t blue = (value & 0x003e) << 2;

			palette[i] = get_color_argb1555(red, green, blue, 1);
		}
		for (int i = 0; i < sky_width * sky_height; i++)
			((uint16_t *)sky_texture)[i] = palette[pixels[i]];

		free(pixels);
		Z_Free(data);
	}

	PSP_GUDrawPanoramicSky(sky_texture, sky_width, sky_height, vertices, 4);
}

void R_SetupSky(void)
{
	R_RenderSKY = R_RenderPSPSky;
	pvrsky[0] = NULL;
	pvrsky[1] = NULL;
	lastlump[0] = -1;
	lastlump[1] = -1;
	Skyfadeback = 0;
}

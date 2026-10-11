#include "../doomdef.h"
#include "../r_local.h"

#include <stdlib.h>
#include <string.h>

#define SKY_WIDTH 64
#define SKY_HEIGHT 64
#define SKY_IMAGE_CACHE_SIZE 4
#define SKY_SCREEN_LEFT (-103.52941f)
#define SKY_SCREEN_RIGHT 743.52941f

typedef struct {
	int lump;
	int width;
	int height;
	int real_height;
	uint16_t *texture;
} sky_image_t;

fixed_t FogNear = 985;
int FogColor;
int Skyfadeback;
skyfunc_t R_RenderSKY;
pvr_ptr_t pvrsky[2];
int lastlump[2] = { -1, -1 };
int add_lightning;

static sky_image_t sky_images[SKY_IMAGE_CACHE_SIZE];
static uint8_t cloud_density[SKY_WIDTH * SKY_HEIGHT];
static uint16_t cloud_texture[SKY_WIDTH * SKY_HEIGHT]
	__attribute__((aligned(16)));
static uint8_t fire_source[SKY_WIDTH * SKY_HEIGHT];
static uint8_t fire_pixels[SKY_WIDTH * SKY_HEIGHT];
static uint16_t fire_texture[SKY_WIDTH * SKY_HEIGHT]
	__attribute__((aligned(16)));
static int cloud_texture_loaded;
static int fire_texture_loaded;
static int active_sky;
static int next_sky_image;

static void PSP_LoadSkyImage(sky_image_t *image, int lump,
		int transparent_zero)
{
	spriteN64_t *header;
	uint8_t *data;
	uint8_t *source;
	uint8_t *pixels;
	uint16_t palette[256];
	uint16_t *texture;
	int width;
	int height;
	int padded_height;

	if (image->texture && image->lump == lump)
		return;

	if (image->texture)
		PSP_GUFreeTexture(image->texture);

	data = W_CacheLumpNum(lump, PU_STATIC, dec_jag);
	if (!data)
		I_Error("Could not load sky lump %d", lump);

	header = (spriteN64_t *)data;
	width = (SwapShort(header->width) + 7) & ~7;
	height = SwapShort(header->height);
	padded_height = 1;
	while (padded_height < height)
		padded_height <<= 1;
	if (width < 2 || height < 2 || width > 512 || padded_height > 512 ||
			(width & (width - 1)))
		I_Error("Invalid sky dimensions %d x %d", width, height);

	source = data + sizeof(*header);
	pixels = calloc((size_t)width * padded_height, 1);
	texture = PSP_GUAllocTexture((size_t)width * padded_height *
		sizeof(*texture));
	if (!pixels || !texture) {
		free(pixels);
		PSP_GUFreeTexture(texture);
		I_Error("Could not allocate sky texture");
	}

	// N64 layout: linear rows, with the two dwords of every 8 bytes
	// swapped on odd rows
	for (int y = 0; y < height; y++) {
		const uint8_t *row = source + (size_t)y * width;
		uint8_t *out = pixels + (size_t)y * width;

		for (int x = 0; x < width; x++)
			out[x] = (y & 1) ? row[x ^ 4] : row[x];
	}

	source += (size_t)width * height;
	for (int i = 0; i < 256; i++) {
		uint16_t value = SwapShort(((uint16_t *)source)[i]);
		uint8_t red = (value & 0xf800) >> 8;
		uint8_t green = (value & 0x07c0) >> 3;
		uint8_t blue = (value & 0x003e) << 2;

		palette[i] = get_color_argb1555(red, green, blue,
			!transparent_zero || i != 0);
	}
	for (size_t i = 0; i < (size_t)width * padded_height; i++)
		texture[i] = (i < (size_t)width * height) ?
			palette[pixels[i]] : 0;

	free(pixels);
	Z_Free(data);
	sceKernelDcacheWritebackInvalidateRange(texture,
		(size_t)width * padded_height * sizeof(*texture));
	image->lump = lump;
	image->width = width;
	image->height = padded_height;
	image->real_height = height;
	image->texture = texture;
}

static sky_image_t *PSP_GetSkyImage(const char *name, int transparent_zero)
{
	int lump = W_GetNumForName((char *)name);
	sky_image_t *image = NULL;

	for (int i = 0; i < SKY_IMAGE_CACHE_SIZE; i++) {
		if (sky_images[i].texture && sky_images[i].lump == lump) {
			image = &sky_images[i];
			break;
		}
		if (!image && !sky_images[i].texture)
			image = &sky_images[i];
	}
	if (!image) {
		image = &sky_images[next_sky_image];
		next_sky_image = (next_sky_image + 1) % SKY_IMAGE_CACHE_SIZE;
	}

	PSP_LoadSkyImage(image, lump, transparent_zero);
	return image;
}

static void PSP_DrawSkyRect(const uint16_t *texture, int width, int height,
		float y_top, float y_bottom, float u0, float u1, float v0, float v1,
		uint32_t tint, int wrap_s, int wrap_t, int translucent)
{
	pvr_vertex_t vertices[4] = {
		{PVR_CMD_VERTEX, SKY_SCREEN_LEFT, y_top, 1.0f, tint, 0, u0, v0},
		{PVR_CMD_VERTEX, SKY_SCREEN_LEFT, y_bottom, 1.0f, tint, 0, u0, v1},
		{PVR_CMD_VERTEX, SKY_SCREEN_RIGHT, y_top, 1.0f, tint, 0, u1, v0},
		{PVR_CMD_VERTEX_EOL, SKY_SCREEN_RIGHT, y_bottom, 1.0f, tint, 0, u1, v1}
	};

	PSP_GUDrawSkyTexture(texture, width, height, vertices, 4, wrap_s, wrap_t,
		translucent);
}

static void PSP_FillSky(uint32_t color)
{
	pvr_vertex_t vertices[4] = {
		{PVR_CMD_VERTEX, SKY_SCREEN_LEFT, 0.0f, 1.0f, color, 0, 0, 0},
		{PVR_CMD_VERTEX, SKY_SCREEN_LEFT, 480.0f, 1.0f, color, 0, 0, 0},
		{PVR_CMD_VERTEX, SKY_SCREEN_RIGHT, 0.0f, 1.0f, color, 0, 0, 0},
		{PVR_CMD_VERTEX_EOL, SKY_SCREEN_RIGHT, 480.0f, 1.0f, color, 0, 0, 0}
	};

	PSP_GUDrawFlat(vertices, 4);
}

static void PSP_LoadCloudTexture(void)
{
	uint8_t *data;
	uint8_t *source;

	if (cloud_texture_loaded)
		return;

	data = W_CacheLumpName("CLOUD", PU_STATIC, dec_jag);
	if (!data)
		I_Error("Could not load CLOUD sky texture");
	source = data + 8;
	memcpy(cloud_density, source, sizeof(cloud_density));
	Z_Free(data);
	for (int word = 0; word < SKY_WIDTH * SKY_HEIGHT / 4; word += 2) {
		if (word & (SKY_WIDTH / 4)) {
			for (int byte = 0; byte < 4; byte++) {
				uint8_t value = cloud_density[word * 4 + byte];

				cloud_density[word * 4 + byte] =
					cloud_density[(word + 1) * 4 + byte];
				cloud_density[(word + 1) * 4 + byte] = value;
			}
		}
	}
	cloud_texture_loaded = 1;
}

static void PSP_UpdateCloudTexture(uint32_t background, uint32_t cloud)
{
	int background_red = (background >> 16) & 0xff;
	int background_green = (background >> 8) & 0xff;
	int background_blue = background & 0xff;
	int cloud_red = (cloud >> 16) & 0xff;
	int cloud_green = (cloud >> 8) & 0xff;
	int cloud_blue = cloud & 0xff;

	for (int i = 0; i < SKY_WIDTH * SKY_HEIGHT; i++) {
		int density = cloud_density[i];
		int red = (background_red * (255 - density) +
			cloud_red * density) / 255;
		int green = (background_green * (255 - density) +
			cloud_green * density) / 255;
		int blue = (background_blue * (255 - density) +
			cloud_blue * density) / 255;

		cloud_texture[i] = get_color_argb1555(red, green, blue, 1);
	}
	sceKernelDcacheWritebackInvalidateRange(cloud_texture,
		sizeof(cloud_texture));
}

static void PSP_LoadFireTexture(void)
{
	uint8_t *data;

	if (fire_texture_loaded)
		return;

	data = W_CacheLumpName("FIRE", PU_STATIC, dec_jag);
	if (!data)
		I_Error("Could not load FIRE sky texture");
	memcpy(fire_source, data + 8, sizeof(fire_source));
	memcpy(fire_pixels, fire_source, sizeof(fire_pixels));
	Z_Free(data);
	fire_texture_loaded = 1;
}

static void PSP_UpdateFire(void)
{
	static int last_update_tic = -1;

	if (gametic < last_update_tic)
		last_update_tic = -1;

	if (!gamepaused && (gametic & 1) && last_update_tic != gametic) {
		uint32_t random_state = (uint32_t)gametic ^ 0x9e3779b9u;

		last_update_tic = gametic;
		for (int x = 0; x < SKY_WIDTH; x++) {
			for (int y = 1; y < SKY_HEIGHT; y++) {
				random_state = random_state * 1664525u + 1013904223u;
				int random = (int)(random_state >> 24);
				int source_x = (x - (random & 3) + 1) & (SKY_WIDTH - 1);
				int value = fire_pixels[y * SKY_WIDTH + x];
				int decay = (random & 1) ? 16 : 0;
				int output_y = y - 1;

				fire_pixels[output_y * SKY_WIDTH + source_x] =
					value > decay ? value - decay : 0;
			}
		}
	}

	for (int i = 0; i < SKY_WIDTH * SKY_HEIGHT; i++) {
		int intensity = fire_pixels[i];
		int red = active_sky == 9 ? intensity / 2 : intensity;
		int green = active_sky == 9 ? intensity : intensity / 2;
		fire_texture[i] = get_color_argb1555(red, green, 0, 1);
	}
	sceKernelDcacheWritebackInvalidateRange(fire_texture,
		sizeof(fire_texture));
}

static void R_RenderPSPSky(void)
{
	const float angle = (float)((viewangle >> 22) & 255) * recip256;
	const float game_scroll = (float)gametic / 2048.0f;
	uint32_t cloud_color = 0xffffffff;
	uint32_t background_color = 0xff000000;
	int logo_alpha = 0;

	if (active_sky == 7 && Skyfadeback) {
		Skyfadeback += 4;
		if (Skyfadeback > 255)
			Skyfadeback = 255;
		logo_alpha = Skyfadeback > 128 ? 128 : Skyfadeback;
	}

	switch (active_sky) {
	case 1:
	case 10:
		background_color = (gametic % 180) < 3 ?
			0xff9090b0 : 0xff080820;
		PSP_FillSky(background_color);
		cloud_color = 0xffb080ff;
		break;
	case 2:
		background_color = 0xff200808;
		PSP_FillSky(background_color);
		cloud_color = 0xffff5050;
		break;
	case 3:
		background_color = 0xff180804;
		PSP_FillSky(background_color);
		cloud_color = 0xffd07040;
		break;
	case 4:
	case 9:
		PSP_FillSky(0xff080808);
		PSP_UpdateFire();
		PSP_DrawSkyRect(fire_texture, SKY_WIDTH, SKY_HEIGHT, 0.0f,
			240.0f, -angle, 5.0f - angle, 0.0f, 1.0f,
			0xffffffff, GU_REPEAT, GU_CLAMP, 0);
		return;
	case 5:
		background_color = 0xff301008;
		PSP_FillSky(background_color);
		cloud_color = 0xffd07840;
		break;
	case 6:
	case 7:
	case 11:
		PSP_FillSky(0xff000000);
		break;
	case 8:
		PSP_FillSky(0xff003838);
		return;
	default:
		PSP_FillSky(0xff000000);
		break;
	}

	if (active_sky == 1 || active_sky == 2 || active_sky == 3 ||
			active_sky == 5 || active_sky == 10) {
		PSP_LoadCloudTexture();
		PSP_UpdateCloudTexture(background_color, cloud_color);
		PSP_DrawSkyRect(cloud_texture, SKY_WIDTH, SKY_HEIGHT, 0.0f,
			240.0f, -angle + game_scroll, 1.5f - angle + game_scroll,
			game_scroll, 2.0f + game_scroll, 0xffffffff,
			GU_REPEAT, GU_REPEAT, 0);
	}

	if (active_sky == 6 || active_sky == 7 || active_sky == 11) {
		sky_image_t *space = PSP_GetSkyImage("SPACE", 0);
		if (active_sky != 7 || Skyfadeback < 255) {
			PSP_DrawSkyRect(space->texture, space->width, space->height,
				(128.0f - space->real_height) * 2.0f, 256.0f, -angle, 1.0f - angle,
				0.0f, (float)space->real_height / space->height, 0xffffffff, GU_REPEAT, GU_CLAMP, 0);
		}
	}

	if (active_sky == 3 || active_sky == 10 || active_sky == 11) {
		const char *mountain_name = active_sky == 3 ? "MOUNTB" :
			active_sky == 10 ? "MOUNTC" : "MOUNTA";
		sky_image_t *mountain = PSP_GetSkyImage(mountain_name, 1);

		PSP_DrawSkyRect(mountain->texture, mountain->width,
			mountain->height, (170.0f - mountain->real_height) * 2.0f, 340.0f,
			-angle, 1.0f - angle, 0.0f,
			(float)mountain->real_height / mountain->height, 0xffffffff, GU_REPEAT, GU_CLAMP, 1);
	}

	if (logo_alpha)
		M_DrawBackground(EVIL, logo_alpha);
}

void R_SetupSky(void)
{
	active_sky = skytexture;
	if (active_sky < 1 || active_sky > 11)
		active_sky = 6;

	FogNear = 985;
	FogColor = 0;
	if (active_sky == 5) {
		FogNear = 975;
		FogColor = PACKRGBA(48, 16, 8, 0);
	} else if (active_sky == 7) {
		FogNear = 995;
	} else if (active_sky == 8) {
		FogNear = 975;
		FogColor = PACKRGBA(0, 64, 64, 0);
	}

	if (active_sky == 3 || active_sky == 10 || active_sky == 11)
		(void)PSP_GetSkyImage(active_sky == 3 ? "MOUNTB" :
			active_sky == 10 ? "MOUNTC" : "MOUNTA", 1);
	if (active_sky == 4 || active_sky == 9)
		PSP_LoadFireTexture();
	if (active_sky == 1 || active_sky == 2 || active_sky == 3 ||
			active_sky == 5 || active_sky == 10)
		PSP_LoadCloudTexture();
	if (active_sky == 6 || active_sky == 7 || active_sky == 11)
		(void)PSP_GetSkyImage("SPACE", 0);

	R_RenderSKY = R_RenderPSPSky;
	Skyfadeback = 0;
}

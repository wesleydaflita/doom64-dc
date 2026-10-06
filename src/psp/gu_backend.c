#include "platform.h"

#include <pspdisplay.h>
#include <psputils.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#define PSP_FRAME_WIDTH 480
#define PSP_FRAME_HEIGHT 272
#define PSP_BUFFER_WIDTH 512
#define PSP_GU_LIST_SIZE (512 * 1024)

static unsigned int __attribute__((aligned(16))) gu_list[PSP_GU_LIST_SIZE];
extern void __I_Error(const char *funcname, char *error, ...);

typedef struct {
	uint32_t color;
	float x, y, z;
} gu_vertex_t;

typedef struct {
	float u, v;
	uint32_t color;
	float x, y, z;
} gu_texture_vertex_t;

static uint16_t __attribute__((aligned(16))) gu_palette[1024];

static int PSP_NormalizeTextureSize(int *width, int *height)
{
	int w = *width;
	int h = *height;
	int pw = 1;
	int ph = 1;

	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;

	if (w > 512)
		w = 512;
	if (h > 512)
		h = 512;

	while (pw < w)
		pw <<= 1;
	while (ph < h)
		ph <<= 1;

	*width = pw;
	*height = ph;
	return pw * ph;
}

void *PSP_GUAllocTexture(size_t size)
{
	return memalign(16, size);
}

void PSP_GUFreeTexture(void *texture)
{
	free(texture);
}

void PSP_GULoadTexture(const void *source, void *texture, size_t size)
{
	if (source && texture)
		memcpy(texture, source, size);
}

static size_t PSP_TwiddleIndex(unsigned int x, unsigned int y)
{
	size_t index = 0;

	for (unsigned int bit = 0; bit < 10; bit++) {
		index |= (size_t)((x >> bit) & 1) << (bit * 2 + 1);
		index |= (size_t)((y >> bit) & 1) << (bit * 2);
	}

	return index;
}

int PSP_GUUntwiddle8(const uint8_t *source, uint8_t *texture,
		int width, int height)
{
	int min_dimension;
	int mask;

	if (!source || !texture || width < 2 || height < 2 ||
			(width & 1) || (height & 1))
		return -1;

	min_dimension = width < height ? width : height;
	mask = min_dimension - 1;

	for (int y = 0; y < height; y += 2) {
		for (int x = 0; x < width; x++) {
			size_t index = PSP_TwiddleIndex(
				(unsigned)(y & mask) / 2, (unsigned)x & (unsigned)mask) +
				(size_t)(x / min_dimension + y / min_dimension) *
					(size_t)min_dimension * (size_t)min_dimension / 2;
			texture[y * width + x] = source[index * 2];
			texture[(y + 1) * width + x] = source[index * 2 + 1];
		}
	}

	return 0;
}

void PSP_GUTextureContext(pvr_poly_cxt_t *context, int list, int format,
		int width, int height, pvr_ptr_t texture, int filter)
{
	if (!context)
		return;

	PSP_NormalizeTextureSize(&width, &height);

	memset(context, 0, sizeof(*context));
	context->list = list;
	context->txr.format = format;
	context->txr.width = width;
	context->txr.height = height;
	context->txr.base = texture;
	context->txr.filter = filter;
	context->txr.wrap_s = GU_CLAMP;
	context->txr.wrap_t = GU_CLAMP;
	if (format & PVR_TXRFMT_PAL8BPP)
		context->txr.palette = (format >> 8) & 3;
}

void PSP_GUCompileTextureHeader(pvr_poly_hdr_t *header,
		const pvr_poly_cxt_t *context)
{
	header->context = *context;
}

void PSP_GUFlatContext(pvr_poly_cxt_t *context, int list)
{
	memset(context, 0, sizeof(*context));
	context->list = list;
}

int PSP_GUInit(void)
{
	const float near_plane = 8.0f;
	const float far_plane = 3808.0f;
	const float depth_scale =
		(far_plane + near_plane) / (far_plane - near_plane);
	const float depth_offset =
		-(2.0f * far_plane * near_plane) / (far_plane - near_plane);
	const ScePspFMatrix4 identity = {
		.x = {1.0f, 0.0f, 0.0f, 0.0f},
		.y = {0.0f, 1.0f, 0.0f, 0.0f},
		.z = {0.0f, 0.0f, 1.0f, 0.0f},
		.w = {0.0f, 0.0f, 0.0f, 1.0f}
	};
	const ScePspFMatrix4 world_projection = {
		.x = {1.0f, 0.0f, 0.0f, 0.0f},
		.y = {0.0f, 1.0f, 0.0f, 0.0f},
		.z = {0.0f, 0.0f, depth_scale, 1.0f},
		.w = {0.0f, 0.0f, depth_offset, 0.0f}
	};

	sceGuInit();
	sceGuStart(GU_DIRECT, gu_list);
	sceGuDrawBuffer(GU_PSM_5650, (void *)GU_VRAM_BP_0, PSP_BUFFER_WIDTH);
	sceGuDispBuffer(PSP_FRAME_WIDTH, PSP_FRAME_HEIGHT,
		(void *)GU_VRAM_BP_1, PSP_BUFFER_WIDTH);
	sceGuDepthBuffer((void *)GU_VRAM_BP_2, PSP_BUFFER_WIDTH);
	sceGuOffset((2048 - (PSP_FRAME_WIDTH / 2)),
		(2048 - (PSP_FRAME_HEIGHT / 2)));
	sceGuViewport(2048, 2048, PSP_FRAME_WIDTH, PSP_FRAME_HEIGHT);
	sceGuSetMatrix(GU_MODEL, &identity);
	sceGuSetMatrix(GU_VIEW, &identity);
	sceGuSetMatrix(GU_PROJECTION, &world_projection);
	sceGuDepthRange(65535, 0);
	sceGuScissor(0, 0, PSP_FRAME_WIDTH, PSP_FRAME_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_CULL_FACE);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuFrontFace(GU_CW);
	sceGuFinish();
	sceGuSync(0, 0);
	sceGuDisplay(GU_TRUE);
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	return 0;
}

void PSP_GUShutdown(void)
{
	sceGuTerm();
}

void PSP_GUBeginFrame(uint32_t clear_color)
{
	sceGuStart(GU_DIRECT, gu_list);
	sceGuDisable(GU_CULL_FACE);
	sceGuClearColor(clear_color);
	sceGuClearDepth(0);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

void PSP_GUEndFrame(void)
{
	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

int PSP_ReadController(SceCtrlData *state)
{
	if (!state)
		return -1;

	return sceCtrlPeekBufferPositive(state, 1);
}

uint64_t PSP_TimeMicroseconds(void)
{
	return sceKernelGetSystemTimeWide();
}

void PSP_GUSetPaletteEntry(int index, uint16_t color)
{
	if ((unsigned)index < 1024)
		gu_palette[index] = color;
}

static void PSP_GUDrawFlatInternal(const pvr_vertex_t *vertices, int count,
		int transform_3d, int blend_state, int triangle_strip)
{
	gu_vertex_t *draw_vertices;

	if (!vertices || count < 3 || count > 5)
		return;

	draw_vertices = sceGuGetMemory(count * sizeof(*draw_vertices));
	if (!draw_vertices)
		__I_Error(__func__, "GU display-list vertex allocation failed");

	for (int i = 0; i < count; i++) {
		uint32_t argb = vertices[i].argb;
		uint32_t red = (argb >> 16) & 0xff;
		uint32_t green = (argb >> 8) & 0xff;
		uint32_t blue = argb & 0xff;

		draw_vertices[i].color = (argb & 0xff000000) | (blue << 16) |
			(green << 8) | red;
		draw_vertices[i].x = vertices[i].x;
		draw_vertices[i].y = vertices[i].y;
		draw_vertices[i].z = vertices[i].z;
	}

	sceKernelDcacheWritebackInvalidateRange(draw_vertices,
		count * sizeof(*draw_vertices));
	if (transform_3d) {
		sceGuEnable(GU_DEPTH_TEST);
		sceGuDepthMask(blend_state ? GU_TRUE : GU_FALSE);
	} else {
		sceGuDisable(GU_DEPTH_TEST);
		sceGuDepthMask(GU_TRUE);
	}
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_ALPHA_TEST);
	if (blend_state < 0) {
		blend_state = 0;
		for (int i = 0; i < count; i++)
			if ((vertices[i].argb >> 24) != 0xff) {
				blend_state = 1;
				break;
			}
	}
	if (blend_state) {
		sceGuEnable(GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA,
			0, 0);
	} else {
		sceGuDisable(GU_BLEND);
	}
	sceGuDrawArray(triangle_strip ? GU_TRIANGLE_STRIP : GU_TRIANGLE_FAN,
		GU_COLOR_8888 | GU_VERTEX_32BITF |
		(transform_3d ? GU_TRANSFORM_3D : GU_TRANSFORM_2D),
		count, NULL, draw_vertices);
}

void PSP_GUDrawFlat(const pvr_vertex_t *vertices, int count)
{
	PSP_GUDrawFlatInternal(vertices, count, 0, -1, 0);
}

static void PSP_GUDrawIndexedInternal(const uint8_t *texture, int width,
		int height, int palette, const pvr_vertex_t *vertices, int count,
		int transform_3d, int translucent, int alpha_test, int filter,
		int triangle_strip, int wrap_s, int wrap_t, int scale_uv)
{
	gu_texture_vertex_t *draw_vertices;

	if (!texture || !vertices || count < 3 || count > 5)
		return;

	PSP_NormalizeTextureSize(&width, &height);
	if (width < 1 || height < 1 || (unsigned)palette > 3)
		return;

	draw_vertices = sceGuGetMemory(count * sizeof(*draw_vertices));
	if (!draw_vertices)
		__I_Error(__func__, "GU display-list texture vertex allocation failed");

	for (int i = 0; i < count; i++) {
		uint32_t argb = vertices[i].argb;
		uint32_t red = (argb >> 16) & 0xff;
		uint32_t green = (argb >> 8) & 0xff;
		uint32_t blue = argb & 0xff;

		draw_vertices[i].u = vertices[i].u * (scale_uv ? width : 1.0f);
		draw_vertices[i].v = vertices[i].v * (scale_uv ? height : 1.0f);
		draw_vertices[i].color = (argb & 0xff000000) |
			(blue << 16) | (green << 8) | red;
		if (transform_3d) {
			draw_vertices[i].x = vertices[i].x;
			draw_vertices[i].y = vertices[i].y;
			draw_vertices[i].z = vertices[i].z;
		} else {
			draw_vertices[i].x =
				vertices[i].x * (PSP_FRAME_WIDTH / 640.0f);
			draw_vertices[i].y =
				vertices[i].y * (PSP_FRAME_HEIGHT / 480.0f);
			draw_vertices[i].z = vertices[i].z;
		}
	}

	sceKernelDcacheWritebackInvalidateRange(draw_vertices,
		count * sizeof(*draw_vertices));
	sceKernelDcacheWritebackInvalidateRange((void *)texture, width * height);
	sceKernelDcacheWritebackInvalidateRange(gu_palette, sizeof(gu_palette));
	sceGuClutMode(GU_PSM_5551, 0, 0xff, 0);
	sceGuClutLoad(32, gu_palette + (palette * 256));
	sceGuTexMode(GU_PSM_T8, 0, 0, GU_FALSE);
	sceGuTexImage(0, width, height, width, texture);
	sceGuTexScale(1.0f, 1.0f);
	sceGuTexOffset(0.0f, 0.0f);
	sceGuTexWrap(wrap_s, wrap_t);
	sceGuTexFilter(filter == PVR_FILTER_BILINEAR ? GU_LINEAR : GU_NEAREST,
		filter == PVR_FILTER_BILINEAR ? GU_LINEAR : GU_NEAREST);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuEnable(GU_TEXTURE_2D);
	if (alpha_test) {
		sceGuAlphaFunc(GU_GREATER, 0, 0xff);
		sceGuEnable(GU_ALPHA_TEST);
	} else {
		sceGuDisable(GU_ALPHA_TEST);
	}
	if (translucent) {
		sceGuEnable(GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	} else {
		sceGuDisable(GU_BLEND);
	}
	if (transform_3d) {
		sceGuEnable(GU_DEPTH_TEST);
		sceGuDepthMask(translucent ? GU_TRUE : GU_FALSE);
	} else {
		sceGuDisable(GU_DEPTH_TEST);
		sceGuDepthMask(GU_TRUE);
	}
	sceGuDrawArray(triangle_strip ? GU_TRIANGLE_STRIP : GU_TRIANGLE_FAN,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF |
		(transform_3d ? GU_TRANSFORM_3D : GU_TRANSFORM_2D),
		count, NULL, draw_vertices);
	sceGuTexFlush();
}

void PSP_GUDrawIndexed(const uint8_t *texture, int width, int height,
		int palette, const pvr_vertex_t *vertices, int count)
{
	PSP_GUDrawIndexedInternal(texture, width, height, palette, vertices,
		count, 0, 1, 1, PVR_FILTER_NONE, 0, GU_CLAMP, GU_CLAMP, 1);
}

static void PSP_GUDraw5551Internal(const uint16_t *texture, int width,
		int height, const pvr_vertex_t *vertices, int count,
		int transform_3d, int translucent, int alpha_test, int filter,
		int triangle_strip, int wrap_s, int wrap_t, int scale_uv)
{
	gu_texture_vertex_t *draw_vertices;

	if (!texture || !vertices || count < 3 || count > 5)
		return;

	PSP_NormalizeTextureSize(&width, &height);
	if (width < 1 || height < 1)
		return;

	draw_vertices = sceGuGetMemory(count * sizeof(*draw_vertices));
	if (!draw_vertices)
		__I_Error(__func__, "GU display-list texture vertex allocation failed");

	for (int i = 0; i < count; i++) {
		uint32_t argb = vertices[i].argb;
		uint32_t red = (argb >> 16) & 0xff;
		uint32_t green = (argb >> 8) & 0xff;
		uint32_t blue = argb & 0xff;

		draw_vertices[i].u = vertices[i].u * (scale_uv ? width : 1.0f);
		draw_vertices[i].v = vertices[i].v * (scale_uv ? height : 1.0f);
		draw_vertices[i].color = (argb & 0xff000000) |
			(blue << 16) | (green << 8) | red;
		if (transform_3d) {
			draw_vertices[i].x = vertices[i].x;
			draw_vertices[i].y = vertices[i].y;
			draw_vertices[i].z = vertices[i].z;
		} else {
			draw_vertices[i].x =
				vertices[i].x * (PSP_FRAME_WIDTH / 640.0f);
			draw_vertices[i].y =
				vertices[i].y * (PSP_FRAME_HEIGHT / 480.0f);
			draw_vertices[i].z = vertices[i].z;
		}
	}

	sceKernelDcacheWritebackInvalidateRange(draw_vertices,
		count * sizeof(*draw_vertices));
	sceKernelDcacheWritebackInvalidateRange((void *)texture,
		width * height * sizeof(*texture));
	sceGuTexMode(GU_PSM_5551, 0, 0, GU_FALSE);
	sceGuTexImage(0, width, height, width, texture);
	sceGuTexScale(1.0f, 1.0f);
	sceGuTexOffset(0.0f, 0.0f);
	sceGuTexWrap(wrap_s, wrap_t);
	sceGuTexFilter(filter == PVR_FILTER_BILINEAR ? GU_LINEAR : GU_NEAREST,
		filter == PVR_FILTER_BILINEAR ? GU_LINEAR : GU_NEAREST);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuEnable(GU_TEXTURE_2D);
	if (alpha_test) {
		sceGuAlphaFunc(GU_GREATER, 0, 0xff);
		sceGuEnable(GU_ALPHA_TEST);
	} else {
		sceGuDisable(GU_ALPHA_TEST);
	}
	if (translucent) {
		sceGuEnable(GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	} else {
		sceGuDisable(GU_BLEND);
	}
	if (transform_3d) {
		sceGuEnable(GU_DEPTH_TEST);
		sceGuDepthMask(translucent ? GU_TRUE : GU_FALSE);
	} else {
		sceGuDisable(GU_DEPTH_TEST);
		sceGuDepthMask(GU_TRUE);
	}
	sceGuDrawArray(triangle_strip ? GU_TRIANGLE_STRIP : GU_TRIANGLE_FAN,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF |
		(transform_3d ? GU_TRANSFORM_3D : GU_TRANSFORM_2D),
		count, NULL, draw_vertices);
	sceGuTexFlush();
}

void PSP_GUDraw5551(const uint16_t *texture, int width, int height,
		const pvr_vertex_t *vertices, int count)
{
	PSP_GUDraw5551Internal(texture, width, height, vertices, count, 0, 1,
		1, PVR_FILTER_NONE, 0, GU_CLAMP, GU_CLAMP, 1);
}

static int PSP_VertexAngleLess(const pvr_vertex_t *a, const pvr_vertex_t *b,
		float center_x, float center_y)
{
	float az = a->z > 0.0f ? a->z : 1.0f;
	float bz = b->z > 0.0f ? b->z : 1.0f;
	float ax = a->x / az - center_x;
	float ay = a->y / az - center_y;
	float bx = b->x / bz - center_x;
	float by = b->y / bz - center_y;
	int a_half = ay < 0.0f || (ay == 0.0f && ax < 0.0f);
	int b_half = by < 0.0f || (by == 0.0f && bx < 0.0f);
	float cross;

	if (a_half != b_half)
		return a_half < b_half;

	cross = ax * by - ay * bx;
	if (cross != 0.0f)
		return cross > 0.0f;

	return ax * ax + ay * ay < bx * bx + by * by;
}

static const pvr_vertex_t *PSP_OrderWorldVertices(
		const pvr_vertex_t *vertices, int count, pvr_vertex_t ordered[5])
{
	float center_x = 0.0f;
	float center_y = 0.0f;

	if (count < 4 || count > 5)
		return vertices;

	for (int i = 0; i < count; i++) {
		float z = vertices[i].z > 0.0f ? vertices[i].z : 1.0f;

		ordered[i] = vertices[i];
		center_x += vertices[i].x / z;
		center_y += vertices[i].y / z;
	}
	center_x /= count;
	center_y /= count;

	for (int i = 1; i < count; i++) {
		pvr_vertex_t vertex = ordered[i];
		int j = i;

		while (j > 0 && PSP_VertexAngleLess(&vertex, &ordered[j - 1],
				center_x, center_y)) {
			ordered[j] = ordered[j - 1];
			j--;
		}
		ordered[j] = vertex;
	}

	return ordered;
}

void PSP_GUDrawWorldPoly(const pvr_poly_hdr_t *header,
		const pvr_vertex_t *vertices, int count)
{
	const pvr_poly_cxt_t *context;
	pvr_vertex_t ordered_vertices[5];
	int format;

	if (!header || !vertices)
		return;

	context = &header->context;
	vertices = PSP_OrderWorldVertices(vertices, count, ordered_vertices);
	int translucent = context->list == PVR_LIST_TR_POLY;
	int alpha_test = context->list != PVR_LIST_OP_POLY;
	if (!context->txr.base) {
		PSP_GUDrawFlatInternal(vertices, count, 1, translucent, 0);
		return;
	}

	format = context->txr.format;
	if (format & PVR_TXRFMT_PAL8BPP) {
		PSP_GUDrawIndexedInternal(context->txr.base,
			context->txr.width, context->txr.height,
			context->txr.palette, 			vertices, count, 1, translucent,
			alpha_test, context->txr.filter, 0,
			context->txr.wrap_s, context->txr.wrap_t, 0);
	} else {
		PSP_GUDraw5551Internal(context->txr.base, context->txr.width,
			context->txr.height, vertices, count, 1, translucent,
			alpha_test, context->txr.filter, 0,
			context->txr.wrap_s, context->txr.wrap_t, 0);
	}
}

void PSP_GUDrawOverlayPoly(const pvr_poly_hdr_t *header,
		const pvr_vertex_t *vertices, int count)
{
	const pvr_poly_cxt_t *context;
	int translucent;

	if (!header || !vertices)
		return;

	context = &header->context;
	translucent = context->list == PVR_LIST_TR_POLY;
	if (!context->txr.base) {
		PSP_GUDrawFlatInternal(vertices, count, 0, translucent, 1);
		return;
	}

	if (context->txr.format & PVR_TXRFMT_PAL8BPP) {
		PSP_GUDrawIndexedInternal(context->txr.base, context->txr.width,
			context->txr.height, context->txr.palette, vertices, count,
			0, translucent, 1, context->txr.filter, 1,
			context->txr.wrap_s, context->txr.wrap_t, 1);
	} else {
		PSP_GUDraw5551Internal(context->txr.base, context->txr.width,
			context->txr.height, vertices, count, 0, translucent, 1,
			context->txr.filter, 1,
			context->txr.wrap_s, context->txr.wrap_t, 1);
	}
}

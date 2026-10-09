#ifndef DOOM64_PSP_PLATFORM_H
#define DOOM64_PSP_PLATFORM_H

#include <stdbool.h>
#include <math.h>
#include <malloc.h>
#include <stdio.h>
#include <stdint.h>

#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspiofilemgr.h>
#include <pspgu.h>
#include <pspkernel.h>
#include "matrix.h"

#include <stddef.h>
#include <sys/types.h>

#define fsqrt sqrtf
#define frsqrt(value) (1.0f / sqrtf(value))
#define dbgio_printf(...) ((void)0)
#define dbgio_dev_select(device) ((void)(device))
#define rtc_unix_secs() ((int)(PSP_TimeMicroseconds() / 1000000))
#define FILEHND_INVALID (-1)
#define O_RDONLY PSP_O_RDONLY
#define fs_load PSP_LoadFile
#define fs_open PSP_OpenFile
#define fs_read PSP_ReadFile
#define fs_close sceIoClose
#define fs_total PSP_FileSize
#define fs_seek sceIoLseek

typedef int sfxhnd_t;
typedef uint8_t uint8;
typedef SceUID file_t;
typedef void *pvr_ptr_t;

typedef struct {
	float x, y, z, w;
} vector_t;

typedef struct {
	uint32_t flags;
	float x, y, z;
	uint32_t argb, oargb;
	float u, v;
} pvr_vertex_t;

typedef struct {
	struct { int src_enable, dst_enable, src, dst; } blend;
	struct { int specular, fog_type, fog_type2; } gen;
	struct {
		int env, filter, uv_flip, format, palette;
		pvr_ptr_t base;
		int width, height, wrap_s, wrap_t;
	} txr;
	struct { int write; } depth;
	int list;
} pvr_poly_cxt_t;

typedef struct { pvr_poly_cxt_t context; } pvr_poly_hdr_t;
typedef pvr_poly_hdr_t pvr_sprite_hdr_t;
typedef pvr_poly_cxt_t pvr_sprite_cxt_t;
typedef struct { uint32_t flags; float u, v; } pvr_sprite_txr_t;
typedef struct { uint32_t unused; } pvr_dr_state_t;
typedef struct { uint32_t unused; } pvr_init_params_t;
typedef struct { char name[256]; uint32_t size; } dirent_t;

void PSP_GUDrawFlat(const pvr_vertex_t *vertices, int count);
void PSP_GUDrawWorldPoly(const pvr_poly_hdr_t *header,
	const pvr_vertex_t *vertices, int count);
void PSP_GUDrawOverlayPoly(const pvr_poly_hdr_t *header,
	const pvr_vertex_t *vertices, int count);
void PSP_GUDraw5551(const uint16_t *texture, int width, int height,
	const pvr_vertex_t *vertices, int count);
void PSP_GUSetPaletteEntry(int index, uint16_t color);
void PSP_GUDrawIndexed(const uint8_t *texture, int width, int height,
	int palette, const pvr_vertex_t *vertices, int count);
void *PSP_GUAllocTexture(size_t size);
void PSP_GUFreeTexture(void *texture);
void PSP_GUFlushMirroredTextures(void);
void PSP_GULoadTexture(const void *source, void *texture, size_t size);
int PSP_GUUntwiddle8(const uint8_t *source, uint8_t *texture,
	int width, int height);
void PSP_GUTextureContext(pvr_poly_cxt_t *context, int list, int format,
	int width, int height, pvr_ptr_t texture, int filter);
void PSP_GUFlatContext(pvr_poly_cxt_t *context, int list);
void PSP_GUCompileTextureHeader(pvr_poly_hdr_t *header,
	const pvr_poly_cxt_t *context);
ssize_t PSP_LoadFile(const char *path, void **buffer);
file_t PSP_OpenFile(const char *path, int flags);
ssize_t PSP_ReadFile(file_t file, void *buffer, size_t size);
ssize_t PSP_FileSize(file_t file);

#define PVR_LIST_OP_POLY 0
#define PVR_LIST_TR_POLY 1
#define PVR_LIST_PT_POLY 2
#define PVR_CMD_VERTEX 0
#define PVR_CMD_VERTEX_EOL 1
#define PVR_TXRFMT_ARGB1555 0x01
#define PVR_TXRFMT_ARGB4444 0x02
#define PVR_TXRFMT_RGB565 0x03
#define PVR_TXRFMT_BUMP 0x04
#define PVR_TXRFMT_TWIDDLED 0x10
#define PVR_TXRFMT_NONTWIDDLED 0x20
#define PVR_TXRFMT_PAL8BPP 0x40
#define PVR_TXRFMT_8BPP_PAL(n) ((n) << 8)
#define PVR_BLEND_ZERO 0
#define PVR_BLEND_ONE 1
#define PVR_BLEND_SRCALPHA 2
#define PVR_BLEND_INVSRCALPHA 3
#define PVR_BLEND_DESTCOLOR 4
#define PVR_FILTER_NONE 0
#define PVR_FILTER_BILINEAR 1
#define PVR_DEPTHWRITE_DISABLE 0
#define PVR_FOG_VERTEX 0
#define PVR_FOG_TABLE 1
#define PVR_SPECULAR_ENABLE 1
#define PVR_TXRENV_DECAL 0
#define PVR_UVFLIP_NONE 0
#define PSP_GU_UV_MIRROR_U 0x01
#define PSP_GU_UV_MIRROR_V 0x02
#define PSP_GU_UV_WALL 0x04
#define PVR_TXRLOAD_8BPP 0
#define PVR_TXRLOAD_16BPP 1
#define PVR_MIN_Z 0.0001f

int PSP_GUInit(void);
void PSP_GUShutdown(void);
void PSP_GUBeginFrame(uint32_t clear_color);
void PSP_GUEndFrame(void);
int PSP_ReadController(SceCtrlData *state);
uint64_t PSP_TimeMicroseconds(void);

#endif

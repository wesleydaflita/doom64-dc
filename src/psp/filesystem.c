#include "platform.h"

#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"

int32_t Pak_Memory = 0;
int32_t Pak_Size = 0;
uint8_t *Pak_Data = NULL;
dirent_t __attribute__((aligned(32))) FileState[200];
int32_t FilesUsed = -1;
int32_t ControllerPakStatus = 1;

static int PSP_CountPakSlots(void)
{
	int count = 0;

	if (!Pak_Data || Pak_Size < 512)
		return 0;

	for (int i = 0; i < 16; i++)
		if (Pak_Data[i * 32] != 0)
			count++;

	return count;
}

static int PSP_EnsureSaveDir(void)
{
	static const char *directories[] = {
		"ms0:/PSP",
		"ms0:/PSP/GAME",
		"ms0:/PSP/GAME/DOOM64"
	};

	for (size_t i = 0; i < sizeof(directories) / sizeof(directories[0]); i++) {
		if (sceIoMkdir(directories[i], 0777) < 0) {
			SceUID directory = sceIoDopen(directories[i]);
			if (directory < 0)
				return -1;
			sceIoDclose(directory);
		}
	}

	return 0;
}

static int PSP_StoreBinaryFile(const char *path, const void *data, size_t length)
{
	SceUID file;
	const uint8_t *bytes = (const uint8_t *)data;
	size_t total = 0;

	if (!path || !data || !length)
		return -1;

	file = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (file < 0)
		return -1;

	while (total < length) {
		ssize_t written = sceIoWrite(file, bytes + total, length - total);
		if (written <= 0) {
			sceIoClose(file);
			return -1;
		}
		total += (size_t)written;
	}

	sceIoClose(file);
	return 0;
}

static int PSP_LoadBinaryFile(const char *path, void **buffer, size_t *length_out)
{
	SceUID file;
	SceOff end;
	SceOff pos;
	void *data;
	SceSize bytes_read;

	if (!path || !buffer || !length_out)
		return -1;

	*buffer = NULL;
	*length_out = 0;

	file = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (file < 0)
		return -1;

	end = sceIoLseek(file, 0, PSP_SEEK_END);
	if (end < 0) {
		sceIoClose(file);
		return -1;
	}
	pos = sceIoLseek(file, 0, PSP_SEEK_SET);
	if (pos < 0) {
		sceIoClose(file);
		return -1;
	}

	data = calloc(1, (size_t)end == 0 ? 1 : (size_t)end);
	if (!data) {
		sceIoClose(file);
		return -1;
	}

	bytes_read = sceIoRead(file, data, (SceSize)(size_t)end);
	sceIoClose(file);
	if (bytes_read != end) {
		free(data);
		return -1;
	}

	*buffer = data;
	*length_out = (size_t)end;
	return 0;
}

file_t PSP_OpenFile(const char *path, int flags)
{
	SceUID file;

	if (!path)
		return FILEHND_INVALID;

	file = sceIoOpen(path, flags, 0777);
	return file < 0 ? FILEHND_INVALID : file;
}

ssize_t PSP_ReadFile(file_t file, void *buffer, size_t size)
{
	ssize_t bytes_read = sceIoRead(file, buffer, (SceSize)size);
	return bytes_read < 0 ? -1 : bytes_read;
}

ssize_t PSP_FileSize(file_t file)
{
	SceOff current = sceIoLseek(file, 0, PSP_SEEK_CUR);
	SceOff end = sceIoLseek(file, 0, PSP_SEEK_END);
	if (current >= 0)
		sceIoLseek(file, current, PSP_SEEK_SET);
	return (ssize_t)end;
}

ssize_t PSP_LoadFile(const char *path, void **buffer)
{
	SceUID file;
	SceOff length;
	void *data;
	SceSize bytes_read;

	if (!path || !buffer)
		return -1;

	*buffer = NULL;
	file = sceIoOpen(path, PSP_O_RDONLY, 0);
	if (file < 0)
		return -1;

	length = sceIoLseek(file, 0, PSP_SEEK_END);
	if (length <= 0 || sceIoLseek(file, 0, PSP_SEEK_SET) < 0) {
		sceIoClose(file);
		return -1;
	}

	data = malloc((size_t)length);
	if (!data) {
		sceIoClose(file);
		return -1;
	}

	bytes_read = sceIoRead(file, data, (SceSize)length);
	sceIoClose(file);
	if (bytes_read != (SceSize)length) {
		free(data);
		return -1;
	}

	*buffer = data;
	return (ssize_t)length;
}

static int PSP_SavePath(char *buffer, size_t buffer_size, const char *name)
{
	if (!buffer || !name)
		return -1;
	return snprintf(buffer, buffer_size, "%s/%s", STORAGE_PREFIX, name) >= 0 ? 0 : -1;
}

void I_VMUUpdateFace(uint8_t *image, int force_refresh)
{
	(void)image;
	(void)force_refresh;
}

void I_VMUFB(int force_refresh)
{
	(void)force_refresh;
}

int I_CheckControllerPak(void)
{
	char path[256];
	SceUID fd;

	if (PSP_EnsureSaveDir() != 0)
		return PFS_ERR_NOPACK;

	if (PSP_SavePath(path, sizeof(path), "doom64") != 0)
		return PFS_ERR_NOPACK;

	fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd >= 0) {
		sceIoClose(fd);
		return 0;
	}

	return 0;
}

int I_DeletePakFile(dirent_t *de)
{
	char path[256];
	(void)de;

	if (PSP_EnsureSaveDir() != 0)
		return PFS_ERR_ID_FATAL;
	if (PSP_SavePath(path, sizeof(path), "doom64") != 0)
		return PFS_ERR_ID_FATAL;
	if (Pak_Data) {
		Z_Free(Pak_Data);
		Pak_Data = NULL;
	}
	Pak_Size = 0;
	FilesUsed = 0;
	if (sceIoRemove(path) < 0)
		return PFS_ERR_ID_FATAL;
	return 0;
}

int I_CreatePakFile(void)
{
	char path[256];

	if (PSP_EnsureSaveDir() != 0)
		return PFS_ERR_ID_FATAL;
	if (PSP_SavePath(path, sizeof(path), "doom64") != 0)
		return PFS_ERR_ID_FATAL;

	if (Pak_Data)
		Z_Free(Pak_Data);
	Pak_Data = Z_Malloc(512, PU_STATIC, NULL);
	Pak_Size = 512;
	if (!Pak_Data) {
		FilesUsed = -1;
		return PFS_ERR_ID_FATAL;
	}

	memset(Pak_Data, 0, 512);
	if (PSP_StoreBinaryFile(path, Pak_Data, 512) != 0) {
		Z_Free(Pak_Data);
		Pak_Data = NULL;
		Pak_Size = 0;
		FilesUsed = -1;
		return PFS_ERR_ID_FATAL;
	}

	FilesUsed = 0;
	Pak_Memory = 200;
	ControllerPakStatus = 1;
	return 0;
}

int I_SavePakFile(void)
{
	char path[256];

	if (!Pak_Data || Pak_Size <= 0)
		return PFS_ERR_ID_FATAL;
	if (PSP_EnsureSaveDir() != 0)
		return PFS_ERR_ID_FATAL;
	if (PSP_SavePath(path, sizeof(path), "doom64") != 0)
		return PFS_ERR_ID_FATAL;
	if (PSP_StoreBinaryFile(path, Pak_Data, (size_t)Pak_Size) != 0)
		return PFS_ERR_ID_FATAL;
	FilesUsed = PSP_CountPakSlots();
	return 0;
}

int I_ReadPakFile(void)
{
	char path[256];
	void *data = NULL;
	size_t length = 0;
	int fd;

	if (Pak_Data) {
		Z_Free(Pak_Data);
		Pak_Data = NULL;
	}
	Pak_Size = 0;
	FilesUsed = -1;

	if (PSP_EnsureSaveDir() != 0)
		return PFS_ERR_ID_FATAL;
	if (PSP_SavePath(path, sizeof(path), "doom64") != 0)
		return PFS_ERR_ID_FATAL;

	fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return I_CreatePakFile();
	sceIoClose(fd);

	if (PSP_LoadBinaryFile(path, &data, &length) != 0)
		return PFS_ERR_ID_FATAL;
	if (length > 512) {
		free(data);
		return PFS_ERR_ID_FATAL;
	}

	Pak_Data = Z_Malloc(512, PU_STATIC, NULL);
	if (!Pak_Data) {
		free(data);
		return PFS_ERR_ID_FATAL;
	}
	memset(Pak_Data, 0, 512);
	memcpy(Pak_Data, data, length);
	free(data);
	Pak_Size = 512;
	FilesUsed = PSP_CountPakSlots();
	Pak_Memory = 200;
	ControllerPakStatus = 1;
	return 0;
}

int I_SavePakSettings(doom64_settings_t *msettings)
{
	char path[256];

	if (!msettings)
		return PFS_ERR_ID_FATAL;
	if (PSP_EnsureSaveDir() != 0)
		return PFS_ERR_ID_FATAL;
	if (PSP_SavePath(path, sizeof(path), "doom64stg") != 0)
		return PFS_ERR_ID_FATAL;
	if (PSP_StoreBinaryFile(path, msettings, sizeof(*msettings)) != 0)
		return PFS_ERR_ID_FATAL;
	return 0;
}

int I_ReadPakSettings(doom64_settings_t *msettings)
{
	char path[256];
	void *data = NULL;
	size_t length = 0;
	int fd;

	if (!msettings)
		return PFS_ERR_ID_FATAL;
	if (PSP_EnsureSaveDir() != 0)
		return PFS_ERR_ID_FATAL;
	if (PSP_SavePath(path, sizeof(path), "doom64stg") != 0)
		return PFS_ERR_ID_FATAL;

	fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return PFS_ERR_ID_FATAL;
	sceIoClose(fd);

	if (PSP_LoadBinaryFile(path, &data, &length) != 0)
		return PFS_ERR_ID_FATAL;
	if (length < sizeof(*msettings)) {
		free(data);
		return PFS_ERR_ID_FATAL;
	}

	memcpy(msettings, data, sizeof(*msettings));
	free(data);
	msettings->runintroduction = false;
	global_render_state.quality = msettings->Quality;
	global_render_state.fps_uncap = msettings->FpsUncap;
	return 0;
}

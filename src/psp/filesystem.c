#include "platform.h"

#include <pspiofilemgr.h>
#include <stdlib.h>

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

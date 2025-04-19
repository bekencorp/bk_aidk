#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// #include "booka.h"
#include "log.h"
#include "file_writer.h"

typedef struct {
	char base_name[128];
	FILE *file;
} file_writer_t;

void *create_file_writer(const char *base_name)
{
	file_writer_t *file_writer = (file_writer_t *)malloc(sizeof(file_writer_t));
	if (!file_writer) {
		return NULL;
	}

	memset(file_writer, 0, sizeof(file_writer_t));
	strncpy(file_writer->base_name, base_name, sizeof(file_writer->base_name));
	return file_writer;
}

int write_file(void *file_writer, uint8_t data_type, const void *data, size_t size)
{
	return 0;
	// if (!file_writer) {
	// 	return -1;
	// }
	// file_writer_t *fw = (file_writer_t *)file_writer;

	// // if file is not created yet, creat it
	// if (!fw->file) {
	// 	char file_name[128];
	// 	char file_suffix[6];
	// 	strncpy(file_name, fw->base_name, sizeof(file_name));

	// 	switch (data_type) {
	// 	case VIDEO_DATA_TYPE_H264:
	// 		strncpy(file_suffix, ".h264", sizeof(file_suffix));
	// 		break;
	// 	// case VIDEO_DATA_TYPE_GENERIC_JPEG:
	// 	// 	strncpy(file_suffix, ".mjpg", sizeof(file_suffix));
	// 	// 	break;
	// 	// case VIDEO_DATA_TYPE_GENERIC:
	// 	// 	strncpy(file_suffix, ".bin", sizeof(file_suffix));
	// 	// 	break;
	// 	case AUDIO_DATA_TYPE_OPUS:
	// 		strncpy(file_suffix, ".opus", sizeof(file_suffix));
	// 		break;
	// 	case AUDIO_DATA_TYPE_AACLC:
	// 	// case AUDIO_DATA_TYPE_HEAAC:
	// 		strncpy(file_suffix, ".aac", sizeof(file_suffix));
	// 		break;
	// 	// case AUDIO_DATA_TYPE_PCM:
	// 	// 	strncpy(file_suffix, ".pcm", sizeof(file_suffix));
	// 	// 	break;
	// 	default:
	// 		strncpy(file_suffix, ".bin", sizeof(file_suffix));
	// 		break;
	// 	}

	// 	strcat(file_name, file_suffix);
	// 	if ((fw->file = fopen(file_name, "w")) == NULL) {
	// 		LOGE("Failed to create file \"%s\"", file_name);
	// 		return -1;
	// 	}
	// 	LOGI("Create file \"%s\" successfully", file_name);
	// }

	// return fwrite(data, 1, size, fw->file);
}

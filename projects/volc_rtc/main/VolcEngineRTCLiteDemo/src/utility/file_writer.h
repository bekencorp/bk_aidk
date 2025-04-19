
void *create_file_writer(const char *base_name);
int write_file(void *file_writer, uint8_t codec_type, const void *data, size_t size);
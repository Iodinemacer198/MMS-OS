#ifndef FS_H
#define FS_H

#include <stdbool.h>
#include <stdint.h>

void vfs_init();
bool vfs_write_file(const char* path, const char* data);
bool vfs_read_file(const char* path, char* buffer_out);
void vfs_list_files();
bool vfs_delete_file(const char* path);

#endif
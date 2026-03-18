#include "fs.h"
#include "ata.h"

extern int strlen(const char* str);
extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern bool strcmp(const char* a, const char* b);

int strlen(const char* str) {
    int len = 0;
    while(str[len]) len++;
    return len;
}

void strcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

#define VFS_MAX_FILES 10
#define SECTOR_SIZE 512
#define FS_MAGIC 0x4D4D5346 

typedef struct {
    char path[32];
    uint32_t lba;    
    uint32_t size;    
    bool exists;
} FileEntry;

typedef struct {
    uint32_t magic; 
    FileEntry files[VFS_MAX_FILES];
} DirectoryTable;

DirectoryTable current_dir;
uint8_t io_buffer[SECTOR_SIZE];

void vfs_init() {
    ata_read_sector(1, (uint8_t*)&current_dir);

    if (current_dir.magic != FS_MAGIC) {
        println("Formatting blank drive...");
        current_dir.magic = FS_MAGIC;
        for (int i = 0; i < VFS_MAX_FILES; i++) {
            current_dir.files[i].exists = false;
        }
        ata_write_sector(1, (uint8_t*)&current_dir);
        
        vfs_write_file("0:\\test.txt", "Hello, curious user!");
    } else {
        println("Disk mounted successfully.");
    }
}

bool vfs_write_file(const char* path, const char* data) {
    int file_len = strlen(data);
    if (file_len > SECTOR_SIZE) return false;

    int target_idx = -1;

    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (current_dir.files[i].exists && strcmp(current_dir.files[i].path, path)) {
            target_idx = i;
            break;
        }
    }

    if (target_idx == -1) {
        for (int i = 0; i < VFS_MAX_FILES; i++) {
            if (!current_dir.files[i].exists) {
                target_idx = i;
                strcpy(current_dir.files[target_idx].path, path);
                current_dir.files[target_idx].lba = 2 + i; 
                current_dir.files[target_idx].exists = true;
                break;
            }
        }
    }

    if (target_idx == -1) return false; 

    current_dir.files[target_idx].size = file_len;

    for(int i = 0; i < SECTOR_SIZE; i++) io_buffer[i] = 0;
    strcpy((char*)io_buffer, data);
    ata_write_sector(current_dir.files[target_idx].lba, io_buffer);

    ata_write_sector(1, (uint8_t*)&current_dir);
    return true;
}

bool vfs_read_file(const char* path, char* buffer_out) {
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (current_dir.files[i].exists && strcmp(current_dir.files[i].path, path)) {
            ata_read_sector(current_dir.files[i].lba, (uint8_t*)buffer_out);
            buffer_out[current_dir.files[i].size] = '\0';
            return true;
        }
    }
    return false;
}

void vfs_list_files() {
    println("------ User Disk 0:\\ ------");
    bool empty = true;
    for(int i = 0; i < VFS_MAX_FILES; i++) {
        if(current_dir.files[i].exists) {
            if (strcmp(current_dir.files[i].path, "0:\\password.ini") || strcmp(current_dir.files[i].path, "0:\\username.ini")) {}
            else {
                print(current_dir.files[i].path);
                print(" (");
                printint(current_dir.files[i].size);
                println(" bytes)");
                empty = false;
            }
        }
    }
    if(empty) println("No files found.");
    println("---------------------------");
}

bool vfs_delete_file(const char* path) {
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (current_dir.files[i].exists && strcmp(current_dir.files[i].path, path)) {
            current_dir.files[i].exists = false;
            current_dir.files[i].size = 0;
            for (int j = 0; j < 32; j++) {
                current_dir.files[i].path[j] = '\0';
            }
            ata_write_sector(1, (uint8_t*)&current_dir);
            return true; 
        }
    }
    return false; 
}
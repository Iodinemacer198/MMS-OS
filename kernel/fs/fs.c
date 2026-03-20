#include "fs.h"
#include "ata.h"

extern int strlen(const char* str);
extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern bool strcmp(const char* a, const char* b);
extern void reboot();
extern void putchar(char c);
extern char get_key();
extern void sleep();

extern int cursorX;

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

#define VFS_MAX_FILES 12
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

static const char* default_demo_source =
    "int main() {\n"
    "println(\"Hello from Tiny C!\");\n"
    "int answer = 2 + 3 * 4;\n"
    "printint(answer);\n"
    "println(\"\");\n"
    "return answer;\n"
    "}";

static void vfs_seed_defaults() {
    char read_buffer[SECTOR_SIZE + 1];

    if (!vfs_read_file("0:\\test.txt", read_buffer)) {
        vfs_write_file("0:\\test.txt", "Hello, curious user!");
    }
    if (!vfs_read_file("0:\\ode.md", read_buffer)) {
        vfs_write_file("0:\\ode.md", "370 400\n370 400\n392 400\n440 400\n440 400\n392 400\n370 400\n330 400\n294 400\n294 400\n330 400\n370 400\n370 600\n330 200\n330 800");
    }
    if (!vfs_read_file("0:\\demo.c", read_buffer) || !strcmp(read_buffer, default_demo_source)) {
        vfs_write_file("0:\\demo.c", default_demo_source);
    }
}

void vfs_init() {
    ata_read_sector(1, (uint8_t*)&current_dir);

    if (current_dir.magic != FS_MAGIC) {
        println("Formatting blank drive...");
        current_dir.magic = FS_MAGIC;
        for (int i = 0; i < VFS_MAX_FILES; i++) {
            current_dir.files[i].exists = false;
        }
        ata_write_sector(1, (uint8_t*)&current_dir);
    } else {
        println("Disk mounted successfully.");
    }

    vfs_seed_defaults();
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

bool vfs_read_file_line(const char* path, char* line_out) {
    static char file_buffer[512];
    static int index = 0;
    static bool loaded = false;

    if (!loaded) {
        for (int i = 0; i < VFS_MAX_FILES; i++) {
            if (current_dir.files[i].exists && strcmp(current_dir.files[i].path, path)) {
                ata_read_sector(current_dir.files[i].lba, (uint8_t*)file_buffer);
                file_buffer[current_dir.files[i].size] = '\0';
                index = 0;
                loaded = true;
                break;
            }
        }

        if (!loaded) return false; 
    }

    if (file_buffer[index] == '\0') {
        loaded = false; 
        return false;
    }

    int i = 0;
    while (file_buffer[index] != '\n' && file_buffer[index] != '\0') {
        line_out[i++] = file_buffer[index++];
    }

    if (file_buffer[index] == '\n') {
        index++;
    }

    line_out[i] = '\0';
    return true;
}

int vfs_file_count() {
    int count = 0;

    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (current_dir.files[i].exists) {
            count++;
        }
    }

    return count;
}

void vfs_reset() {
    print("Are you sure you would like to reset this system? (y/n): ");
    char ans[64] = "";
    int ans_index = 0;
    bool running = true;
    while (running) {
        char key = get_key();

        if (!key) {
            continue;
        }
        if (key == '\n') {
            running = false;
        }
        else if (key == 8) {
            if (ans_index > 0) {
                ans_index--;
                ans[ans_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if ((key == 'y' || key == 'n') && ans_index <= 1) {
            putchar(key);
            ans[ans_index] = key;
            ans_index++;
        }
        else {
            continue;
        }
    }
    if (strcmp(ans, "y")) {
        for (int i = 0; i < VFS_MAX_FILES; i++) {
            if (current_dir.files[i].exists) {
                vfs_delete_file(current_dir.files[i].path);
            }
        }
        vfs_seed_defaults();
        putchar('\n');
        println("System reset. Rebooting...");
        sleep(20000);
        reboot();
    }
    else println("");
}

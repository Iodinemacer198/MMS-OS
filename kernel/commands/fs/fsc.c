#include <stdint.h>
#include <stdbool.h>
#include "fsc.h"

extern void print(const char* str);
extern void println(const char* str);
extern bool vfs_write_file(const char* path, const char* data);
extern bool vfs_read_file(const char* path, char* buffer_out);
extern void path_prepend(char* path);
extern void putchar(char c);
extern char get_key();
extern bool vfs_delete_file(const char* path);
extern int vfs_file_count();
extern bool vfs_make_dir(const char* path);
extern bool vfs_remove_dir(const char* path);
extern bool vfs_change_dir(const char* path);
extern void vfs_get_cwd(char* out);
extern void vfs_list_current_dir();

extern int cursorX;

static bool contains_sep(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\\' || str[i] == '/') return true;
    }
    return false;
}

void ls() {
    vfs_list_current_dir();
}

void pwd() {
    char cwd[64];
    vfs_get_cwd(cwd);
    println(cwd);
}

void cd(const char* path) {
    if (vfs_change_dir(path)) {
    } else {
        println("Could not change directory.");
    }
}

void mkdir_cmd(char* name) {
    if (contains_sep(name)) {
        println("Use a single directory name (no path separators).");
        return;
    }

    path_prepend(name);
    if (vfs_make_dir(name)) {
        println("Directory created.");
    } else {
        println("Could not create directory.");
    }
}

void rmdir_cmd(char* name) {
    if (contains_sep(name)) {
        println("Use a single directory name (no path separators).");
        return;
    }

    path_prepend(name);
    if (vfs_remove_dir(name)) {
        println("Directory removed.");
    } else {
        println("Directory not empty, not found, or protected.");
    }
}

void mkf(char* path2) {
    int fileCount = vfs_file_count();
    if (fileCount >= 56) {
        println("Directory table is full! Use 'rmf' to delete some files!");
        return;
    }

    if (contains_sep(path2)) {
        println("Use a single file name (no path separators).");
        return;
    }

    path_prepend(path2);

    char temp[512] = "";
    if (vfs_read_file(path2, temp)) {
        println("A file with this name already exists!");
        return;
    }

    if (!vfs_write_file(path2, "")) {
        println("Error: Could not create file.");
        return;
    }
    println("Write contents (type :s and press enter to save): ");

    char data[512] = "";
    int idx = 0;
    bool running = true;
    while (running) {
        char key = get_key();
        if (!key) continue;

        if (key == '\n' && idx >= 2 && data[idx - 1] == 's' && data[idx - 2] == ':') {
            data[idx - 2] = '\0';
            running = false;
        }
        else if (key == 8) {
            if (idx > 0) {
                idx--;
                data[idx] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if (idx < 511) {
            putchar(key);
            data[idx++] = key;
            data[idx] = '\0';
        }
    }

    vfs_write_file(path2, data);
    putchar('\n');
    println("File saved.");
}

void read(char* path) {
    if (contains_sep(path)) {
        println("Use a single file name (no path separators).");
        return;
    }

    path_prepend(path);

    char read_buffer[8092];
    if (vfs_read_file(path, read_buffer)) {
        println(read_buffer);
    }
    else {
        print("Error: ");
        print(path);
        println(" not found.");
    }
}

void rmf(char* path2) {
    if (contains_sep(path2)) {
        println("Use a single file name (no path separators).");
        return;
    }

    path_prepend(path2);

    if (!vfs_delete_file(path2)) {
        println("File not found.");
    } else {
        println("File deleted.");
    }
}

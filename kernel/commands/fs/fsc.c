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
extern void dprintc(const char* str, uint8_t color);
extern void dputcharc(unsigned char c, uint8_t color);

extern int cursorX;

extern int dcX;
extern int dcY;

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

void vgag_mkf() {
    int fileCount = vfs_file_count();
    dcX = 41; dcY = 4;
    if (fileCount >= 56) {
        dprintc("Directory table is full! Use 'rmf' to delete some files!", 0x70);
        return;
    }

    dprintc("Please enter the new file name:", 0x70); dcY++; dcX = 41;

    char path2[512] = "";
    int idx2 = 0;
    bool running2 = true;
    while (running2) {
        char key = get_key();
        if (!key) continue;

        if (key == '\n') {
            running2 = false;
        }
        else if (key == 8) {
            if (idx2 > 0) {
                idx2--;
                path2[idx2] = '\0';
                dcX--;
                dputcharc(' ', 0x70);
                dcX--;
            }
        }
        else if (idx2 < 511) {
            dputcharc(key, 0x70);
            path2[idx2++] = key;
            path2[idx2] = '\0';
            if (dcX >= 72) {dcY++; dcX = 41;}
        }
    }

    dcX = 41; dcY += 2;

    if (contains_sep(path2)) {
        dprintc("Use a single file name", 0x70);
        return;
    }

    path_prepend(path2);

    char temp[512] = "";
    if (vfs_read_file(path2, temp)) {
        dprintc("A file with this name exists!", 0x70);
        return;
    }

    if (!vfs_write_file(path2, "")) {
        dprintc("Error: Could not create file.", 0x70);
        return;
    }
    dprintc("Write (:s + enter to save): ", 0x70); dcX = 41; dcY++;

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
                dcX--;
                dputcharc(' ', 0x70);
                dcX--;
            }
        }
        else if (idx < 511) {
            if (key == '\n') {
                dputcharc(key, 0x70);
                dcX = 41;
            }
            else {
                dputcharc(key, 0x70);
                data[idx++] = key;
                data[idx] = '\0';
                if (dcX >= 72) {dcY++; dcX = 41;}
            }
        }
    }

    vfs_write_file(path2, data); dcX = 41; dcY++;
    dprintc("File saved.", 0x70);
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

void vgag_print_wrapped(const char* text, uint8_t color) {
    int i = 0;
    int m = 0;

    while (text[i]) {

        if (text[i] == '\n') {
            dcY++;
            dcX = 41;
            i++;
            continue;
        }

        if (dcX >= 72) {
            dcY++;
            dcX = 41;
        }

        if (dcY >= 20) {
            dcX = 41; dcY = 21;
            m = 1;
            dprintc("...", 0x70);
        }

        if (m==0) dputcharc(text[i], color);

        i++;
    }
}

void vgag_read(char* path) {
    if (contains_sep(path)) {
        dprintc("Use a single file name (no path separators).", 0x70);
        return;
    }

    path_prepend(path);

    char read_buffer[8092];
    if (vfs_read_file(path, read_buffer)) {
        vgag_print_wrapped(read_buffer, 0x70);
        dcX = 41; dcY++;
    }
    else {
        dprintc("Error: ", 0x70);
        dprintc(path, 0x70);
        dprintc(" not found.", 0x70);
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

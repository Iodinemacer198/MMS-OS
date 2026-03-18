#include <stdint.h>
#include <stdbool.h>
#include "fsc.h"

extern int strlen(const char* str);
extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern bool strcmp(const char* a, const char* b);
extern bool vfs_write_file(const char* path, const char* data);
extern bool vfs_read_file(const char* path, char* buffer_out);
extern void path_prepend(char* path);
extern void putchar(char c);
extern char get_key();
extern bool vfs_delete_file(const char* path);

extern int cursorX;

void mkf() {
    print("File name: ");
    char path2[64] = "";
    int path2_index = 0;
    bool running2 = true;
    while (running2) {
        char key = get_key();

        if (!key) {
            continue;
        }
        if (key == '\n') {
            path_prepend(path2);
            running2 = false;
        }
        else if (key == 8) {
            if (path2_index > 0) {
                path2_index--;
                path2[path2_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else {
            putchar(key);
            path2[path2_index] = key;
            path2_index++;
        }
    }
    char temp[512] = "";
    if (vfs_read_file(path2, temp)) {
        println("A file with this name already exists!");
    } else {
        if (vfs_write_file(path2, "")) {
            putchar('\n');
            print(path2);
            println(" successfully created!");
        } else {
            println("Error: Could not create file.");
        }
    }
    char path3[64] = "";
    int path3_index = 0;
    print("Write contents: ");
    bool running3 = true;
    while (running3) {
        char key = get_key();

        if (!key) {
            continue;
        }
        if (key == '\n') {
            running3 = false;
        }
        else if (key == 8) {
            if (path3_index > 0) {
                path3_index--;
                path3[path3_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else {
            putchar(key);
            path3[path3_index] = key;
            path3_index++;
        }
    }
    vfs_write_file(path2, path3);
    putchar('\n');
    print("Contents successfully written to ");
    print(path2);
    putchar('\n');
}

void read() {
    print("File path: ");
    char path[64] = "";
    int path_index = 0;
    bool running = true;
    while (running) {
        char key = get_key();

        if (!key) {
            continue;
        }
        if (key == '\n') {
            path_prepend(path);
            running = false;
        }
        else if (key == 8) {
            if (path_index > 0) {
                path_index--;
                path[path_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else {
            putchar(key);
            path[path_index] = key;
            path_index++;
        }
    }
    char read_buffer[512];
    if (strcmp(path, "0:\\password.ini") || strcmp(path, "0:\\username.ini")) {
        putchar('\n');
        print("Error: ");
        print(path);
        print(" not found.");
        putchar('\n');
    }
    else if (vfs_read_file(path, read_buffer)) {
        putchar('\n');
        println(read_buffer);
    } 
    else {
        putchar('\n');
        print("Error: ");
        print(path);
        print(" not found.");
        putchar('\n');
    }
    for (int i = 0; i < 64; i++) {
        path[i] = 0;
    }
    path_index = 0;
}

void rmf() {
    print("File name: ");
    char path2[64] = "";
    int path2_index = 0;
    bool running2 = true;
    while (running2) {
        char key = get_key();

        if (!key) {
            continue;
        }
        if (key == '\n') {
            path_prepend(path2);
            running2 = false;
        }
        else if (key == 8) {
            if (path2_index > 0) {
                path2_index--;
                path2[path2_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else {
            putchar(key);
            path2[path2_index] = key;
            path2_index++;
        }
    }
    if (!vfs_delete_file(path2)) {
        putchar('\n');
        println("This file doesn't exit!");
    } else {
        putchar('\n');
        print("Successfully deleted ");
        print(path2);
        putchar('\n');
    }
}
#include <stdint.h>
#include "login.h"

extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern int atoi(const char* str);
extern char get_key();
extern bool isdigit(char c);
extern void putchar(char c);
extern void sleep(uint32_t count);
extern void reboot();
extern int memcmp(const void* s1, const void* s2, int n);
extern bool vfs_read_file(const char* path, char* buffer_out);
extern bool vfs_write_file(const char* path, const char* data);
extern bool strcmp(const char* a, const char* b);

extern int cursorX;
extern int cursorY;

#define USIN_BUFFER 20
char usin_buffer[USIN_BUFFER];
int usin_index = 0;

#define PSIN_BUFFER 20
char psin_buffer[PSIN_BUFFER];
int psin_index = 0;

char username_buffer[20];
char password_buffer[20];

void handle_login() {
    if (vfs_read_file("0:\\password.ini", password_buffer) && vfs_read_file("0:\\username.ini", username_buffer)) {
        print("Username > ");
        bool userrunning = true;
        while (userrunning) {
            char key = get_key();

            if (!key) {
                continue;
            }
            else if (key == '\n') {
                if (memcmp(username_buffer, usin_buffer, 20) == 0)
                {
                    userrunning = false;
                }
                else 
                {
                    putchar('\n');
                    println("Username incorrect! The system will restart.");
                    sleep(20000);
                    reboot();
                }
            }
            else if (key == 8) {
                if (usin_index > 0) {
                    usin_index--;
                    usin_buffer[usin_index] = '\0';
                    cursorX--;
                    putchar(' ');
                    cursorX--;
                }
            }
            else {
                if (usin_index <= 20) {
                    putchar(key);
                    usin_buffer[usin_index] = key;
                    usin_index++;
                }
                else {
                    continue;
                }
            }
        }
        putchar('\n');
        print("Password > ");
        bool passrunning = true;
        while (passrunning) {
            char key = get_key();

            if (!key) {
                continue;
            }
            else if (key == '\n') {
                if (strcmp(password_buffer, psin_buffer)) {
                    putchar('\n');
                    passrunning = false;
                }
                else {
                    putchar('\n');
                    println("Password incorrect! The system will restart.");
                    //println(password_buffer);
                    //println(psin_buffer);
                    sleep(20000);
                    reboot();
                }
            }
            else if (key == 8) {
                if (psin_index > 0) {
                    psin_index--;
                    psin_buffer[psin_index] = '\0';
                    cursorX--;
                    putchar(' ');
                    cursorX--;
                }
            }
            else if (key) {
                if (psin_index <= 20) {
                    putchar(key);
                    psin_buffer[psin_index] = key;
                    psin_index++;
                }
                else {
                    continue;
                }
            }
            else {
                continue;
            }
        }
    }
    else {
        print("New Username > ");
        bool userrunning = true;
        while (userrunning) {
            char key = get_key();

            if (!key) {
                continue;
            }
            else if (key == '\n') {
                vfs_write_file("0:\\username.ini", usin_buffer);
                userrunning = false;
            }
            else if (key == 8) {
                if (usin_index > 0) {
                    usin_index--;
                    usin_buffer[usin_index] = '\0';
                    cursorX--;
                    putchar(' ');
                    cursorX--;
                }
            }
            else {
                if (usin_index < 20) {
                    putchar(key);
                    usin_buffer[usin_index] = key;
                    usin_index++;
                }
                else {
                    continue;
                }
            }
        }
        putchar('\n');
        print("New Password > ");
        bool passrunning = true;
        while (passrunning) {
            char key = get_key();

            if (!key) {
                continue;
            }
            else if (key == '\n') {
                vfs_write_file("0:\\password.ini", psin_buffer);
                //println(psin_buffer);
                passrunning = false;
            }
            else if (key == 8) {
                if (psin_index > 0) {
                    psin_index--;
                    psin_buffer[psin_index] = '\0';
                    cursorX--;
                    putchar(' ');
                    cursorX--;
                }
            }
            else if (key) {
                if (psin_index < 20) {
                    putchar(key);
                    psin_buffer[psin_index] = key;
                    psin_index++;
                }
                else {
                    continue;
                }
            }
            else {
                continue;
            }
        }
        putchar('\n');
        println("Successfully registered!");
    }
}
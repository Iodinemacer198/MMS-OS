#include <stdint.h>
#include "vgag.h"

extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern int atoi(const char* str);
extern char get_key();
extern bool isdigit(char c);
extern void putchar(unsigned char c);
extern void sleep(uint32_t count);
extern void reboot();
extern int memcmp(const void* s1, const void* s2, int n);
extern bool strcmp(const char* a, const char* b);
extern void printlnc(const char* str, uint8_t color);
extern void printc(const char* str, uint8_t color);
extern void putcharc(unsigned char c, uint8_t color);
extern void printmult(unsigned char c, int l);
extern void clear_screen();
extern void printmultc(unsigned char c, int l, uint8_t color);
extern void reboot();
extern bool vfs_read_file(const char* path, char* buffer_out);
extern void beep(uint32_t frequency, uint32_t count);
extern bool vfs_write_file(const char* path, const char* data);
extern void printintc(int num, uint8_t color);
extern uint8_t get_rtc_register(int reg);
extern uint8_t get_update_in_progress_flag();
extern void scroll();
extern void cd(const char* path);
extern void run_tcc_build(char* src_path);
extern void run_tcc_exec(char* program_path);
extern void discrete_run_tcc_build(char* src_path);
extern void vgag_list_current_dir();
extern void vgag_execute(int f3i);
extern void vfs_get_cwd(char* out);
extern void vgag_ls();
extern void vgag_cd();

extern uint16_t* vga;
extern int cursorX;
extern int cursorY;

char* out;

int dcX;
int dcY;

unsigned int sqrt(unsigned int n) {
    unsigned int res = 0;
    unsigned int bit = 1 << 30; 
    while (bit > n) bit >>= 2;
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

void dputcharc(unsigned char c, uint8_t color) {
    if (c == '\n')
    {
        dcX = 0;
        dcY++;
    }
    else
    {
        vga[dcY * 80 + dcX] = (color << 8) | c;
        dcX++;

        if (dcX >= 80)
        {
            dcX = 0;
            dcY++;
        }
    }

    if (dcY >= 25)
    {
        scroll();
        dcY = 25 - 1;
    }
}

void dprintmultc(unsigned char c, int l, uint8_t color) {
    int i = 0;
    while (i < l) {
        dputcharc(c, color);
        i++;
    }
}

void dprintc(const char* str, uint8_t color) {
    for (int i = 0; str[i] != 0; i++)
        dputcharc(str[i], color);
}

void dprintintc(int num, uint8_t color) {
    char buf[16];
    int i = 0;

    if (num == 0)
    {
        dputcharc('0', color);
        return;
    }

    while (num > 0)
    {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i--)
        dputcharc(buf[i], color);
}

void dprintfloatc(float num, uint8_t color) {
    if (num < 0) {
        dputcharc('-', color);
        num = -num;
    }

    int int_part = (int)num;
    dprintintc(int_part, color);

    float frac = num - (float)int_part;

    if (frac == 0.0f) {
        return;
    }

    char buf[16];
    int i = 0;

    for (int j = 0; j < 6; j++) { 
        frac *= 10;
        int digit = (int)frac;
        buf[i++] = '0' + digit;
        frac -= digit;
    }

    while (i > 0 && buf[i - 1] == '0') {
        i--;
    }

    if (i == 0) {
        return;
    }

    dputcharc('.', color);

    for (int j = 0; j < i; j++) {
        dputcharc(buf[j], color);
    }
}

void dprintfloatcr(float num, uint8_t color) {
    if (num < 0) {
        dputcharc('-', color);
        num = -num;
    }

    num = (int)(num * 10 + 0.5f) / 10.0f;

    int int_part = (int)num;
    dprintintc(int_part, color);

    int decimal = (int)((num - int_part) * 10);

    dputcharc('.', color);
    dputcharc('0' + decimal, color);
}

void vgag_blue() {
    for (int y = 0; y < 25; y++)
        for (int x = 0; x < 80; x++)
            vga[y * 80 + x] = (0x11 << 8) | 0xDB;
}

void vgag_scc(uint8_t color) {
    for (int y = 0; y < 25; y++)
        for (int x = 0; x < 80; x++)
            vga[y * 80 + x] = (color << 8) | 0xDB;
}

void vgag_scblue() {
    for (int y = 2; y < 25; y++)
        for (int x = 0; x < 80; x++)
            vga[y * 80 + x] = (0x11 << 8) | 0xDB;
}

int int_length(int value) {
    int length = 0;

    if (value == 0) return 1;

    if (value < 0) {
        length++;
        value = -value;
    }

    while (value > 0) {
        value /= 10;
        length++;
    }

    return length;
}

#define TL 0xC9  // ╔
#define TR 0xBB  // ╗
#define BL 0xC8  // ╚
#define BR 0xBC  // ╝
#define H  0xCD  // ═
#define V  0xBA  // ║

void vgag_box() {
    dcX = 10; dcY = 5;
    dputcharc(0xC9, 0x70); dprintmultc(0xCD, 58, 0x70); dputcharc(0xBB, 0x70); // Top

    dcX = 10; dcY = 6;
    while (dcY < 19) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 10;
    } // Left side

    dcX = 69; dcY = 6;
    while (dcY < 19) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 69;
    } // right side

    dcX = 10; dcY = 19;
    dputcharc(0xC8, 0x70); dprintmultc(0xCD, 58, 0x70); dputcharc(0xBC, 0x70); // Bottom

    dcX = 11; dcY = 6;
    while (dcY < 19) {
        dprintmultc(0xDB, 58, 0x7);
        dcX = 11;
        dcY++;
    } // Fill

    dcX = 11; dcY = 20;
    dprintmultc(0xDF, 60, 0x10); 
    dcX = 70; dcY = 6;
    while (dcY < 20) {
        dputcharc(0xDB, 0x0);
        dcY++;
        dcX = 70;
    } // Shadow
    
    dcX = 0; dcY = 0;
}

void vgag_bb() {
    dcX = 3; dcY = 2;
    while (dcY < 15) {
        dprintmultc(0xDB, 15, 0x11);
        dcX = 3;
        dcY++;
    }
}

void vgag_splitbox() {
    dcX = 10; dcY = 4;
    dputcharc(0xC9, 0x70); dprintmultc(0xCD, 27, 0x70); dputcharc(0xBB, 0x70); // Top

    dcX = 10; dcY = 5;
    while (dcY < 20) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 10;
    } // Left side

    dcX = 38; dcY = 5;
    while (dcY < 20) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 38;
    } // right side

    dcX = 10; dcY = 20;
    dputcharc(0xC8, 0x70); dprintmultc(0xCD, 27, 0x70); dputcharc(0xBC, 0x70); // Bottom

    dcX = 11; dcY = 5;
    while (dcY < 20) {
        dprintmultc(0xDB, 27, 0x7);
        dcX = 11;
        dcY++;
    } // Fill

    dcX = 11; dcY = 21;
    dprintmultc(0xDF, 29, 0x10); 
    dcX = 39; dcY = 5;
    while (dcY < 21) {
        dputcharc(0xDB, 0x0);
        dcY++;
        dcX = 39;
    } // Shadow
    
    dcX = 0; dcY = 0;


    // bleh

    dcX = 41; dcY = 4;
    dputcharc(0xC9, 0x70); dprintmultc(0xCD, 27, 0x70); dputcharc(0xBB, 0x70); // Top

    dcX = 41; dcY = 5;
    while (dcY < 20) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 41;
    } // Left side

    dcX = 69; dcY = 5;
    while (dcY < 20) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 69;
    } // right side

    dcX = 41; dcY = 20;
    dputcharc(0xC8, 0x70); dprintmultc(0xCD, 27, 0x70); dputcharc(0xBC, 0x70); // Bottom

    dcX = 42; dcY = 5;
    while (dcY < 20) {
        dprintmultc(0xDB, 27, 0x7);
        dcX = 42;
        dcY++;
    } // Fill

    dcX = 42; dcY = 21;
    dprintmultc(0xDF, 29, 0x10); 
    dcX = 70; dcY = 5;
    while (dcY < 21) {
        dputcharc(0xDB, 0x0);
        dcY++;
        dcX = 70;
    } // Shadow
    
    dcX = 0; dcY = 0;
}

void vgag_bigbox() {
    dcX = 6; dcY = 3;
    dputcharc(0xC9, 0x70); dprintmultc(0xCD, 66, 0x70); dputcharc(0xBB, 0x70); // Top

    dcX = 6; dcY = 4;
    while (dcY < 22) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 6;
    } // Left side

    dcX = 73; dcY = 4;
    while (dcY < 22) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 73;
    } // right side

    dcX = 6; dcY = 22;
    dputcharc(0xC8, 0x70); dprintmultc(0xCD, 66, 0x70); dputcharc(0xBC, 0x70); // Bottom

    dcX = 7; dcY = 4;
    while (dcY < 22) {
        dprintmultc(0xDB, 66, 0x7);
        dcX = 7;
        dcY++;
    } // Fill

    dcX = 7; dcY = 23;
    dprintmultc(0xDF, 68, 0x10); 
    dcX = 74; dcY = 4;
    while (dcY < 23) {
        dputcharc(0xDB, 0x0);
        dcY++;
        dcX = 74;
    } // Shadow
    
    dcX = 0; dcY = 0;
}

void vgag_splitbigbox() {
    dcX = 6; dcY = 3;
    dputcharc(0xC9, 0x70); dprintmultc(0xCD, 66, 0x70); dputcharc(0xBB, 0x70); // Top

    dcX = 6; dcY = 4;
    while (dcY < 22) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 6;
    } // Left side

    dcX = 73; dcY = 4;
    while (dcY < 22) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 73;
    } // right side

    dcX = 6; dcY = 22;
    dputcharc(0xC8, 0x70); dprintmultc(0xCD, 66, 0x70); dputcharc(0xBC, 0x70); // Bottom

    dcX = 7; dcY = 4;
    while (dcY < 22) {
        dprintmultc(0xDB, 66, 0x7);
        dcX = 7;
        dcY++;
    } // Fill

    dcX = 7; dcY = 23;
    dprintmultc(0xDF, 68, 0x10); 
    dcX = 74; dcY = 4;
    while (dcY < 23) {
        dputcharc(0xDB, 0x0);
        dcY++;
        dcX = 74;
    } // Shadow

    dcX = 40; dcY = 3;
    dputcharc(0xCB, 0x70); 
    dcX = 40; dcY = 4;
    while (dcY < 22) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 40;
    } // divider
    dcX = 40; dcY = 22;
    dputcharc(0xCA, 0x70);
    
    dcX = 0; dcY = 0;
}

void vgag_bigbox2() {
    dcX = 60; dcY = 3;
    dputcharc(0xC9, 0x70); dprintmultc(0xCD, 13, 0x70); dputcharc(0xBB, 0x70); // Top

    dcX = 60; dcY = 4;
    while (dcY < 11) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 60;
    } // Left side

    dcX = 74; dcY = 4;
    while (dcY < 11) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 74;
    } // right side

    dcX = 60; dcY = 11;
    dputcharc(0xC8, 0x70); dprintmultc(0xCD, 13, 0x70); dputcharc(0xBC, 0x70); // Bottom

    dcX = 61; dcY = 4;
    while (dcY < 11) {
        dprintmultc(0xDB, 13, 0x7);
        dcX = 61;
        dcY++;
    } // Fill

    dcX = 61; dcY = 12;
    dprintmultc(0xDF, 15, 0x10); 
    dcX = 75; dcY = 4;
    while (dcY < 12) {
        dputcharc(0xDB, 0x0);
        dcY++;
        dcX = 75;
    } // Shadow

    // blehhhh

    dcX = 6; dcY = 3;
    dputcharc(0xC9, 0x70); dprintmultc(0xCD, 50, 0x70); dputcharc(0xBB, 0x70); // Top

    dcX = 6; dcY = 4;
    while (dcY < 22) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 6;
    } // Left side

    dcX = 57; dcY = 4;
    while (dcY < 22) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 57;
    } // right side

    dcX = 6; dcY = 22;
    dputcharc(0xC8, 0x70); dprintmultc(0xCD, 50, 0x70); dputcharc(0xBC, 0x70); // Bottom

    dcX = 7; dcY = 4;
    while (dcY < 22) {
        dprintmultc(0xDB, 50, 0x7);
        dcX = 7;
        dcY++;
    } // Fill

    dcX = 7; dcY = 23;
    dprintmultc(0xDF, 52, 0x10); 
    dcX = 58; dcY = 4;
    while (dcY < 23) {
        dputcharc(0xDB, 0x0);
        dcY++;
        dcX = 58;
    } // Shadow

    // super belh

    dcX = 60; dcY = 14;
    dputcharc(0xC9, 0x70); dprintmultc(0xCD, 13, 0x70); dputcharc(0xBB, 0x70); // Top

    dcX = 60; dcY = 15;
    while (dcY < 16) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 60;
    } // Left side

    dcX = 74; dcY = 15;
    while (dcY < 16) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 74;
    } // right side

    dcX = 60; dcY = 16;
    dputcharc(0xC8, 0x70); dprintmultc(0xCD, 13, 0x70); dputcharc(0xBC, 0x70); // Bottom

    dcX = 61; dcY = 15;
    while (dcY < 16) {
        dprintmultc(0xDB, 13, 0x7);
        dcX = 61;
        dcY++;
    } // Fill

    dcX = 61; dcY = 17;
    dprintmultc(0xDF, 15, 0x10); 
    dcX = 75; dcY = 15;
    while (dcY < 17) {
        dputcharc(0xDB, 0x0);
        dcY++;
        dcX = 75;
    } // Shadow
    
    dcX = 0; dcY = 0;
}

char usin_buffer2[20];
int usin_index2 = 0;

char psin_buffer2[20];
int psin_index2 = 0;

char username_buffer2[20];
char password_buffer2[20];
char default_buffer[20];

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

void vgag_login() {
    dcX = 24; dcY = 10;
    if (vfs_read_file("0:\\data\\password.ini", password_buffer2) && vfs_read_file("0:\\data\\username.ini", username_buffer2)) {
        dprintc("Username: ", 0x70);
        dprintmultc(0xDB, 20, 0x7F);
        dcX = 24; dcY = 12;
        dprintc("Password: ", 0x70);
        dprintmultc(0xDB, 20, 0x7F);
        dcX = 22; dcY = 10;
        dprintc(">", 0x70);
        dcX = 34; dcY = 10;
        bool userrunning = true;
        while (userrunning) {
            char key = get_key();

            if (!key) {
                continue;
            }
            else if (key == '\n') {
                if (memcmp(username_buffer2, usin_buffer2, 20) == 0)
                {
                    userrunning = false;
                }
                else 
                {
                    dcX = 18; dcY = 16;
                    dprintc("Username incorrect! The system will restart.", 0x74);
                    sleep(20000);
                    reboot();
                }
            }
            else if (key == 8) {
                if (usin_index2 > 0) {
                    usin_index2--;
                    usin_buffer2[usin_index2] = '\0';
                    dcX--;
                    dputcharc(' ', 0xF0);
                    dcX--;
                }
            }
            else {
                if (usin_index2 < 20) {
                    dputcharc(key, 0xF0);
                    usin_buffer2[usin_index2] = key;
                    usin_index2++;
                }
                else {
                    continue;
                }
            }
        }
        dcX = 22; dcY = 10;
        dputcharc(0xDB, 0x7);
        dcX = 22; dcY = 12;
        dprintc(">", 0x70);
        dcX = 34; dcY = 12;
        bool passrunning = true;
        while (passrunning) {
            char key = get_key();

            if (!key) {
                continue;
            }
            else if (key == '\n') {
                if (strcmp(password_buffer2, psin_buffer2)) {
                    passrunning = false;
                }
                else {
                    dcX = 18; dcY = 16;
                    dprintc("Password incorrect! The system will restart.", 0x74);
                    sleep(20000);
                    reboot();
                }
            }
            else if (key == 8) {
                if (psin_index2 > 0) {
                    psin_index2--;
                    psin_buffer2[psin_index2] = '\0';
                    dcX--;
                    dputcharc(' ', 0xF0);
                    dcX--;
                }
            }
            else if (key) {
                if (psin_index2 < 20) {
                    dputcharc('*', 0xF0);
                    psin_buffer2[psin_index2] = key;
                    psin_index2++;
                }
                else {
                    continue;
                }
            }
            else {
                continue;
            }
        }
        vgag_blue();
    } else {
        dcX = 22; dcY = 10;
        dprintc("New Username: ", 0x70);
        dprintmultc(0xDB, 20, 0x7F);
        dcX = 22; dcY = 12;
        dprintc("New Password: ", 0x70);
        dprintmultc(0xDB, 20, 0x7F);
        dcX = 20; dcY = 10;
        dprintc(">", 0x70);
        dcX = 36; dcY = 10;
        bool userrunning = true;
        while (userrunning) {
            char key = get_key();

            if (!key) {
                continue;
            }
            else if (key == '\n') {
                vfs_write_file("0:\\data\\username.ini", usin_buffer2);
                userrunning = false;
            }
            else if (key == 8) {
                if (usin_index2 > 0) {
                    usin_index2--;
                    usin_buffer2[usin_index2] = '\0';
                    dcX--;
                    dputcharc(' ', 0xF0);
                    dcX--;
                }
            }
            else {
                if (usin_index2 < 20) {
                    dputcharc(key, 0xF0);
                    usin_buffer2[usin_index2] = key;
                    usin_index2++;
                }
                else {
                    continue;
                }
            }
        }
        dcX = 20; dcY = 10;
        dputcharc(0xDB, 0x7);
        dcX = 20; dcY = 12;
        dprintc(">", 0x70);
        dcX = 36; dcY = 12;
        bool passrunning = true;
        while (passrunning) {
            char key = get_key();

            if (!key) {
                continue;
            }
            else if (key == '\n') {
                vfs_write_file("0:\\data\\password.ini", psin_buffer2);
                //println(psin_buffer2);
                passrunning = false;
            }
            else if (key == 8) {
                if (psin_index2 > 0) {
                    psin_index2--;
                    psin_buffer2[psin_index2] = '\0';
                    dcX--;
                    dputcharc(' ', 0xF0);
                    dcX--;
                }
            }
            else if (key) {
                if (psin_index2 < 20) {
                    dputcharc(key, 0xF0);
                    psin_buffer2[psin_index2] = key;
                    psin_index2++;
                }
                else {
                    continue;
                }
            }
            else {
                continue;
            }
        }
        dcX = 28; dcY = 16;
        dprintc("Successfully registered!", 0x72);
        sleep(20000);
        vgag_box();
        dcX = 12; dcY = 9;
        dprintc("Would you like to auto boot to shell (favored), or VGAG?", 0x70); dcX = 20; dcY = 11;
        dprintc("Press 1 for shell       Press 2 for VGAG", 0x70); dcX = 18; dcY = 13;
        dprintc("This can be changed at any time in settings.", 0x70);
        bool default_running = true;
        while (default_running) {
            char key = get_key();

            if (!key) {
                continue;
            }
            else if (key == '1') {
                vfs_write_file("0:\\data\\default.ini", "1");
                default_running = false;
            }
            else if (key == '2') {
                vfs_write_file("0:\\data\\default.ini", "2");
                default_running = false;
            }
            else {
                continue;
            }
        }
        vgag_blue();
    }
}

void vgag_printlnm(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '*') {
            dputcharc('*', 0x17);
        }
        else {
            dputcharc(str[i], 0x1F); 
        }
    }
    dcX = 16;
    dcY++;
}

void vgag_printlni(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '*') {
            dputcharc('*', 0x79);
        }
        else if (str[i] == '%') {
            dputcharc('%', 0x78);
        }
        else if (str[i] == '#') {
            dputcharc('#', 0x7C);
        }
        if (str[i] == '@') {
            dputcharc('@', 0x70);
        }
    }
    dcX = 48;
    dcY++;
}

void vgag_printlni2(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '*') {
            dputcharc('*', 0xF9);
        }
        else if (str[i] == '%') {
            dputcharc('%', 0xF8);
        }
        else if (str[i] == '#') {
            dputcharc('#', 0xFC);
        }
        if (str[i] == '@') {
            dputcharc('@', 0xF0);
        }
    }
    dcX = 55;
    dcY++;
}

void vgag_mol() {
    dcX = 16; dcY = 1;
    vgag_printlnm("                  *                       ");
    vgag_printlnm("                   *                      ");
    vgag_printlnm("                   *+=----        **      ");
    vgag_printlnm("                   =-------      *        ");
    vgag_printlnm("    **       *****+--------+*****         ");
    vgag_printlnm("     *      **    --------=+     *        ");
    vgag_printlnm("      *++++**      ===--=++       **      ");
    vgag_printlnm("     +=-----=      =-::::-=               ");
    vgag_printlnm("    *=-------+*++++-::::::-+*****       **");
    vgag_printlnm("    +--------======-::::::-=    **      * ");
    vgag_printlnm("      -----==========-::--=      **   **  ");
    vgag_printlnm("      **   ++========----==      ++====++ ");
    vgag_printlnm("    **      ++======------==    +=======+*");
    vgag_printlnm("    *        **++++-------==-::-=========*");
    vgag_printlnm("                   ==------:::::-======== ");
    vgag_printlnm("                    *++++=-::::::-++++**  ");
    vgag_printlnm("                          --::::-=        ");
    vgag_printlnm("                           +====*         ");
    vgag_printlnm("                         **      *        ");
    vgag_printlnm("                        ***      ***      ");
    vgag_printlnm("                          *               ");
    vgag_printlnm("                           **             ");
    vgag_printlnm("                                          ");
    vgag_printlnm("                  MMS - OS                ");
    dcX = 80; dcY = 25;
    sleep(5000);
    beep(660, 500);
    beep(590, 500);
    sleep(5000);
    vgag_blue();
}

void vgag_iodine() {
    dcX = 48; dcY = 6;
    vgag_printlni("********************");
    vgag_printlni("*********#@*********");
    vgag_printlni("******%%*#@*%%******");
    vgag_printlni("****%****#@****%****");
    vgag_printlni("***%***%@##@****%***");
    vgag_printlni("***%**@@@##@@***%***");
    vgag_printlni("***%*@@@@##@@@@*%***");
    vgag_printlni("****@**@@##@@**@****");
    vgag_printlni("***##############***");
    vgag_printlni("*****@@@@##@@@@*****");
    vgag_printlni("*********##*********");
}

void vgag_iodine2() {
    dcX = 55; dcY = 1;
    vgag_printlni2("********************");
    vgag_printlni2("*********#@*********");
    vgag_printlni2("******%%*#@*%%******");
    vgag_printlni2("****%****#@****%****");
    vgag_printlni2("***%***%@##@****%***");
    vgag_printlni2("***%**@@@##@@***%***");
    vgag_printlni2("***%*@@@@##@@@@*%***");
    vgag_printlni2("****@**@@##@@**@****");
    vgag_printlni2("***##############***");
    vgag_printlni2("*****@@@@##@@@@*****");
    vgag_printlni2("*********##*********");
}

void vgag_printlnmms(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '*') {
            dputcharc('*', 0xFB);
        }
        else if (str[i] == '%') {
            dputcharc('%', 0xF5);
        }
        else if (str[i] == '#') {
            dputcharc('#', 0xFD);
        }
        else if (str[i] == '@') {
            dputcharc('@', 0xF5);
        }
        else if (str[i] == '-') {
            dputcharc('-', 0xF7);
        }
        else if (str[i] == '+') {
            dputcharc('+', 0xF7);
        }
        else if (str[i] == '=') {
            dputcharc('=', 0xF7);
        }
    }
    dcX = 55;
    dcY++;
}

void vgag_mms() {
    dcX = 55; dcY = 13;
    vgag_printlnmms("@@@@%%%%@@%%*@@@@@@@");
    vgag_printlnmms("@@#*@@@+%@%%%@@@@@@@");
    vgag_printlnmms("@%#%@@%*#%#%%%@%@@@@");
    vgag_printlnmms("%@@%#+#%###%%%@@@@@@");
    vgag_printlnmms("%%##*++***+***#@@@@%");
    vgag_printlnmms("@%#+===--*#++*#+#@@%");
    vgag_printlnmms("@@#%%++======*++#@@@");
    vgag_printlnmms("@@@@%##*++*+=##%#%%%");
    vgag_printlnmms("@@@@%#%##########%%%");
    vgag_printlnmms("@%%@@@@@@%%@@@@%%@@@");
    vgag_printlnmms("@@@@@@@@@@@@@@@@@@%%");
}

/*
@@@@%%%%@@%%*@@@@@@@    @ is black, = - is white, % is dark magenta/purple, # is light, + is blue
@@#*@@@+%@%%%@@@@@@@
@%#%@@%*#%#%%%@%@@@@
%@@%#+#%###%%%@@@@@@
%%##*++***+***#@@@@%
@%#+===--*#++*#+#@@%
@@#%%++======*++#@@@
@@@@%##*++*+=##%#%%%
@@@@%#%##########%%%
@%%@@@@@@%%@@@@%%@@@
@@@@@@@@@@@@@@@@@@%%
*/

char type_buffer[128];
int type_index = 0;

void vgag_hello() {
    dcX = 11; dcY = 6; 
    dprintc("Welcome to the MMS-OS VGAG mode! ", 0x70); dputcharc(0x01, 0x70);
    dcX = 11; dcY = 7;
    dprintc("Use function keys to navigate menus", 0x70);
    dcX = 11; dcY = 9;
    dprintc("Use tab to return here at any time", 0x70);
    dcX = 11; dcY = 11;
    dprintc("This graphics mode is extremely WIP", 0x70);
    dcX = 11; dcY = 12;
    dprintc("and may contain some issues", 0x70);
    dcX = 11; dcY = 14;
    dprintc("Developed by iodinemacer with C",0x70);
}

int last_second = 0;
int frame_count = 0;
int fps = 0;

int get_fps() {
    static int last_second = -1;
    static int frame_count = 0;
    static int fps = 0;

    while (get_update_in_progress_flag());

    uint8_t second = get_rtc_register(0x00);

    second = (second & 0x0F) + ((second / 16) * 10);

    frame_count++;

    if (second != last_second) {
        if (last_second != -1) {
            fps = (fps * 3 + frame_count) / 4;
        }

        frame_count = 0;
        last_second = second;
    }

    return fps;
}

void vgag_time() {
    while (get_update_in_progress_flag());

    uint8_t second = get_rtc_register(0x00);
    uint8_t minute = get_rtc_register(0x02);
    uint8_t hour   = get_rtc_register(0x04);
    uint8_t day    = get_rtc_register(0x07);
    uint8_t month  = get_rtc_register(0x08);
    uint8_t year   = get_rtc_register(0x09);

    second = (second & 0x0F) + ((second / 16) * 10);
    minute = (minute & 0x0F) + ((minute / 16) * 10);
    hour   = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
    day    = (day & 0x0F) + ((day / 16) * 10);
    month  = (month & 0x0F) + ((month / 16) * 10);
    year   = (year & 0x0F) + ((year / 16) * 10);

    dcX = 50; dcY = 0;

    int rfps = get_fps();

    dprintc("FPS: ", 0x70); 
    dprintintc(rfps, 0x70); 
    
    dcX = 70;

    dprintintc(hour, 0x70); dputcharc(':', 0x70);
    if (minute < 10) dprintintc(0, 0x70);
    dprintintc(minute, 0x70); dputcharc(':', 0x70);
    if (second < 10) dprintintc(0, 0x70);
    dprintintc(second, 0x70);
}

void vgag_taskbar() {
    dcX = 0; dcY = 0;
    dprintmultc(0xDB, 80, 0x7);
    dcX = 1; dcY = 0;
    dputcharc(0xF0, 0x70);
    dprintmultc(0xDB, 2, 0x7);
    dprintc("F1", 0x70);
    dprintmultc(0xDB, 2, 0x7);
    dprintc("F2", 0x70);
    dprintmultc(0xDB, 2, 0x7);
    dprintc("F3", 0x70);
    dprintmultc(0xDB, 2, 0x7);
    dprintc("F4", 0x70);
    dcX = 0; dcY = 1;
    dprintmultc(0xDF, 80, 0x10); 
    vgag_time();
}

int fclick = 0;

void vgag_intro() {
    vgag_box(); vgag_hello(); vgag_iodine();
    dcX = 11; dcY = 18;
    dprintc("Press ESC to return to shell", 0x70);
    fclick = 0;
    dcX = 0; dcY = 0;
    dputcharc(0xDB, 0x8);
    dputcharc(0xF0, 0x8F);
    dputcharc(0xDB, 0x8);
    dprintmultc(0xDB, 1, 0x7);
    dprintc("F1", 0x70);
    dprintmultc(0xDB, 2, 0x7);
    dprintc("F2", 0x70);
    dprintmultc(0xDB, 2, 0x7);
    dprintc("F3", 0x70);
    dprintmultc(0xDB, 2, 0x7);
    dprintc("F4", 0x70);
}

void vgag_about() {
    vgag_scc(0x00);
    sleep(1000);
    vgag_scc(0x88);
    sleep(1000);
    vgag_scc(0x77);
    sleep(1000);
    vgag_scc(0xFF);
    sleep(1000);
    dcX = 4; dcY = 3;
    dprintc("Molecular Multiverse Services OS:", 0xF0); dcX = 4; dcY = 4;
    dprintc("developed by the the MMS team with C.", 0xF0); dcX = 4; dcY = 6;
    dprintc("If you need support, contact therealiodinemacer", 0xF0); dcX = 4; dcY = 7;
    dprintc("or join discord.com/ZAx3NN5TJY on Discord.", 0xF0); dcX = 4; dcY = 9;
    dprintc("MMS-OS is the culmination of many MMS projects", 0xF0); dcX = 4; dcY = 10;
    dprintc("spread across varied platforms and languages,", 0xF0); dcX = 4; dcY = 11;
    dprintc("most notably the MMS-2 Discord bot, coded in JS.", 0xF0); dcX = 4; dcY = 13;
    dprintc("While this project isn't particularly high-", 0xF0); dcX = 4; dcY = 14;
    dprintc("quality, I've still enjoyed developing it,", 0xF0); dcX = 4; dcY = 15;
    dprintc("it is pretty rewarding to build something", 0xF0); dcX = 4; dcY = 16;
    dprintc("like this.", 0xF0); dcX = 4; dcY = 18;
    dprintc("Press enter to return to shell.", 0xF0);
    vgag_iodine2();
    vgag_mms();
    bool running = true;
    while (running) {
        unsigned char key = get_key();

        if (!key) {
            continue;
        }
        else if (key == '\n') {
            running = false;
        }
    }
    vgag_scc(0x77);
    sleep(1000);
    vgag_scc(0x88);
    sleep(2000);
    vgag_scc(0x00);
    clear_screen();
}

void old_vgag_f1() {
    /*
    dcX = 3; dcY = 2;
    while (dcY < 15) {
        dprintmultc(0xDB, 20, 0x11);
        dcX = 3;
        dcY++;
    }
    */

    dcX = 3; dcY = 2;
    dputcharc(0xC9, 0x70); dprintmultc(0xCD, 13, 0x70); dputcharc(0xBB, 0x70); // Top

    dcX = 3; dcY = 3;
    while (dcY < 15) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 3;
    } // Left side

    dcX = 17; dcY = 3;
    while (dcY < 15) {
        dputcharc(0xBA, 0x70);
        dcY++;
        dcX = 17;
    } // right side

    dcX = 3; dcY = 15;
    dputcharc(0xC8, 0x70); dprintmultc(0xCD, 13, 0x70); dputcharc(0xBC, 0x70); // Bottom

    dcX = 4; dcY = 3;
    while (dcY < 15) {
        dprintmultc(0xDB, 13, 0x7);
        dcX = 4;
        dcY++;
    } // Fill

    dcX = 4; dcY = 16;
    dprintmultc(0xDF, 6, 0x10); dprintmultc(0xDF, 9, 0x70);
    dcX = 18; dcY = 3;
    while (dcY < 16) {
        dputcharc(0xDB, 0x0);
        dcY++;
        dcX = 18;
    } // Shadow
    
    dcX = 0; dcY = 0;
}

void vgag_f1() {
    vgag_scblue();
    vgag_splitbox();
    dcX = 10; dcY = 7;
    dputcharc(0xC7, 0x70); dprintmultc(0xC4, 27, 0x70); dputcharc(0xB6, 0x70);
    dcX = 42; dcY = 5;
    dprintc("HISTORY", 0x70);
    dcX = 41; dcY = 6;
    dputcharc(0xC7, 0x70); dprintmultc(0xC4, 27, 0x70); dputcharc(0xB6, 0x70);

    //numpad 1

    dcX = 14; dcY = 8;
    dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70);
    dcX = 14; dcY = 9;
    dputcharc(0xB3, 0x70); dputcharc('1', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('2', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('3', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('+', 0x70); dputcharc(0xB3, 0x70);
    dcX = 14; dcY = 10;
    dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); 

    //numpad 2

    dcX = 14; dcY = 11;
    dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70);
    dcX = 14; dcY = 12;
    dputcharc(0xB3, 0x70); dputcharc('4', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('5', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('6', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('-', 0x70); dputcharc(0xB3, 0x70);
    dcX = 14; dcY = 13;
    dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); 

    //numpad 3

    dcX = 14; dcY = 14;
    dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70);
    dcX = 14; dcY = 15;
    dputcharc(0xB3, 0x70); dputcharc('7', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('8', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('9', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('*', 0x70); dputcharc(0xB3, 0x70);
    dcX = 14; dcY = 16;
    dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); 

    //numpad 4
    dcX = 14; dcY = 17;
    dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70); dcX = dcX + 3; dputcharc(0xDA, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xBF, 0x70);
    dcX = 14; dcY = 18;
    dputcharc(0xB3, 0x70); dputcharc(' ', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('0', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc(' ', 0x70); dputcharc(0xB3, 0x70); dcX = dcX + 3; dputcharc(0xB3, 0x70); dputcharc('/', 0x70); dputcharc(0xB3, 0x70);
    dcX = 14; dcY = 19;
    dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); dcX = dcX + 3; dputcharc(0xC0, 0x70); dputcharc(0xC4, 0x70); dputcharc(0xD9, 0x70); 
}

void ball(int x, int y) {
    dcX = x; dcY = y;
    dprintmultc(0xDB, 2, 0x55);
}

float g = 9.8;
float e = 0.5;
int h = 15;
int iter = 100;
int ps = 0;

void vgag_f2() {
    vgag_scblue();
    vgag_bigbox2();
    dcX = 61; dcY = 4;
    if (ps == 0) {dprintc("g = ", 0x8F); dprintfloatcr(g, 0x8F); dprintc(" < >", 0x8F); dcY = dcY + 2; dcX = 61;}
    else {dprintc("g = ", 0x70); dprintfloatcr(g, 0x70); dcY = dcY + 2; dcX = 61;}

    if (ps == 1) {dprintc("e = ", 0x8F); dprintfloatcr(e, 0x8F); dprintc(" < >", 0x8F); dcY = dcY + 2; dcX = 61;}
    else {dprintc("e = ", 0x70); dprintfloatcr(e, 0x70); dcY = dcY + 2; dcX = 61;}

    if (ps == 2) {dprintc("h = ", 0x8F); dprintintc(h, 0x8F); dprintc(" < >", 0x8F);  dcY = dcY + 2; dcX = 61;}
    else {dprintc("h = ", 0x70); dprintintc(h, 0x70);  dcY = dcY + 2; dcX = 61;}

    if (ps == 3) {dprintc("i = ", 0x8F); dprintintc(iter, 0x8F); dprintc(" < >", 0x8F);}
    else {dprintc("i = ", 0x70); dprintintc(iter, 0x70);}
    dcX = 61; dcY = 15;
    dputcharc(' ', 0x70); dputcharc(0x10, 0x70); dprintc(" Spacebar", 0x70);
    ball(32, 20-h);
}

void f2_clear() {
    dcX = 7; dcY = 4;
    while (dcY < 22) {
        dprintmultc(0xDB, 50, 0x7);
        dcX = 7;
        dcY++;
    } 
}

void old_vgag_f3() {
    discrete_run_tcc_build("periodic.c");
    run_tcc_exec("periodic.tbc");
}

int f3i = 0;
int f4i = 0;

void vgag_f3() {
    vgag_scblue();
    vgag_box();
    vgag_list_current_dir();
    dcX = 11; dcY = 8+f3i;
    dputcharc('>', 0x70);
}

void f3_arrows() {
    dcX = 11; dcY = 8;
    while (dcY < 19) {
        dputcharc(0xDB, 0x77);
        dcY++;
        dcX = 11;
    } 
    dcX = 11; dcY = 8+f3i;
    dputcharc('>', 0x70);
}

void vgag_f4() {
    vgag_scblue();
    vgag_splitbigbox();
    vgag_ls();
    dcX = 7; dcY = 6+f4i;
    dputcharc('>', 0x70);
    dcX = 7; dcY = 21;
    dprintc("  ", 0x70);
    dputcharc(0x1e, 0x70);
    dprintc(" Return to root", 0x70);
}

void f4_reset() {
    dcX = 41; dcY = 4;
    while (dcY < 22) {
        dprintmultc(0xDB, 31, 0x7);
        dcX = 41;
        dcY++;
    }
}

void f4_arrows() {
    dcX = 7; dcY = 6;
    while (dcY < 22) {
        dputcharc(0xDB, 0x77);
        dcY++;
        dcX = 7;
    } 
    dcX = 7; dcY = 6+f4i;
    dputcharc('>', 0x70);
}

float abs(float input) {
    return (input < 0) ? -input : input;
}

char num1[9];
bool num1_focus = true;
int num1_index = 0;

char num2[9];
bool num2_focus = false;
int num2_index = 0;

char op[1];
int op_index = 0;

bool finished = false;

int history_y = 7;

bool f1_open = false;
bool f2_open = false;
bool f3_open = false;
bool f4_open = false;

void debug() {
    dcX = 0; dcY = 24;
    if (f1_open == true) dprintc("True", 0x17);
    else if (f1_open == false) dprintc("False", 0x17);
    dcX = 6;
    if (num1_focus == true) dprintc("num1", 0x17);
    else if (num2_focus == true) dprintc("num2", 0x17);
    dcX = 11;
    dprintintc(ps, 0x17); dcX++;
    dprintintc(g, 0x17); dcX++;
    dprintintc(e, 0x17); dcX++;
    dprintintc(h, 0x17); dcX++;
    dprintintc(f3i, 0x17); dcX++;
}

int last_dcX = 0;
int last_dcY = 0;

void calc_reset() {
    num1_focus = true;
    num2_focus = false;
    for (int i = 0; i < 9; i++) {
        num1[i] = 0;
    }
    num1_index = 0;
    for (int i = 0; i < 9; i++) {
        num2[i] = 0;
    }
    num2_index = 0;
    for (int i = 0; i < 1; i++) {
        op[i] = 0;
    }
    op_index = 0;
    dcX = 11; dcY = 5;
    dprintmultc(0xDB, 27, 0x7);
    dcX = 11; dcY = 6;
    dprintmultc(0xDB, 27, 0x7);
    cursorX = 11; cursorY = 5;
}

void right_reset() {
    dcX = 42; dcY = 7;
    while (dcY < 20) {
        dprintmultc(0xDB, 27, 0x7);
        dcX = 42;
        dcY++;
    } 
    history_y = 7;
}

int INT_MAX =  2147483647;
int INT_MIN = -2147483648;

void vgag_run() {
    clear_screen();
    vgag_blue();
    vgag_taskbar();
    vgag_intro();
    dcX = 11; dcY = 6;
    cursorX = 11; cursorY = 5;
    bool running = true;
    while (running) {
        vgag_time();
        //debug();
        unsigned char key = get_key();

        if (!key) {
            continue;
        }
        else if (key == 27) {
            //vfs_get_cwd(out);
            //if (strcmp(out, ))
            cd("..");
            running = false;
        }
        else if (key == 9) {
            vgag_scblue();
            vgag_intro();
            f1_open = false;
            f2_open = false;
            f3_open = false;
            f4_open = false;
        }
        else if (key == 128) {
            cursorX = 11; cursorY = 6;
            dcX = 0; dcY = 0;
            dputcharc(0xDB, 0x7);
            dputcharc(0xF0, 0x70);
            dputcharc(0xDB, 0x7);
            dputcharc(0xDB, 0x8);
            dprintc("F1", 0x8F);
            dprintmultc(0xDB, 1, 0x8);
            dprintmultc(0xDB, 1, 0x7);
            dprintc("F2", 0x70);
            dprintmultc(0xDB, 2, 0x7);
            dprintc("F3", 0x70);
            dprintmultc(0xDB, 2, 0x7);
            dprintc("F4", 0x70);
            dprintmultc(0xDB, 1, 0x7);
            fclick++;
            if (fclick == 1) vgag_bb();
            vgag_f1();
            f1_open = true;
            f2_open = false;
            f3_open = false;
            f4_open = false;
            calc_reset();
        }
        else if (key == 129) {
            dcX = 0; dcY = 0;
            dputcharc(0xDB, 0x7);
            dputcharc(0xF0, 0x70);
            dputcharc(0xDB, 0x7);
            dprintmultc(0xDB, 1, 0x7);
            dprintc("F1", 0x70);
            dprintmultc(0xDB, 1, 0x7);
            dputcharc(0xDB, 0x8);
            dprintc("F2", 0x8F);
            dprintmultc(0xDB, 1, 0x8);
            dprintmultc(0xDB, 1, 0x7);
            dprintc("F3", 0x70);
            dprintmultc(0xDB, 1, 0x7);
            dprintmultc(0xDB, 1, 0x7);
            dprintc("F4", 0x70);
            dprintmultc(0xDB, 1, 0x7);
            fclick++;
            if (fclick == 1) vgag_bb();
            vgag_f2();
            f2_open = true;
            f1_open = false;
            f3_open = false;
            f4_open = false;
        }
        else if (key == 130) {
            dcX = 0; dcY = 0;
            dputcharc(0xDB, 0x7);
            dputcharc(0xF0, 0x70);
            dputcharc(0xDB, 0x7);
            dprintmultc(0xDB, 1, 0x7);
            dprintc("F1", 0x70);
            dprintmultc(0xDB, 2, 0x7);
            dprintc("F2", 0x70);
            dprintmultc(0xDB, 1, 0x7);
            dputcharc(0xDB, 0x8);
            dprintc("F3", 0x8F);
            dprintmultc(0xDB, 1, 0x8);
            dprintmultc(0xDB, 1, 0x7);
            dprintc("F4", 0x70);
            dprintmultc(0xDB, 1, 0x7);
            fclick++;
            if (fclick == 1) vgag_bb();
            vgag_f3();
            f1_open = false;
            f2_open = false;
            f3_open = true;
            f4_open = false;
        }
        else if (key == 131) {
            dcX = 0; dcY = 0;
            dputcharc(0xDB, 0x7);
            dputcharc(0xF0, 0x70);
            dputcharc(0xDB, 0x7);
            dprintmultc(0xDB, 1, 0x7);
            dprintc("F1", 0x70);
            dprintmultc(0xDB, 2, 0x7);
            dprintc("F2", 0x70);
            dprintmultc(0xDB, 1, 0x7);
            dputcharc(0xDB, 0x7);
            dprintc("F3", 0x70);
            dprintmultc(0xDB, 1, 0x7);
            dprintmultc(0xDB, 1, 0x8);
            dprintc("F4", 0x8F);
            dprintmultc(0xDB, 1, 0x8);
            fclick++;
            if (fclick == 1) vgag_bb();
            cd("..");
            vgag_f4();
            f1_open = false;
            f2_open = false;
            f3_open = false;
            f4_open = true;
        }
        else {
            if (f1_open == true) {   
                if (finished == true) {
                    calc_reset(); 
                    finished = false;  
                }    
                else if (num1_focus == true) {
                    if ((key == '+' || key == '-' || key == '*' || key == '/') && num1_index > 0) {
                        putcharc(' ', 0x70); putcharc(key, 0x70); putcharc(' ', 0x70); 
                        op[op_index] = key;
                        op_index++;
                        num2_focus = true;
                        num1_focus = false;
                    }
                    else if (key == '\n') {
                        dcX = 32; dcY = 6;
                        dprintc("Error!", 0x74);
                        finished = true;
                    }
                    else if (num1_index == 8) {
                        continue;
                    }
                    else if (isdigit(key)) {
                        putcharc(key, 0x70);   
                        num1[num1_index] = key;
                        num1_index++;
                    }
                    else {
                        continue;
                    }
                }
                if (num2_focus == true) {
                    if (key == '\n' && num2_index == 0) {
                        dcX = 32; dcY = 6;
                        dprintc("Error!", 0x74);
                        finished = true;
                    }
                    else if (key == '\n') {
                        int first_num = atoi(num1);
                        int second_num = atoi(num2);
                        if (strcmp(op, "+")) {
                            if ((second_num > 0 && first_num > INT_MAX - second_num) || (second_num < 0 && first_num < INT_MIN - second_num)) {
                                dcX = 23; dcY = 6;
                                dprintc("Overflow error!", 0x74);
                                finished = true;
                            }
                            else {
                                int output = first_num + second_num;
                                int offset = int_length(output) + (output < 0 ? 2 : 1);
                                dcX = 38 - offset; dcY = 6;
                                dputcharc('=', 0x70);
                                dprintintc(output, 0x70);
                                if (history_y >= 18) right_reset();
                                dcX = 42; dcY = history_y;
                                dprintintc(first_num, 0x70); dputcharc(' ', 0x70); dprintc(op, 0x70); dputcharc(' ', 0x70); dprintintc(second_num, 0x70);
                                dcX = 69 - offset; dcY++;
                                dputcharc('=', 0x70); dprintintc(output, 0x70);
                                dcY = dcY + 2;
                                history_y = dcY;
                                finished = true;
                            }
                        }
                        else if (strcmp(op, "-")) {
                            if ((second_num < 0 && first_num > INT_MAX + second_num) || (second_num > 0 && first_num < INT_MIN + second_num)) {
                                dcX = 23; dcY = 6;
                                dprintc("Overflow error!", 0x74);
                                finished = true;
                            }
                            else {
                                int output = first_num - second_num;
                                int unsigned_output = (output < 0) ? -output : output;
                                int offset = int_length(output) + (output < 0 ? 2 : 1);
                                if (output < 0) { 
                                    dcX = (38 - offset + 1); dcY = 6; 
                                    dputcharc('=', 0x70); dputcharc('-', 0x70); dprintintc(unsigned_output, 0x70);
                                    if (history_y >= 18) right_reset();
                                    dcX = 42; dcY = history_y;
                                    dprintintc(first_num, 0x70); dputcharc(' ', 0x70); dprintc(op, 0x70); dputcharc(' ', 0x70); dprintintc(second_num, 0x70);
                                    dcX = 69 - offset + 1; dcY++;
                                    dputcharc('=', 0x70); dputcharc('-', 0x70); dprintintc(unsigned_output, 0x70);
                                    dcY = dcY + 2;
                                    history_y = dcY; 
                                    finished = true; 
                                } else { 
                                    dcX = (38 - offset); dcY = 6; 
                                    dputcharc('=', 0x70); dprintintc(output, 0x70); 
                                    if (history_y >= 18) right_reset();
                                    dcX = 42; dcY = history_y;
                                    dprintintc(first_num, 0x70); dputcharc(' ', 0x70); dprintc(op, 0x70); dputcharc(' ', 0x70); dprintintc(second_num, 0x70);
                                    dcX = 69 - offset; dcY++;
                                    dputcharc('=', 0x70); dprintintc(output, 0x70);
                                    dcY = dcY + 2;
                                    history_y = dcY;
                                    finished = true; 
                                }
                            }
                        }
                        else if (strcmp(op, "*")) {
                            if (first_num != 0 && (second_num > INT_MAX / first_num || second_num < INT_MIN / first_num)) {
                                dcX = 23; dcY = 6;
                                dprintc("Overflow error!", 0x74);
                                finished = true;
                            }
                            else {
                                int output = first_num * second_num;
                                int offset = int_length(output) + (output < 0 ? 2 : 1);
                                dcX = 38 - offset; dcY = 6;
                                dputcharc('=', 0x70);
                                dprintintc(output, 0x70);
                                if (history_y >= 18) right_reset();
                                dcX = 42; dcY = history_y;
                                dprintintc(first_num, 0x70); dputcharc(' ', 0x70); dprintc(op, 0x70); dputcharc(' ', 0x70); dprintintc(second_num, 0x70);
                                dcX = 69 - offset; dcY++;
                                dputcharc('=', 0x70); dprintintc(output, 0x70);
                                dcY = dcY + 2;
                                history_y = dcY;
                                finished = true;
                            }
                        }
                        else if (strcmp(op, "/")) {
                            if (second_num == 0) {
                                dcX = 17; dcY = 6;
                                dprintc("Divide by zero error!", 0x74);
                                finished = true;
                            }
                            else if (first_num == INT_MIN && second_num == -1) {
                                dcX = 23; dcY = 6;
                                dprintc("Overflow error!", 0x74);
                                finished = true;
                            }
                            else {
                                int output = first_num / second_num;
                                int offset = int_length(output) + (output < 0 ? 2 : 1);
                                dcX = 38 - offset; dcY = 6;
                                dputcharc('=', 0x70);
                                dprintintc(output, 0x70);
                                if (history_y >= 18) right_reset();
                                dcX = 42; dcY = history_y;
                                dprintintc(first_num, 0x70); dputcharc(' ', 0x70); dprintc(op, 0x70); dputcharc(' ', 0x70); dprintintc(second_num, 0x70);
                                dcX = 69 - offset; dcY++;
                                dputcharc('=', 0x70); dprintintc(output, 0x70);
                                dcY = dcY + 2;
                                history_y = dcY;
                                finished = true;
                            }
                        }
                    }
                    else if (num2_index == 8) {
                        continue;
                    }
                    else if (isdigit(key)) {
                        putcharc(key, 0x70);   
                        num2[num2_index] = key;
                        num2_index++;
                    }
                    else {
                        continue;
                    }
                }
            }
            else if (f2_open == true) { 
                if (key == ' ') {
                    int i = 0;
                    float y = 20-h;        
                    float v = 0;    

                    while (i<iter) {
                        sleep(300); 
                        v += (g/50);    
                        y += v;    

                        if (y >= 20) { 
                            y = 20;
                            v = -v * (e); 
                        }

                        i++;

                        f2_clear();
                        ball(32, (int)y);
                    }
                    sleep(10000);
                    vgag_f2();
                }
                else if (key == 140) {
                    if (ps == 0) continue;
                    else {ps--; vgag_f2();}
                }
                else if (key == 141) {
                    if (ps == 3) continue;
                    else {ps++; vgag_f2();}
                }
                else if (key == 142 && g > 0.1 && ps == 0) {
                    g -= 0.1;
                    vgag_f2();
                }
                else if (key == 143 && g < 20 && ps == 0) {
                    g += 0.1;
                    vgag_f2();
                }
                else if (key == 142 && e > 0.1 && ps == 1) {
                    e -= 0.1;
                    vgag_f2();
                }
                else if (key == 143 && e < 1.0 && ps == 1) {
                    e += 0.1;
                    vgag_f2();
                }
                else if (key == 142 && h != 1 && ps == 2) {
                    h--;
                    vgag_f2();
                }
                else if (key == 143 && h != 15 && ps == 2) {
                    h++;
                    vgag_f2();
                }
                else if (key == 142 && iter != 10 && ps == 3) {
                    iter -= 10;
                    vgag_f2();
                }
                else if (key == 143 && iter != 1000 && ps == 3) {
                    iter += 10;
                    vgag_f2();
                }
                else {
                    continue;
                }
            }
            else if (f3_open == true) {
                if (key == '\n') {
                    vgag_execute(f3i);
                }
                else if (key == 140) {
                    if (f3i == 0) continue;
                    else {f3i--; f3_arrows();}
                }
                else if (key == 141) {
                    if (f3i == 10) continue;
                    else {f3i++; f3_arrows();}
                }
            }
            else if (f4_open == true) {
                if (key == '\n') {
                    vgag_cd(f4i);
                    f4i = 0;
                    f4_arrows();
                }
                else if (key == 140) {
                    if (f4i == 0) continue;
                    else {f4i--; f4_arrows();}
                }
                else if (key == 141) {
                    if (f4i == 15) continue;
                    else {f4i++; f4_arrows();}
                }
            }
            else {
                continue;
            }
        }
    }
    clear_screen();
}

void ind_login() {
    clear_screen();
    vgag_blue();
    vgag_box();
    vgag_login();
    vgag_mol();
    dcX = 0; dcY = 0;
    if (vfs_read_file("0:\\data\\default.ini", default_buffer)) {
        if (strcmp(default_buffer, "1")) {}
        else if (strcmp(default_buffer, "2")) {
            vgag_run();
        }
        else {}
    }
    else {}
}
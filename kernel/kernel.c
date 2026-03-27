#include <stdint.h>
#include <stdbool.h>
#include "fs.h"
#include "calc.h"
#include "login.h"
#include "wordle.h"
#include "fsc.h"
#include "tinycc.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

uint16_t* vga = (uint16_t*)0xB8000;

int cursorX = 0;
int cursorY = 0;

uint8_t color = 0x0F;

int memcmp(const void* s1, const void* s2, int n) {
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;
    for (int i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

void sleep(uint32_t count) {
    for (volatile uint32_t i = 0; i < count * 10000; i++) {
        __asm__ volatile("nop");
    }
}

void memset(void* dest, uint8_t val, uint32_t len) {
    uint8_t* ptr = (uint8_t*)dest;
    while (len-- > 0) *ptr++ = val;
}

// Text

void scroll() {
    for (int y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            vga[y * VGA_WIDTH + x] = vga[(y + 1) * VGA_WIDTH + x];
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++)
    {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (color << 8) | ' ';
    }
}

void putchar(char c) {
    if (c == '\n')
    {
        cursorX = 0;
        cursorY++;
    }
    else
    {
        vga[cursorY * VGA_WIDTH + cursorX] = (color << 8) | c;
        cursorX++;

        if (cursorX >= VGA_WIDTH)
        {
            cursorX = 0;
            cursorY++;
        }
    }

    if (cursorY >= VGA_HEIGHT)
    {
        scroll();
        cursorY = VGA_HEIGHT - 1;
    }
}

void print(const char* str) {
    for (int i = 0; str[i] != 0; i++)
        putchar(str[i]);
}

void println(const char* str) {
    print(str);
    putchar('\n');
}

void clear_screen() {
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            vga[y * VGA_WIDTH + x] = (color << 8) | ' ';

    cursorX = 0;
    cursorY = 0;
}

void path_prepend(char* path) {
    if (path[0] == '0' && path[1] == ':' && path[2] == '\\') {
        return;
    }

    char cwd[64];
    vfs_get_cwd(cwd);

    int cwd_len = 0;
    while (cwd[cwd_len] != '\0') cwd_len++;

    int path_len = 0;
    while (path[path_len] != '\0') path_len++;

    bool need_sep = !(cwd_len == 3 && cwd[2] == '\\');
    int prefix_len = cwd_len + (need_sep ? 1 : 0);

    for (int i = path_len; i >= 0; i--) {
        path[i + prefix_len] = path[i];
    }

    for (int i = 0; i < cwd_len; i++) {
        path[i] = cwd[i];
    }
    if (need_sep) {
        path[cwd_len] = '\\';
    }
}

bool strscmp(const char* str, const char* check, const int count) {
    for (int i = 0; i < count; i++) {
        if (str[i] != check[i]) {
            return false;
        }
    }
    return true;
}



// IO

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

void reboot() {
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);

    while (1);
}

void shutdown() {
    outw(0x604, 0x2000);
}

bool isdigit(char c) {
    return (c >= '0' && c <= '9');
}

int atoi(const char* str) {
    int result = 0;
    int i = 0;

    while (str[i] >= '0' && str[i] <= '9')
    {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return result;
}

void printint(int num) {
    char buf[16];
    int i = 0;

    if (num == 0)
    {
        putchar('0');
        return;
    }

    while (num > 0)
    {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i--)
        putchar(buf[i]);
}

// Sound

void play_sound(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;

    outb(0x43, 0xB6);

    outb(0x42, (uint8_t)(divisor & 0xFF));
    outb(0x42, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

void stop_sound() {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

void beep(uint32_t frequency, uint32_t count) {
    play_sound(frequency); 
    sleep((count*10));       
    stop_sound();
}

void musparse_line(const char* line, int* freq, int* duration) {
    char freq_buf[16];
    char dur_buf[16];

    int i = 0, j = 0;

    while (line[i] != ' ' && line[i] != '\0') {
        freq_buf[j++] = line[i++];
    }
    freq_buf[j] = '\0';

    if (line[i] == ' ') i++;

    j = 0;
    while (line[i] != '\0') {
        dur_buf[j++] = line[i++];
    }
    dur_buf[j] = '\0';

    *freq = atoi(freq_buf);
    *duration = atoi(dur_buf);
}

void play_music(const char* path) {
    char line[128];

    print("Now playing ");
    print(path);
    putchar('\n');

    while (vfs_read_file_line(path, line)) {
        int freq = 0;
        int duration = 0;

        musparse_line(line, &freq, &duration);

        if (freq > 0 && duration > 0) {
            play_sound(freq);
            sleep(duration*15);
            stop_sound();
            sleep(550); 
        }
    }
}

// Keyboard

char keyboard_map[128] = {
0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,
'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
'b','n','m',',','.','/',42,'*',54,' ',0
};

char keyboard_map_shift[128] = {
0,27,'!','@','#','$','%','^','&','*','(',')','_','+',8,9,
'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
'D','F','G','H','J','K','L',':', '"','~',0,'|','Z','X','C','V',
'B','N','M','<','>','?',42,'*',54,' ',0
};

bool shift_pressed = false;

char get_key() {
    if (!(inb(0x64) & 1))
        return 0;

    uint8_t scancode = inb(0x60);

    if (scancode == 0x2A) {
        shift_pressed = true;
        return 0;
    }
    if (scancode == 0xAA) {
        shift_pressed = false;
        return 0;
    }

    if (scancode == 0x36) {
        shift_pressed = true;
        return 0;
    }
    if (scancode == 0xB6) {
        shift_pressed = false;
        return 0;
    }

    if (scancode & 0x80)
        return 0;

    if (scancode > 57)
        return 0;

    if (shift_pressed)
        return keyboard_map_shift[scancode];
    else
        return keyboard_map[scancode];
}

#define CMD_BUFFER 128
char cmd_buffer[CMD_BUFFER];
int cmd_index = 0;

bool strcmp(const char* a, const char* b) {
    int i = 0;

    while (a[i] && b[i])
    {
        if (a[i] != b[i])
            return false;
        i++;
    }

    return a[i] == b[i];
}

char* getarg(const char* input, const int count) {
    static char output[128];

    int i = count + 1;
    int out_index = 0;

    while (i < 128 && input[i] != '\0') {
        output[out_index++] = input[i++];
    }

    output[out_index] = '\0'; 
    return output;
}

// Time & Date

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

uint8_t get_update_in_progress_flag() {
    outb(CMOS_ADDRESS, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

uint8_t get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

void print_time() {
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

    print("Current Time: ");
    printint(hour);   putchar(':');
    printint(minute); putchar(':');
    printint(second); print(" UTC");
    print("  Date: ");
    printint(day);    putchar('/');
    printint(month);  putchar('/');
    print("20"); printint(year); 
    println("");
}

// Command logic

char arg_buffer[128];

void run_command() {
    putchar('\n');

    if (strscmp(cmd_buffer, "help", 4)) {
        println("Commands:");
        println("help : This page!                     |  about : OS information");
        println("calc : Simple calculator              |  clear : Clears the screen");
        println("time : Display current time and date  |  wordle : Plays a game of Wordle");
        println("music : Plays a test music file       |  cc : Build a Tiny C source file");
        println("                                      |");
        println("cc : Build a C source file            |  cexec : Run a compiled C file");
        println("                                      |");
        println("ls : List current directory           |  pwd : Show current path");
        println("cd : Change current directory         |  mkdir : Create directory");
        println("rmdir : Remove empty directory        |  mkf : Make a new text file");
        println("read : Read a file in current dir     |  rmf : Delete file in current dir");
        println("                                      |");
        println("reboot : Reboots system               |  shutdown : Shuts down system");
        println("reset : Resets system completely      |");
    }
    else if (strscmp(cmd_buffer, "clear", 5)) clear_screen();
    else if (strscmp(cmd_buffer, "about", 5)) {
        println("Molecular Multiverse Services OS: developed by the the MMS team with C.");
        println("If you need support, contact therealiodinemacer or join ZAx3NN5TJY on Discord.");
    }
    else if (strscmp(cmd_buffer, "read", 4)) read(getarg(cmd_buffer, 4));
    else if (strscmp(cmd_buffer, "pwd", 3)) pwd();
    else if (strscmp(cmd_buffer, "cd", 2)) cd(getarg(cmd_buffer, 2));
    else if (strscmp(cmd_buffer, "mkdir", 5)) mkdir_cmd(getarg(cmd_buffer, 5));
    else if (strscmp(cmd_buffer, "rmdir", 5)) rmdir_cmd(getarg(cmd_buffer, 5));
    else if (strscmp(cmd_buffer, "ls", 2)) ls();
    else if (strscmp(cmd_buffer, "time", 4)) print_time();
    else if (strscmp(cmd_buffer, "calc", 4)) run_calc();
    else if (strscmp(cmd_buffer, "wordle", 6)) run_wordle();
    else if (strscmp(cmd_buffer, "rmf", 3)) rmf(getarg(cmd_buffer, 3));
    else if (strscmp(cmd_buffer, "mkf", 3)) mkf(getarg(cmd_buffer, 3));
    else if (strscmp(cmd_buffer, "cc", 2)) run_tcc_build(getarg(cmd_buffer, 2));
    else if (strscmp(cmd_buffer, "cexec", 5)) run_tcc_exec(getarg(cmd_buffer, 5));
    else if (strscmp(cmd_buffer, "shutdown", 8)) shutdown();
    else if (strscmp(cmd_buffer, "reboot", 6)) reboot();
    else if (strscmp(cmd_buffer, "music", 5)) play_music("0:\\music\\ode.md");
    else if (strscmp(cmd_buffer, "reset", 5)) vfs_reset();
    else println("Unknown command");

    //println("");
    char cwd_prompt[64];
    vfs_get_cwd(cwd_prompt);
    print(cwd_prompt);
    print(" > ");

    cmd_index = 0;

    for(int i = 0; i < CMD_BUFFER; i++)
        cmd_buffer[i] = 0;

    for(int i = 0; i < CMD_BUFFER; i++)
        arg_buffer[i] = 0;
}

// Main loop

void kernel_main() {
    clear_screen();

    vfs_init();

    println("                  *                       ");
    println("                   *                      ");
    println("                   *+=----        **      ");
    println("                   =-------      *        ");
    println("    **       *****+--------+*****         ");
    println("     *      **    --------=+     *        ");
    println("      *++++**      ===--=++       **      ");
    println("     +=-----=      =-::::-=               ");
    println("    *=-------+*++++-::::::-+*****       **");
    println("    +--------======-::::::-=    **      * ");
    println("      -----==========-::--=      **   **  ");
    println("      **   ++========----==      ++====++ ");
    println("    **      ++======------==    +=======+*");
    println("    *        **++++-------==-::-=========*");
    println("                   ==------:::::-======== ");
    println("                    *++++=-::::::-++++**  ");
    println("                          --::::-=        ");
    println("                           +====*         ");
    println("                         **      *        ");
    println("                        ***      ***      ");
    println("                          *               ");
    println("                           **             ");

    handle_login();
    beep(660, 500);
    beep(590, 500);

    println("Welcome to MMS-OS!");
    print_time();
    println("Type 'help' for commands");
    char cwd_prompt[64];
    vfs_get_cwd(cwd_prompt);
    print(cwd_prompt);
    print(" > ");

    while (1) {
        char key = get_key();

        if (!key) {
            continue;
        }
        if (key == '\n') {
            run_command();
        }
        else if (key == 8) {
            if (cmd_index > 0) {
                cmd_index--;
                cmd_buffer[cmd_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else {
            putchar(key);
            cmd_buffer[cmd_index] = key;
            cmd_index++;
        }
    }
}

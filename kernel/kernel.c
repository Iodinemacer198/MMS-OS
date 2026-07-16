#include <stdint.h>
#include <stdbool.h>
#include "fs.h"
#include "calc.h"
#include "login.h"
#include "wordle.h"
#include "fsc.h"
#include "tinycc.h"
#include "vgag.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25


int cursorX = 0;
int cursorY = 0;

uint8_t color = 0x0F;

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_INFO_FRAMEBUFFER_INFO 0x00001000

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t color_info[6];
} __attribute__((packed));

static uint16_t vga_shadow[VGA_WIDTH * VGA_HEIGHT];
uint16_t* vga = vga_shadow;

static uint8_t* framebuffer = 0;
static uint32_t framebuffer_pitch = 0;
static uint32_t framebuffer_width = 0;
static uint32_t framebuffer_height = 0;
static uint8_t framebuffer_bpp = 0;
static bool framebuffer_ready = false;

static const uint8_t font5x7[][7] = {
    ['0']={0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, ['1']={0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    ['2']={0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, ['3']={0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
    ['4']={0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, ['5']={0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},
    ['6']={0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, ['7']={0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    ['8']={0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, ['9']={0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    ['A']={0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, ['B']={0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    ['C']={0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, ['D']={0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    ['E']={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, ['F']={0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    ['G']={0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, ['H']={0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    ['I']={0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, ['J']={0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
    ['K']={0x11,0x12,0x14,0x18,0x14,0x12,0x11}, ['L']={0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    ['M']={0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, ['N']={0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    ['O']={0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, ['P']={0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    ['Q']={0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, ['R']={0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    ['S']={0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, ['T']={0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    ['U']={0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, ['V']={0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    ['W']={0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, ['X']={0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    ['Y']={0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, ['Z']={0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    [' ']={0,0,0,0,0,0,0}, ['.']={0,0,0,0,0,0x0C,0x0C}, [':']={0,0x0C,0x0C,0,0x0C,0x0C,0},
    ['-']={0,0,0,0x1F,0,0,0}, ['_']={0,0,0,0,0,0,0x1F}, ['+']={0,0x04,0x04,0x1F,0x04,0x04,0},
    ['*']={0,0x15,0x0E,0x1F,0x0E,0x15,0}, ['=']={0,0,0x1F,0,0x1F,0,0}, ['/']={0x01,0x02,0x02,0x04,0x08,0x08,0x10},
    ['\\']={0x10,0x08,0x08,0x04,0x02,0x02,0x01}, ['>']={0x10,0x08,0x04,0x02,0x04,0x08,0x10}, ['<']={0x01,0x02,0x04,0x08,0x04,0x02,0x01},
    ['!']={0x04,0x04,0x04,0x04,0x04,0,0x04}, ['?']={0x0E,0x11,0x01,0x02,0x04,0,0x04}, ['|']={0x04,0x04,0x04,0,0x04,0x04,0x04},
    ['[']={0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, [']']={0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, ['(']={0x02,0x04,0x08,0x08,0x08,0x04,0x02}, [')']={0x08,0x04,0x02,0x02,0x02,0x04,0x08},
};

static uint32_t ega_to_rgb(uint8_t ega) {
    static const uint32_t colors[16] = {0x000000,0x0000AA,0x00AA00,0x00AAAA,0xAA0000,0xAA00AA,0xAA5500,0xAAAAAA,0x555555,0x5555FF,0x55FF55,0x55FFFF,0xFF5555,0xFF55FF,0xFFFF55,0xFFFFFF};
    return colors[ega & 0x0F];
}

static void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t rgb) {
    if (!framebuffer_ready || x >= framebuffer_width || y >= framebuffer_height) return;
    uint8_t* pixel = framebuffer + y * framebuffer_pitch + x * (framebuffer_bpp / 8);
    pixel[0] = rgb & 0xFF;
    pixel[1] = (rgb >> 8) & 0xFF;
    pixel[2] = (rgb >> 16) & 0xFF;
    if (framebuffer_bpp == 32) pixel[3] = 0;
}

static void framebuffer_fill_cell(int cell_x, int cell_y, uint8_t cell_color) {
    uint32_t bg = ega_to_rgb(cell_color >> 4);
    for (uint32_t y = 0; y < 16; y++)
        for (uint32_t x = 0; x < 8; x++)
            framebuffer_put_pixel(cell_x * 8 + x, cell_y * 16 + y, bg);
}

static void framebuffer_draw_char(int cell_x, int cell_y, unsigned char c, uint8_t cell_color) {
    if (!framebuffer_ready) return;
    if (c >= 'a' && c <= 'z') c -= 32;
    framebuffer_fill_cell(cell_x, cell_y, cell_color);
    const uint8_t* glyph = font5x7[c];
    uint32_t fg = ega_to_rgb(cell_color & 0x0F);
    for (uint32_t row = 0; row < 7; row++) {
        for (uint32_t col = 0; col < 5; col++) {
            if (glyph[row] & (1 << (4 - col))) {
                uint32_t px = cell_x * 8 + 1 + col;
                uint32_t py = cell_y * 16 + 3 + row * 2;
                framebuffer_put_pixel(px, py, fg);
                framebuffer_put_pixel(px, py + 1, fg);
            }
        }
    }
}


void video_sync_cell(int x, int y) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;
    uint16_t cell = vga_shadow[y * VGA_WIDTH + x];
    framebuffer_draw_char(x, y, cell & 0xFF, cell >> 8);
}

void video_flush_console(void) {
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            video_sync_cell(x, y);
}

static void console_write_cell(int x, int y, unsigned char c, uint8_t cell_color) {
    uint16_t value = ((uint16_t)cell_color << 8) | c;
    vga_shadow[y * VGA_WIDTH + x] = value;
    ((volatile uint16_t*)0xB8000)[y * VGA_WIDTH + x] = value;
    framebuffer_draw_char(x, y, c, cell_color);
}

static void video_init(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC || !multiboot_info_addr) return;
    const struct multiboot_info* mbi = (const struct multiboot_info*)multiboot_info_addr;
    if (!(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) || mbi->framebuffer_type != 1) return;
    if (mbi->framebuffer_bpp != 24 && mbi->framebuffer_bpp != 32) return;
    framebuffer = (uint8_t*)(uint32_t)mbi->framebuffer_addr;
    framebuffer_pitch = mbi->framebuffer_pitch;
    framebuffer_width = mbi->framebuffer_width;
    framebuffer_height = mbi->framebuffer_height;
    framebuffer_bpp = mbi->framebuffer_bpp;
    framebuffer_ready = framebuffer != 0;
}


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

void update_cursor() {
    uint16_t pos = cursorY * VGA_WIDTH + cursorX;

    outb(0x3D4, 0x0F);           
    outb(0x3D5, (uint8_t)(pos & 0xFF));

    outb(0x3D4, 0x0E);             
    outb(0x3D5, (uint8_t)(pos >> 8));
}

void enable_cursor(uint8_t start, uint8_t end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | start);

    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | end);
}

// Text

void scroll() {
    for (int y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            console_write_cell(x, y, vga_shadow[(y + 1) * VGA_WIDTH + x] & 0xFF, vga_shadow[(y + 1) * VGA_WIDTH + x] >> 8);
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++)
    {
        console_write_cell(x, VGA_HEIGHT - 1, ' ', color);
    }
}

void putchar(unsigned char c) {
    if (c == '\n')
    {
        cursorX = 0;
        cursorY++;
    }
    else
    {
        console_write_cell(cursorX, cursorY, c, color);
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

    update_cursor();
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
            console_write_cell(x, y, ' ', color);

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

void printmult(unsigned char c, int l) {
    int i = 0;
    while (i < l) {
        putchar(c);
        i++;
    }
}

// Color text

void putcharc(unsigned char c, uint8_t color) {
    if (c == '\n')
    {
        cursorX = 0;
        cursorY++;
    }
    else
    {
        console_write_cell(cursorX, cursorY, c, color);
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

void printc(const char* str, uint8_t color) {
    for (int i = 0; str[i] != 0; i++)
        putcharc(str[i], color);
}

void printlnc(const char* str, uint8_t color) {
    printc(str, color);
    putchar('\n');
}

void printlnm(const char* str) { //Molecular logo
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '*') {
            putcharc('*', 0x07);
        }
        else {
            putcharc(str[i], 0x0F); 
        }
    }
    putchar('\n');
}

void printmultc(unsigned char c, int l, uint8_t color) {
    int i = 0;
    while (i < l) {
        putcharc(c, color);
        i++;
    }
}

void printintc(int num, uint8_t color) {
    char buf[16];
    int i = 0;

    if (num == 0)
    {
        putcharc('0', color);
        return;
    }

    while (num > 0)
    {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i--)
        putcharc(buf[i], color);
}

// IO

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

void printfloat(float num) {
    if (num < 0) {
        putchar('-');
        num = -num;
    }

    int int_part = (int)num;
    printint(int_part);

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

    putchar('.');

    for (int j = 0; j < i; j++) {
        putchar(buf[j]);
    }
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
    print(" ");
    putchar(0x0E);
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

    if (scancode == 0xE0) {
        while (!(inb(0x64) & 1));
        uint8_t extended = inb(0x60);

        switch (extended) {
            case 0x48: return 140; // Up
            case 0x50: return 141; // Down
            case 0x4B: return 142; // Left
            case 0x4D: return 143; // Right
        }

        return 0;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return 0;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return 0;
    }

    if (scancode & 0x80)
        return 0;

    switch (scancode) {
        case 0x3B: return 128;
        case 0x3C: return 129; 
        case 0x3D: return 130;
        case 0x3E: return 131; 
    }

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
    if (minute < 10) printint(0);
    printint(minute); putchar(':');
    if (second < 10) printint(0);
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
        println("test: Test an in-dev feature          |  ");
        println("                                      |");
        println("ls : List current directory           |  pwd : Show current path");
        println("cd : Change current directory         |  mkdir : Create directory");
        println("rmdir : Remove empty directory        |  mkf : Make a new text file");
        println("read : Read a file in current dir     |  rmf : Delete file in current dir");
        println("                                      |");
        println("reboot : Reboots system               |  shutdown : Shuts down system");
        println("reset : Resets system completely      |  vgag : Test the VGAG mode!");
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
    else if (strscmp(cmd_buffer, "vgag", 4)) vgag_run();
    else if (strscmp(cmd_buffer, "test", 4)) vgag_about();
    else if (strscmp(cmd_buffer, "settings login", 14)) {
        vfs_delete_file("0:\\data\\password.ini");
        vfs_delete_file("0:\\data\\username.ini");
        //clear_screen();
        handle_login();
    }
    else if (strscmp(cmd_buffer, "settings", 8)) {
        println("Settings:");
        println("settings : This page!");
        println("settings login : Reset login information");
        println("settings load : Reset default load settings");
    }
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

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    video_init(multiboot_magic, multiboot_info_addr);
    clear_screen();

    vfs_init();
    enable_cursor(14, 15);

    clear_screen();
    println(" ");

    printlnm("                  *                       ");
    printlnm("                   *                      ");
    printlnm("                   *+=----        **      ");
    printlnm("                   =-------      *        ");
    printlnm("    **       *****+--------+*****         ");
    printlnm("     *      **    --------=+     *        ");
    printlnm("      *++++**      ===--=++       **      ");
    printlnm("     +=-----=      =-::::-=               ");
    printlnm("    *=-------+*++++-::::::-+*****       **");
    printlnm("    +--------======-::::::-=    **      * ");
    printlnm("      -----==========-::--=      **   **  ");
    printlnm("      **   ++========----==      ++====++ ");
    printlnm("    **      ++======------==    +=======+*");
    printlnm("    *        **++++-------==-::-=========*");
    printlnm("                   ==------:::::-======== ");
    printlnm("                    *++++=-::::::-++++**  ");
    printlnm("                          --::::-=        ");
    printlnm("                           +====*         ");
    printlnm("                         **      *        ");
    printlnm("                        ***      ***      ");
    printlnm("                          *               ");
    printlnm("                           **             ");

    //handle_login();
    //beep(660, 500);
    //beep(590, 500);
    ind_login();

    clear_screen();

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

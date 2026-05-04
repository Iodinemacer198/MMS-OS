#include "fs.h"
#include "ata.h"

extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern void reboot();
extern void putchar(unsigned char c);
extern char get_key();
extern void sleep();
extern void printc(const char* str, uint8_t color);
extern void printmult(unsigned char c, int l);
extern void dputcharc(unsigned char c, uint8_t color);
extern void dprintintc(int num, uint8_t color);
extern void dprintc(const char* str, uint8_t color);
extern void cd(const char* path);
extern void dprintmultc(unsigned char c, int l, uint8_t color);

extern int dcX;
extern int dcY;

extern int cursorX;

#define SECTOR_SIZE 512

/* FAT16 layout starting at LBA 1 on the raw drive. */
#define FS_BASE_LBA 1
#define FS_TOTAL_SECTORS 2097152 /* 1 GiB raw disk from run.sh */
#define FS_RESERVED_SECTORS 1
#define FS_NUM_FATS 2
#define FS_ROOT_ENTRIES 512
#define FS_ROOT_DIR_SECTORS ((FS_ROOT_ENTRIES * 32 + (SECTOR_SIZE - 1)) / SECTOR_SIZE)
#define FS_SECTORS_PER_FAT 256
#define FS_SECTORS_PER_CLUSTER 32

#define FAT16_EOC 0xFFFF
#define FAT16_FREE 0x0000

#define DATA_START_LBA (FS_BASE_LBA + FS_RESERVED_SECTORS + (FS_NUM_FATS * FS_SECTORS_PER_FAT) + FS_ROOT_DIR_SECTORS)
#define ROOT_DIR_LBA (FS_BASE_LBA + FS_RESERVED_SECTORS + (FS_NUM_FATS * FS_SECTORS_PER_FAT))
#define TOTAL_CLUSTERS ((FS_TOTAL_SECTORS - FS_RESERVED_SECTORS - (FS_NUM_FATS * FS_SECTORS_PER_FAT) - FS_ROOT_DIR_SECTORS) / FS_SECTORS_PER_CLUSTER)
#define FAT_ENTRY_COUNT (TOTAL_CLUSTERS + 2)

#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20

#define ROOT_CLUSTER 0

#define MAX_PATH_PARTS 16
#define MAX_PATH_LEN 64
#define FILE_BUFFER_LIMIT (SECTOR_SIZE * FS_SECTORS_PER_CLUSTER * 32)

typedef struct __attribute__((packed)) {
    uint8_t jump[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
    uint8_t boot_code[448];
    uint16_t signature;
} FatBootSector;

typedef struct __attribute__((packed)) {
    char name[11];
    uint8_t attr;
    uint8_t nt_reserved;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} FatDirEntry;

static uint16_t fat[FAT_ENTRY_COUNT];
static uint8_t sector_buffer[SECTOR_SIZE];

static char cwd_path[MAX_PATH_LEN] = "0:\\";
static uint16_t cwd_cluster = ROOT_CLUSTER;

static const char* default_demo_source =
    "int main() {\n"
    "println(\"Hello, World!\");\n"
    "int answer = 2 + 3 * 4;\n"
    "printint(answer);\n"
    "println(\"\");\n"
    "return answer;\n"
    "}";

static const char* user_demo2 = 
    "void main() {\n"
    "vgag_box();\n"
    "dprintc(\"H \", 21, 8, 31);\n"
    "}";

/*
static const char* user_demo2 = 
    "void main() {\n"
    "vgag_box();\n"
    "dputcharc(21, 8, 'H', 31); dputcharc(22, 8, ' ', 31);\n"
    "dputcharc(21, 9, 'L', 159); dputcharc(22, 9, 'i', 159);\n"
    "dputcharc(21, 10, 'N', 159); dputcharc(22, 10, 'a', 159);\n"
    "dputcharc(21, 11, 'K', 159); dputcharc(22, 11, ' ', 159);\n"
    "dputcharc(21, 12, 'R', 159); dputcharc(22, 12, 'b', 159);\n"
    "dputcharc(21, 13, 'C', 159); dputcharc(22, 13, 's', 159);\n"
    "dputcharc(21, 14, 'F', 159); dputcharc(22, 14, 'r', 159);\n" // end g1
    "dputcharc(23, 9, 'B', 207); dputcharc(24, 9, 'e', 207);\n"
    "dputcharc(23, 10, 'M', 207); dputcharc(24, 10, 'g', 207);\n"
    "dputcharc(23, 11, 'C', 207); dputcharc(24, 11, 'a', 207);\n"
    "dputcharc(23, 12, 'S', 207); dputcharc(24, 12, 'r', 207);\n"
    "dputcharc(23, 13, 'B', 207); dputcharc(24, 13, 'a', 207);\n"
    "dputcharc(23, 14, 'R', 207); dputcharc(24, 14, 'a', 207);\n" // end g2
    "dputcharc(25, 11, 'S', 95); dputcharc(26, 11, 'c', 95);\n"
    "dputcharc(25, 12, 'Y', 95); dputcharc(26, 12, ' ', 95);\n"
    "dputcharc(25, 13, 'L', 177); dputcharc(26, 13, 'a', 177);\n"
    "dputcharc(25, 14, 'A', 111); dputcharc(26, 14, 'c', 111);\n" // end g4
    "dputcharc(27, 11, 'T', 95); dputcharc(28, 11, 'i', 95);\n" 
    "dputcharc(27, 12, 'Z', 95); dputcharc(28, 12, 'r', 95);\n"
    "dputcharc(27, 13, 'H', 95); dputcharc(28, 13, 'f', 95);\n"
    "dputcharc(27, 14, 'R', 95); dputcharc(28, 14, 'f', 95);\n" // end g5
    "dputcharc(29, 11, 'V', 95); dputcharc(30, 11, ' ', 95);\n"
    "dputcharc(29, 12, 'N', 95); dputcharc(30, 12, 'b', 95);\n"
    "dputcharc(29, 13, 'T', 95); dputcharc(30, 13, 'a', 95);\n"
    "dputcharc(29, 14, 'D', 95); dputcharc(30, 14, 'b', 95);\n" // end g6
    "dputcharc(31, 11, 'C', 95); dputcharc(32, 11, 'r', 95);\n"
    "dputcharc(31, 12, 'M', 95); dputcharc(32, 12, 'o', 95);\n"
    "dputcharc(31, 13, 'W', 95); dputcharc(32, 13, ' ', 95);\n"
    "dputcharc(31, 14, 'S', 95); dputcharc(32, 14, 'g', 95);\n" // end g7
    "dputcharc(33, 11, 'M', 95); dputcharc(34, 11, 'n', 95);\n"
    "dputcharc(33, 12, 'T', 95); dputcharc(34, 12, 'c', 95);\n"
    "dputcharc(33, 13, 'R', 95); dputcharc(34, 13, 'e', 95);\n"
    "dputcharc(33, 14, 'B', 95); dputcharc(34, 14, 'h', 95);\n" // end g8
    "dputcharc(35, 11, 'F', 95); dputcharc(36, 11, 'e', 95);\n"
    "dputcharc(35, 12, 'R', 95); dputcharc(36, 12, 'u', 95);\n"
    "dputcharc(35, 13, 'O', 95); dputcharc(36, 13, 's', 95);\n"
    "dputcharc(35, 14, 'H', 95); dputcharc(36, 14, 's', 95);\n" // end g9
    "dputcharc(37, 11, 'C', 95); dputcharc(38, 11, 'o', 95);\n"
    "dputcharc(37, 12, 'R', 95); dputcharc(38, 12, 'h', 95);\n"
    "dputcharc(37, 13, 'I', 95); dputcharc(38, 13, 'r', 95);\n"
    "dputcharc(37, 14, 'M', 143); dputcharc(38, 14, 't', 143);\n" // end g10
    "dputcharc(39, 11, 'N', 95); dputcharc(40, 11, 'i', 95);\n"
    "dputcharc(39, 12, 'P', 95); dputcharc(40, 12, 'd', 95);\n"
    "dputcharc(39, 13, 'P', 95); dputcharc(40, 13, 't', 95);\n"
    "dputcharc(39, 14, 'D', 143); dputcharc(40, 14, 's', 143);\n" // end g11
    "dputcharc(41, 11, 'C', 95); dputcharc(42, 11, 'u', 95);\n"
    "dputcharc(41, 12, 'A', 95); dputcharc(42, 12, 'g', 95);\n"
    "dputcharc(41, 13, 'A', 95); dputcharc(42, 13, 'u', 95);\n"
    "dputcharc(41, 14, 'R', 143); dputcharc(42, 14, 'g', 143);\n" // end g12
    "dputcharc(43, 11, 'Z', 95); dputcharc(44, 11, 'n', 95);\n"
    "dputcharc(43, 12, 'C', 95); dputcharc(44, 12, 'd', 95);\n"
    "dputcharc(43, 13, 'H', 95); dputcharc(44, 13, 'g', 95);\n"
    "dputcharc(43, 14, 'C',143); dputcharc(44, 14, 'n', 143);\n" // end g13
    "dputcharc(45, 9, 'B', 224); dputcharc(46, 9, ' ', 224);\n"
    "dputcharc(45, 10, 'A', 160); dputcharc(46, 10, 'l', 160);\n"
    "dputcharc(45, 11, 'G', 160); dputcharc(46, 11, 'a', 160);\n"
    "dputcharc(45, 12, 'I', 160); dputcharc(46, 12, 'n', 160);\n"
    "dputcharc(45, 13, 'T', 160); dputcharc(46, 13, 'i', 160);\n"
    "dputcharc(45, 14, 'N', 143); dputcharc(46, 14, 'h', 143);\n" // end g14
    "dputcharc(47, 9, 'C', 31); dputcharc(48, 9, ' ', 31);\n"
    "dputcharc(47, 10, 'S', 224); dputcharc(48, 10, 'i', 224);\n"
    "dputcharc(47, 11, 'G', 224); dputcharc(48, 11, 'e', 224);\n"
    "dputcharc(47, 12, 'S', 160); dputcharc(48, 12, 'n', 160);\n"
    "dputcharc(47, 13, 'P', 160); dputcharc(48, 13, 'b', 160);\n"
    "dputcharc(47, 14, 'F', 143); dputcharc(48, 14, 'l', 143);\n" // end g15
    "dputcharc(49, 9, 'N', 31); dputcharc(50, 9, ' ', 31);\n"
    "dputcharc(49, 10, 'P', 31); dputcharc(50, 10, ' ', 31);\n"
    "dputcharc(49, 11, 'A', 224); dputcharc(50, 11, 's', 224);\n"
    "dputcharc(49, 12, 'S', 224); dputcharc(50, 12, 'b', 224);\n"
    "dputcharc(49, 13, 'B', 160); dputcharc(50, 13, 'i', 160);\n"
    "dputcharc(49, 14, 'M', 143); dputcharc(50, 14, 'c', 143);\n" // end g16
    "dputcharc(51, 9, 'O', 31); dputcharc(52, 9, ' ', 31);\n"
    "dputcharc(51, 10, 'S', 31); dputcharc(52, 10, ' ', 31);\n"
    "dputcharc(51, 11, 'S', 31); dputcharc(52, 11, 'e', 31);\n"
    "dputcharc(51, 12, 'T', 224); dputcharc(52, 12, 'e', 224);\n"
    "dputcharc(51, 13, 'P', 160); dputcharc(52, 13, 'o', 160);\n"
    "dputcharc(51, 14, 'L', 143); dputcharc(52, 14, 'v', 143);\n" // end g17
    "dputcharc(53, 9, 'F', 31); dputcharc(54, 9, ' ', 31);\n"
    "dputcharc(53, 10, 'C', 31); dputcharc(54, 10, 'l', 31);\n"
    "dputcharc(53, 11, 'B', 31); dputcharc(54, 11, 'r', 31);\n"
    "dputcharc(53, 12, 'I', 31); dputcharc(54, 12, ' ', 31);\n"
    "dputcharc(53, 13, 'A', 160); dputcharc(54, 13, 't', 160);\n"
    "dputcharc(53, 14, 'T', 143); dputcharc(54, 14, 's', 143);\n" // eng g18
    "dputcharc(55, 8, 'H', 79); dputcharc(56, 8, 'e', 79);\n"
    "dputcharc(55, 9, 'N', 79); dputcharc(56, 9, 'e', 79);\n"
    "dputcharc(55, 10, 'A', 79); dputcharc(56, 10, 'r', 79);\n"
    "dputcharc(55, 11, 'K', 79); dputcharc(56, 11, 'r', 79);\n"
    "dputcharc(55, 12, 'X', 79); dputcharc(56, 12, 'e', 79);\n"
    "dputcharc(55, 13, 'R', 79); dputcharc(56, 13, 'n', 79);\n"
    "dputcharc(55, 14, 'O', 143); dputcharc(56, 14, 'g', 143);\n" // end 619
    "dputcharc(27, 16, 'C', 177); dputcharc(28, 16, 'e', 177);\n" 
    "dputcharc(29, 16, 'P', 177); dputcharc(30, 16, 'r', 177);\n" 
    "dputcharc(31, 16, 'N', 177); dputcharc(32, 16, 'd', 177);\n" 
    "dputcharc(33, 16, 'P', 177); dputcharc(34, 16, 'm', 177);\n" 
    "dputcharc(35, 16, 'S', 177); dputcharc(36, 16, 'm', 177);\n" 
    "dputcharc(37, 16, 'E', 177); dputcharc(38, 16, 'u', 177);\n" 
    "dputcharc(39, 16, 'G', 177); dputcharc(40, 16, 'd', 177);\n" 
    "dputcharc(41, 16, 'T', 177); dputcharc(42, 16, 'b', 177);\n"
    "dputcharc(43, 16, 'D', 177); dputcharc(44, 16, 'y', 177);\n"  
    "dputcharc(45, 16, 'H', 177); dputcharc(46, 16, 'o', 177);\n" 
    "dputcharc(47, 16, 'E', 177); dputcharc(48, 16, 'r', 177);\n" 
    "dputcharc(49, 16, 'T', 177); dputcharc(50, 16, 'm', 177);\n" 
    "dputcharc(51, 16, 'Y', 177); dputcharc(52, 16, 'b', 177);\n" 
    "dputcharc(53, 16, 'L', 177); dputcharc(54, 16, 'u', 177);\n" // end actinds
    "}";
*/

static int str_len(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static void str_cpy(char* dest, const char* src) {
    int i = 0;
    while (src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static bool streq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static bool str_starts_with(const char* str, const char* pref) {
    int i = 0;
    while (pref[i]) {
        if (str[i] != pref[i]) return false;
        i++;
    }
    return true;
}

/*
static char to_upper_ascii(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - ('a' - 'A'));
    return c;
}
*/

static void mem_zero(uint8_t* ptr, int len) {
    for (int i = 0; i < len; i++) ptr[i] = 0;
}

static bool is_path_sep(char c) {
    return c == '\\' || c == '/';
}

static uint32_t cluster_to_lba(uint16_t cluster) {
    return DATA_START_LBA + ((uint32_t)(cluster - 2) * FS_SECTORS_PER_CLUSTER);
}

static void read_sector(uint32_t lba, uint8_t* out) {
    ata_read_sector(lba, out);
}

static void write_sector(uint32_t lba, const uint8_t* in) {
    for (int i = 0; i < SECTOR_SIZE; i++) sector_buffer[i] = in[i];
    ata_write_sector(lba, sector_buffer);
}

static void fat_flush() {
    const uint8_t* ptr = (const uint8_t*)fat;
    for (int copy = 0; copy < FS_NUM_FATS; copy++) {
        uint32_t fat_lba = FS_BASE_LBA + FS_RESERVED_SECTORS + (copy * FS_SECTORS_PER_FAT);
        for (int sec = 0; sec < FS_SECTORS_PER_FAT; sec++) {
            write_sector(fat_lba + sec, ptr + (sec * SECTOR_SIZE));
        }
    }
}

static void fat_load() {
    uint8_t* ptr = (uint8_t*)fat;
    uint32_t fat_lba = FS_BASE_LBA + FS_RESERVED_SECTORS;
    for (int sec = 0; sec < FS_SECTORS_PER_FAT; sec++) {
        read_sector(fat_lba + sec, sector_buffer);
        for (int i = 0; i < SECTOR_SIZE; i++) ptr[sec * SECTOR_SIZE + i] = sector_buffer[i];
    }
}

static uint16_t dir_entry_cluster(const FatDirEntry* e) {
    return e->first_cluster_low;
}

static void dir_entry_set_cluster(FatDirEntry* e, uint16_t cluster) {
    e->first_cluster_high = 0;
    e->first_cluster_low = cluster;
}

static bool parse_component(const char* path, int* pos, char* out, int out_cap) {
    int idx = 0;
    while (path[*pos] && is_path_sep(path[*pos])) (*pos)++;
    if (!path[*pos]) return false;

    while (path[*pos] && !is_path_sep(path[*pos])) {
        if (idx < out_cap - 1) out[idx++] = path[*pos];
        (*pos)++;
    }
    out[idx] = '\0';
    return idx > 0;
}

static int path_start(const char* path) {
    if (path[0] == '0' && path[1] == ':' && is_path_sep(path[2])) return 3;
    return 0;
}

static bool normalize_path(const char* path, char* out) {
    char parts[MAX_PATH_PARTS][16];
    int count = 0;

    if (!(path[0] == '0' && path[1] == ':' && is_path_sep(path[2]))) {
        int cp = 3;
        char cwd_part[16];
        while (parse_component(cwd_path, &cp, cwd_part, 16)) {
            if (count >= MAX_PATH_PARTS) return false;
            str_cpy(parts[count++], cwd_part);
        }
    }

    int p = path_start(path);
    char part[16];
    while (parse_component(path, &p, part, 16)) {
        if (streq(part, ".")) continue;
        if (streq(part, "..")) {
            if (count > 0) count--;
            continue;
        }
        if (count >= MAX_PATH_PARTS) return false;
        str_cpy(parts[count++], part);
    }

    int o = 0;
    out[o++] = '0';
    out[o++] = ':';
    out[o++] = '\\';
    for (int i = 0; i < count; i++) {
        int j = 0;
        while (parts[i][j] && o < (MAX_PATH_LEN - 1)) out[o++] = parts[i][j++];
        if (i < count - 1 && o < (MAX_PATH_LEN - 1)) out[o++] = '\\';
    }
    out[o] = '\0';
    return true;
}

static bool is_hidden_path(const char* absolute_path) {
    return streq(absolute_path, "0:\\data") || str_starts_with(absolute_path, "0:\\data\\");
}

static bool format_name_83(const char* component, char out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';

    int i = 0;
    int base_len = 0;
    while (component[i] && component[i] != '.') {
        if (base_len >= 8) return false;
        char c = component[i];
        if (c < 33 || c == '"' || c == '*' || c == '+' || c == ',' || c == '/' || c == ':' || c == ';' || c == '<' || c == '=' || c == '>' || c == '?' || c == '[' || c == '\\' || c == ']' || c == '|') {
            return false;
        }
        //out[base_len++] = to_upper_ascii(c);
        out[base_len++] = c;
        i++;
    }

    if (base_len == 0) return false;

    if (component[i] == '.') {
        i++;
        int ext_len = 0;
        while (component[i]) {
            if (ext_len >= 3) return false;
            char c = component[i];
            if (c < 33 || c == '"' || c == '*' || c == '+' || c == ',' || c == '/' || c == ':' || c == ';' || c == '<' || c == '=' || c == '>' || c == '?' || c == '[' || c == '\\' || c == ']' || c == '|') {
                return false;
            }
            //out[8 + ext_len++] = to_upper_ascii(c);
            out[8 + ext_len++] = c;
            i++;
        }
    }

    return true;
}

static void name83_to_display(const char in[11], char* out) {
    int p = 0;
    for (int i = 0; i < 8 && in[i] != ' '; i++) out[p++] = in[i];
    if (in[8] != ' ') {
        out[p++] = '.';
        for (int i = 8; i < 11 && in[i] != ' '; i++) out[p++] = in[i];
    }
    out[p] = '\0';
}

static bool name83_equal(const char a[11], const char b[11]) {
    for (int i = 0; i < 11; i++) if (a[i] != b[i]) return false;
    return true;
}

static bool read_dir_entry(uint16_t dir_cluster, int index, FatDirEntry* out) {
    uint32_t dir_lba;
    int max_entries;

    if (dir_cluster == ROOT_CLUSTER) {
        dir_lba = ROOT_DIR_LBA;
        max_entries = FS_ROOT_ENTRIES;
        if (index < 0 || index >= max_entries) return false;
        int sec = index / 16;
        int off = (index % 16) * 32;
        read_sector(dir_lba + sec, sector_buffer);
        for (int i = 0; i < 32; i++) ((uint8_t*)out)[i] = sector_buffer[off + i];
        return true;
    }

    if (dir_cluster < 2 || dir_cluster >= FAT_ENTRY_COUNT) return false;
    max_entries = (SECTOR_SIZE * FS_SECTORS_PER_CLUSTER) / 32;
    if (index < 0 || index >= max_entries) return false;

    int sec = index / 16;
    int off = (index % 16) * 32;
    read_sector(cluster_to_lba(dir_cluster) + sec, sector_buffer);
    for (int i = 0; i < 32; i++) ((uint8_t*)out)[i] = sector_buffer[off + i];
    return true;
}

static bool write_dir_entry(uint16_t dir_cluster, int index, const FatDirEntry* in) {
    uint32_t dir_lba;
    int max_entries;

    if (dir_cluster == ROOT_CLUSTER) {
        dir_lba = ROOT_DIR_LBA;
        max_entries = FS_ROOT_ENTRIES;
        if (index < 0 || index >= max_entries) return false;
        int sec = index / 16;
        int off = (index % 16) * 32;
        read_sector(dir_lba + sec, sector_buffer);
        for (int i = 0; i < 32; i++) sector_buffer[off + i] = ((const uint8_t*)in)[i];
        ata_write_sector(dir_lba + sec, sector_buffer);
        return true;
    }

    if (dir_cluster < 2 || dir_cluster >= FAT_ENTRY_COUNT) return false;
    max_entries = (SECTOR_SIZE * FS_SECTORS_PER_CLUSTER) / 32;
    if (index < 0 || index >= max_entries) return false;

    int sec = index / 16;
    int off = (index % 16) * 32;
    read_sector(cluster_to_lba(dir_cluster) + sec, sector_buffer);
    for (int i = 0; i < 32; i++) sector_buffer[off + i] = ((const uint8_t*)in)[i];
    ata_write_sector(cluster_to_lba(dir_cluster) + sec, sector_buffer);
    return true;
}

static bool find_in_directory(uint16_t dir_cluster, const char name83[11], FatDirEntry* out, int* out_index) {
    int max_entries = (dir_cluster == ROOT_CLUSTER) ? FS_ROOT_ENTRIES : ((SECTOR_SIZE * FS_SECTORS_PER_CLUSTER) / 32);

    for (int i = 0; i < max_entries; i++) {
        FatDirEntry e;
        if (!read_dir_entry(dir_cluster, i, &e)) return false;

        if ((uint8_t)e.name[0] == 0x00) continue;
        if ((uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr == 0x0F) continue;

        if (name83_equal(e.name, name83)) {
            if (out) *out = e;
            if (out_index) *out_index = i;
            return true;
        }
    }

    return false;
}

static int find_free_directory_slot(uint16_t dir_cluster) {
    int max_entries = (dir_cluster == ROOT_CLUSTER) ? FS_ROOT_ENTRIES : ((SECTOR_SIZE * FS_SECTORS_PER_CLUSTER) / 32);
    for (int i = 0; i < max_entries; i++) {
        FatDirEntry e;
        if (!read_dir_entry(dir_cluster, i, &e)) return -1;
        if ((uint8_t)e.name[0] == 0x00 || (uint8_t)e.name[0] == 0xE5) return i;
    }
    return -1;
}

static uint16_t fat_alloc_cluster() {
    for (uint16_t c = 2; c < FAT_ENTRY_COUNT; c++) {
        if (fat[c] == FAT16_FREE) {
            fat[c] = FAT16_EOC;
            mem_zero(sector_buffer, SECTOR_SIZE);
            uint32_t lba = cluster_to_lba(c);
            for (int sec = 0; sec < FS_SECTORS_PER_CLUSTER; sec++) {
                ata_write_sector(lba + sec, sector_buffer);
            }
            return c;
        }
    }
    return FAT16_FREE;
}

static void fat_free_chain(uint16_t first) {
    uint16_t cur = first;
    while (cur >= 2 && cur < FAT_ENTRY_COUNT && fat[cur] != FAT16_FREE) {
        uint16_t next = fat[cur];
        fat[cur] = FAT16_FREE;
        if (next == FAT16_EOC) break;
        cur = next;
    }
}

static uint16_t fat_alloc_chain(int clusters_needed) {
    uint16_t first = FAT16_FREE;
    uint16_t prev = FAT16_FREE;

    for (int i = 0; i < clusters_needed; i++) {
        uint16_t c = fat_alloc_cluster();
        if (c == FAT16_FREE) {
            if (first != FAT16_FREE) fat_free_chain(first);
            return FAT16_FREE;
        }

        if (first == FAT16_FREE) first = c;
        if (prev != FAT16_FREE) fat[prev] = c;
        prev = c;
    }

    if (prev != FAT16_FREE) fat[prev] = FAT16_EOC;
    return first;
}

static bool resolve_path_to_entry(const char* abs_path, uint16_t* parent_cluster, FatDirEntry* entry, int* entry_index, char leaf_83[11]) {
    int p = path_start(abs_path);
    uint16_t current = ROOT_CLUSTER;
    char part[16];
    char next[16];

    if (!parse_component(abs_path, &p, part, 16)) {
        if (parent_cluster) *parent_cluster = ROOT_CLUSTER;
        return true;
    }

    while (1) {
        int saved = p;
        bool has_next = parse_component(abs_path, &saved, next, 16);

        char name83[11];
        if (!format_name_83(part, name83)) return false;

        if (!has_next) {
            if (parent_cluster) *parent_cluster = current;
            if (leaf_83) {
                for (int i = 0; i < 11; i++) leaf_83[i] = name83[i];
            }
            if (entry || entry_index) {
                if (!find_in_directory(current, name83, entry, entry_index)) return false;
            }
            return true;
        }

        FatDirEntry dirent;
        if (!find_in_directory(current, name83, &dirent, 0)) return false;
        if (!(dirent.attr & ATTR_DIRECTORY)) return false;

        current = dir_entry_cluster(&dirent);
        p = saved;
        str_cpy(part, next);
    }
}

static bool ensure_path_parent(const char* abs_path, uint16_t* parent_cluster, char leaf_83[11]) {
    int p = path_start(abs_path);
    uint16_t current = ROOT_CLUSTER;
    char part[16];
    char next[16];

    if (!parse_component(abs_path, &p, part, 16)) return false;

    while (1) {
        int saved = p;
        bool has_next = parse_component(abs_path, &saved, next, 16);

        char name83[11];
        if (!format_name_83(part, name83)) return false;

        if (!has_next) {
            *parent_cluster = current;
            for (int i = 0; i < 11; i++) leaf_83[i] = name83[i];
            return true;
        }

        FatDirEntry child;
        if (!find_in_directory(current, name83, &child, 0)) return false;
        if (!(child.attr & ATTR_DIRECTORY)) return false;

        current = dir_entry_cluster(&child);
        p = saved;
        str_cpy(part, next);
    }
}

static bool write_chain_data(uint16_t first_cluster, const char* data, int size) {
    uint16_t cur = first_cluster;
    int offset = 0;

    while (cur >= 2 && cur < FAT_ENTRY_COUNT) {
        uint32_t cluster_lba = cluster_to_lba(cur);
        for (int sec = 0; sec < FS_SECTORS_PER_CLUSTER; sec++) {
            for (int i = 0; i < SECTOR_SIZE; i++) {
                sector_buffer[i] = (offset < size) ? (uint8_t)data[offset++] : 0;
            }
            ata_write_sector(cluster_lba + sec, sector_buffer);
        }

        if (fat[cur] == FAT16_EOC) break;
        cur = fat[cur];
    }

    return true;
}

static bool read_chain_data(uint16_t first_cluster, char* out, int size) {
    uint16_t cur = first_cluster;
    int offset = 0;

    while (cur >= 2 && cur < FAT_ENTRY_COUNT && offset < size) {
        uint32_t cluster_lba = cluster_to_lba(cur);
        for (int sec = 0; sec < FS_SECTORS_PER_CLUSTER && offset < size; sec++) {
            ata_read_sector(cluster_lba + sec, sector_buffer);
            for (int i = 0; i < SECTOR_SIZE && offset < size; i++) out[offset++] = (char)sector_buffer[i];
        }

        if (fat[cur] == FAT16_EOC) break;
        cur = fat[cur];
    }

    out[size] = '\0';
    return true;
}

static bool directory_is_empty(uint16_t cluster) {
    int max_entries = (SECTOR_SIZE * FS_SECTORS_PER_CLUSTER) / 32;
    for (int i = 0; i < max_entries; i++) {
        FatDirEntry e;
        if (!read_dir_entry(cluster, i, &e)) return false;
        if ((uint8_t)e.name[0] == 0x00 || (uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr == 0x0F) continue;
        if (e.name[0] == '.' && e.name[1] == ' ') continue;
        if (e.name[0] == '.' && e.name[1] == '.' && e.name[2] == ' ') continue;
        return false;
    }
    return true;
}

static void fs_format_fat16() {
    FatBootSector bs;
    mem_zero((uint8_t*)&bs, sizeof(bs));

    bs.jump[0] = (uint8_t)0xEB;
    bs.jump[1] = (uint8_t)0x3C;
    bs.jump[2] = (uint8_t)0x90;
    bs.oem[0] = 'M'; bs.oem[1] = 'M'; bs.oem[2] = 'S'; bs.oem[3] = 'O'; bs.oem[4] = 'S';
    bs.bytes_per_sector = SECTOR_SIZE;
    bs.sectors_per_cluster = FS_SECTORS_PER_CLUSTER;
    bs.reserved_sector_count = FS_RESERVED_SECTORS;
    bs.num_fats = FS_NUM_FATS;
    bs.root_entry_count = FS_ROOT_ENTRIES;
    bs.total_sectors_16 = 0;
    bs.media = 0xF8;
    bs.fat_size_16 = FS_SECTORS_PER_FAT;
    bs.sectors_per_track = 63;
    bs.num_heads = 16;
    bs.hidden_sectors = 0;
    bs.total_sectors_32 = FS_TOTAL_SECTORS;
    bs.drive_number = 0x80;
    bs.boot_signature = 0x29;
    bs.volume_id = 0x4D4D5301;

    const char label[11] = {'M','M','S','-','O','S',' ',' ',' ',' ',' '};
    const char fstype[8] = {'F','A','T','1','6',' ',' ',' '};
    for (int i = 0; i < 11; i++) bs.volume_label[i] = label[i];
    for (int i = 0; i < 8; i++) bs.fs_type[i] = fstype[i];
    bs.signature = 0xAA55;

    write_sector(FS_BASE_LBA, (uint8_t*)&bs);

    mem_zero((uint8_t*)fat, sizeof(fat));
    fat[0] = 0xFFF8;
    fat[1] = FAT16_EOC;
    fat_flush();

    mem_zero(sector_buffer, SECTOR_SIZE);
    for (int i = 0; i < FS_ROOT_DIR_SECTORS; i++) ata_write_sector(ROOT_DIR_LBA + i, sector_buffer);
}

static void vfs_seed_defaults() {
    static char read_buffer[FILE_BUFFER_LIMIT + 1];

    vfs_make_dir("0:\\data");

    if (!vfs_read_file("0:\\test.txt", read_buffer)) {
        vfs_write_file("0:\\test.txt", "Hello, curious user!");
    }
    if (!vfs_read_file("0:\\music\\ode.md", read_buffer)) {
        vfs_make_dir("0:\\music");
        vfs_write_file("0:\\music\\ode.md", "370 400\n370 400\n392 400\n440 400\n440 400\n392 400\n370 400\n330 400\n294 400\n294 400\n330 400\n370 400\n370 600\n330 200\n330 800");
    }
    if (!vfs_read_file("0:\\programs\\demo.c", read_buffer) || !streq(read_buffer, default_demo_source)) {
        vfs_make_dir("0:\\programs");
        vfs_write_file("0:\\programs\\demo.c", default_demo_source);
    }
    if (!vfs_read_file("0:\\vgag\\periodic.c", read_buffer) || !streq(read_buffer, user_demo2)) {
        vfs_make_dir("0:\\vgag");
        vfs_write_file("0:\\vgag\\periodic.c", user_demo2);
    }
}

void vfs_init() {
    read_sector(FS_BASE_LBA, sector_buffer);

    FatBootSector* bs = (FatBootSector*)sector_buffer;
    bool valid = (bs->signature == 0xAA55) &&
                 (bs->bytes_per_sector == SECTOR_SIZE) &&
                 (bs->sectors_per_cluster == FS_SECTORS_PER_CLUSTER) &&
                 (bs->num_fats == FS_NUM_FATS) &&
                 (bs->fat_size_16 == FS_SECTORS_PER_FAT) &&
                 (bs->root_entry_count == FS_ROOT_ENTRIES) &&
                 (bs->total_sectors_32 == FS_TOTAL_SECTORS);

    if (!valid) {
        println("Formatting drive...");
        fs_format_fat16();
    } else {
        println("Disk mounted successfully.");
    }

    fat_load();
    str_cpy(cwd_path, "0:\\");
    cwd_cluster = ROOT_CLUSTER;

    vfs_seed_defaults();
}

bool vfs_make_dir(const char* path) {
    char abs[MAX_PATH_LEN];
    if (!normalize_path(path, abs)) return false;

    uint16_t parent;
    char leaf[11];
    if (!ensure_path_parent(abs, &parent, leaf)) return false;

    FatDirEntry existing;
    if (find_in_directory(parent, leaf, &existing, 0)) return (existing.attr & ATTR_DIRECTORY) != 0;

    int slot = find_free_directory_slot(parent);
    if (slot < 0) return false;

    uint16_t new_cluster = fat_alloc_cluster();
    if (new_cluster == FAT16_FREE) return false;

    FatDirEntry e;
    mem_zero((uint8_t*)&e, sizeof(e));
    for (int i = 0; i < 11; i++) e.name[i] = leaf[i];
    e.attr = ATTR_DIRECTORY;
    dir_entry_set_cluster(&e, new_cluster);
    e.file_size = 0;

    if (!write_dir_entry(parent, slot, &e)) {
        fat[new_cluster] = FAT16_FREE;
        fat_flush();
        return false;
    }

    mem_zero(sector_buffer, SECTOR_SIZE);
    FatDirEntry* items = (FatDirEntry*)sector_buffer;

    mem_zero((uint8_t*)&items[0], sizeof(FatDirEntry));
    items[0].name[0] = '.';
    for (int i = 1; i < 11; i++) items[0].name[i] = ' ';
    items[0].attr = ATTR_DIRECTORY;
    dir_entry_set_cluster(&items[0], new_cluster);

    mem_zero((uint8_t*)&items[1], sizeof(FatDirEntry));
    items[1].name[0] = '.';
    items[1].name[1] = '.';
    for (int i = 2; i < 11; i++) items[1].name[i] = ' ';
    items[1].attr = ATTR_DIRECTORY;
    dir_entry_set_cluster(&items[1], parent == ROOT_CLUSTER ? 0 : parent);

    uint32_t dir_lba = cluster_to_lba(new_cluster);
    ata_write_sector(dir_lba, sector_buffer);
    mem_zero(sector_buffer, SECTOR_SIZE);
    for (int sec = 1; sec < FS_SECTORS_PER_CLUSTER; sec++) {
        ata_write_sector(dir_lba + sec, sector_buffer);
    }
    fat_flush();

    return true;
}

bool vfs_remove_dir(const char* path) {
    char abs[MAX_PATH_LEN];
    if (!normalize_path(path, abs)) return false;
    if (streq(abs, "0:\\") || is_hidden_path(abs)) return false;

    uint16_t parent;
    FatDirEntry e;
    int idx;
    if (!resolve_path_to_entry(abs, &parent, &e, &idx, 0)) return false;
    if (!(e.attr & ATTR_DIRECTORY)) return false;

    uint16_t cluster = dir_entry_cluster(&e);
    if (cluster < 2) return false;
    if (!directory_is_empty(cluster)) return false;

    fat_free_chain(cluster);
    fat_flush();

    FatDirEntry tomb;
    mem_zero((uint8_t*)&tomb, sizeof(tomb));
    tomb.name[0] = (char)0xE5;
    return write_dir_entry(parent, idx, &tomb);
}

bool vfs_change_dir(const char* path) {
    char abs[MAX_PATH_LEN];
    if (!normalize_path(path, abs)) return false;
    if (is_hidden_path(abs)) return false;

    if (streq(abs, "0:\\")) {
        str_cpy(cwd_path, abs);
        cwd_cluster = ROOT_CLUSTER;
        return true;
    }

    FatDirEntry e;
    if (!resolve_path_to_entry(abs, 0, &e, 0, 0)) return false;
    if (!(e.attr & ATTR_DIRECTORY)) return false;

    cwd_cluster = dir_entry_cluster(&e);
    str_cpy(cwd_path, abs);
    return true;
}

void vfs_get_cwd(char* out) {
    str_cpy(out, cwd_path);
}

bool vfs_write_file(const char* path, const char* data) {
    int len = str_len(data);
    if (len > FILE_BUFFER_LIMIT) return false;

    char abs[MAX_PATH_LEN];
    if (!normalize_path(path, abs)) return false;

    uint16_t parent;
    char leaf[11];
    if (!ensure_path_parent(abs, &parent, leaf)) return false;

    FatDirEntry e;
    int idx = -1;
    bool exists = find_in_directory(parent, leaf, &e, &idx);
    if (exists && (e.attr & ATTR_DIRECTORY)) return false;

    uint16_t first = FAT16_FREE;
    if (len > 0) {
        int cluster_bytes = SECTOR_SIZE * FS_SECTORS_PER_CLUSTER;
        int clusters = (len + cluster_bytes - 1) / cluster_bytes;
        first = fat_alloc_chain(clusters);
        if (first == FAT16_FREE) return false;
        write_chain_data(first, data, len);
    }

    if (exists) {
        if (dir_entry_cluster(&e) >= 2) fat_free_chain(dir_entry_cluster(&e));
    } else {
        idx = find_free_directory_slot(parent);
        if (idx < 0) {
            if (first >= 2) fat_free_chain(first);
            return false;
        }
        mem_zero((uint8_t*)&e, sizeof(e));
        for (int i = 0; i < 11; i++) e.name[i] = leaf[i];
        e.attr = ATTR_ARCHIVE;
    }

    dir_entry_set_cluster(&e, first);
    e.file_size = (uint32_t)len;
    if (!write_dir_entry(parent, idx, &e)) {
        if (first >= 2) fat_free_chain(first);
        fat_flush();
        return false;
    }

    fat_flush();
    return true;
}

bool vfs_read_file(const char* path, char* buffer_out) {
    char abs[MAX_PATH_LEN];
    if (!normalize_path(path, abs)) return false;

    FatDirEntry e;
    if (!resolve_path_to_entry(abs, 0, &e, 0, 0)) return false;
    if (e.attr & ATTR_DIRECTORY) return false;
    if (e.file_size > FILE_BUFFER_LIMIT) return false;

    if (e.file_size == 0 || dir_entry_cluster(&e) < 2) {
        buffer_out[0] = '\0';
        return true;
    }

    return read_chain_data(dir_entry_cluster(&e), buffer_out, (int)e.file_size);
}

void vfs_list_current_dir() {
    printmult(0xCD, 6); print(" "); print(cwd_path); print(" "); printmult(0xCD, 6); putchar('\n');
    bool empty = true;

    int max_entries = (cwd_cluster == ROOT_CLUSTER) ? FS_ROOT_ENTRIES : ((SECTOR_SIZE * FS_SECTORS_PER_CLUSTER) / 32);

    for (int i = 0; i < max_entries; i++) {
        FatDirEntry e;
        if (!read_dir_entry(cwd_cluster, i, &e)) break;
        if ((uint8_t)e.name[0] == 0x00 || (uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr == 0x0F) continue;

        char disp[20];
        name83_to_display(e.name, disp);

        if (streq(disp, ".") || streq(disp, "..")) continue;

        char full[MAX_PATH_LEN];
        if (streq(cwd_path, "0:\\")) {
            str_cpy(full, "0:\\");
            str_cpy(full + 3, disp);
        } else {
            str_cpy(full, cwd_path);
            int p = str_len(full);
            full[p++] = '\\';
            str_cpy(full + p, disp);
        }

        if (is_hidden_path(full)) continue;

        if (e.attr & ATTR_DIRECTORY) print("[DIR] ");
        else print("[FILE] ");
        print(disp);
        if (!(e.attr & ATTR_DIRECTORY)) {
            print(" (");
            printint((int)e.file_size);
            print(" bytes)");
        }
        putchar('\n');
        empty = false;
    }

    if (empty) println("(empty)");

    printmult(0xCD, 14);
    int i = 14;
    int p = 0;
    while (i < 128 && cwd_path[p] != '\0') {
        putchar(0xCD);
        i++; p++;
    }
    putchar('\n');
}

void vgag_list_current_dir() {
    cd("0:\\vgag");
    dcX = 11; dcY = 6;
    dprintc(cwd_path, 0x70); dcY++; dcX = 11;
    dprintmultc(0xCD, 7, 0x70);
    bool empty = true;
    int count = 1;

    int max_entries = (cwd_cluster == ROOT_CLUSTER) ? FS_ROOT_ENTRIES : ((SECTOR_SIZE * FS_SECTORS_PER_CLUSTER) / 32);

    dcX = 11; dcY = 8;

    for (int i = 0; i < max_entries; i++) {
        FatDirEntry e;
        if (!read_dir_entry(cwd_cluster, i, &e)) break;
        if ((uint8_t)e.name[0] == 0x00 || (uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr == 0x0F) continue;

        char disp[20];
        name83_to_display(e.name, disp);

        if (streq(disp, ".") || streq(disp, "..")) continue;

        char full[MAX_PATH_LEN];
        if (streq(cwd_path, "0:\\")) {
            str_cpy(full, "0:\\");
            str_cpy(full + 3, disp);
        } else {
            str_cpy(full, cwd_path);
            int p = str_len(full);
            full[p++] = '\\';
            str_cpy(full + p, disp);
        }

        if (is_hidden_path(full)) continue;

        if (e.attr & ATTR_DIRECTORY) {dprintintc(count, 0x70); dprintc(" [DIR] ", 0x70);} 
        else {dprintintc(count, 0x70); dprintc(" [FILE] ", 0x70);}
        dprintc(disp, 0x70);
        if (!(e.attr & ATTR_DIRECTORY)) {
            dprintc(" (", 0x70);
            dprintintc((int)e.file_size, 0x70);
            dprintc(" bytes)", 0x70);
        }
        dcX = 11; dcY++;
        count++;
        empty = false;
    }

    if (empty) dprintc("(empty)", 0x70);
}

void vfs_list_files() {
    vfs_list_current_dir();
}

bool vfs_delete_file(const char* path) {
    char abs[MAX_PATH_LEN];
    if (!normalize_path(path, abs)) return false;

    uint16_t parent;
    FatDirEntry e;
    int idx;
    if (!resolve_path_to_entry(abs, &parent, &e, &idx, 0)) return false;
    if (e.attr & ATTR_DIRECTORY) return false;

    if (dir_entry_cluster(&e) >= 2) fat_free_chain(dir_entry_cluster(&e));

    FatDirEntry tomb;
    mem_zero((uint8_t*)&tomb, sizeof(tomb));
    tomb.name[0] = (char)0xE5;
    if (!write_dir_entry(parent, idx, &tomb)) return false;

    fat_flush();
    return true;
}

bool vfs_read_file_line(const char* path, char* line_out) {
    static char loaded_path[MAX_PATH_LEN];
    static char file_buffer[FILE_BUFFER_LIMIT + 1];
    static int index = 0;
    static bool loaded = false;

    char abs[MAX_PATH_LEN];
    if (!normalize_path(path, abs)) return false;

    if (!loaded || !streq(loaded_path, abs)) {
        if (!vfs_read_file(abs, file_buffer)) {
            loaded = false;
            return false;
        }
        str_cpy(loaded_path, abs);
        index = 0;
        loaded = true;
    }

    if (file_buffer[index] == '\0') {
        loaded = false;
        return false;
    }

    int i = 0;
    while (file_buffer[index] != '\n' && file_buffer[index] != '\0') {
        line_out[i++] = file_buffer[index++];
    }

    if (file_buffer[index] == '\n') index++;

    line_out[i] = '\0';
    return true;
}

int vfs_file_count() {
    int count = 0;
    for (int i = 0; i < FS_ROOT_ENTRIES; i++) {
        FatDirEntry e;
        if (!read_dir_entry(ROOT_CLUSTER, i, &e)) break;
        if ((uint8_t)e.name[0] == 0x00 || (uint8_t)e.name[0] == 0xE5) continue;
        if (e.attr == 0x0F) continue;
        if (!(e.attr & ATTR_DIRECTORY)) count++;
    }

    return count;
}

void vfs_reset() {
    printc("Are you sure you would like to reset this system? (y/n): ", 0x0C);
    char ans[4] = "";
    int ans_index = 0;
    bool running = true;

    while (running) {
        char key = get_key();
        if (!key) continue;
        if (key == '\n') running = false;
        else if (key == 8) {
            if (ans_index > 0) {
                ans_index--;
                ans[ans_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if ((key == 'y' || key == 'n') && ans_index < 3) {
            putchar(key);
            ans[ans_index++] = key;
            ans[ans_index] = '\0';
        }
    }

    if (streq(ans, "y")) {
        fs_format_fat16();
        fat_load();
        str_cpy(cwd_path, "0:\\");
        cwd_cluster = ROOT_CLUSTER;

        vfs_seed_defaults();
        putchar('\n');
        println("System reset. Rebooting...");
        sleep(20000);
        reboot();
    }
    else {
        println("");
    }
}

#include "fs.h"
#include "ata.h"

extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern void reboot();
extern void putchar(char c);
extern char get_key();
extern void sleep();

extern int cursorX;

#define SECTOR_SIZE 512

/* FAT16 layout starting at LBA 1 on the raw drive. */
#define FS_BASE_LBA 1
#define FS_TOTAL_SECTORS 20480 /* 10 MiB raw disk from run.sh */
#define FS_RESERVED_SECTORS 1
#define FS_NUM_FATS 2
#define FS_ROOT_ENTRIES 128
#define FS_ROOT_DIR_SECTORS ((FS_ROOT_ENTRIES * 32 + (SECTOR_SIZE - 1)) / SECTOR_SIZE)
#define FS_SECTORS_PER_FAT 80
#define FS_SECTORS_PER_CLUSTER 1

#define FAT16_EOC 0xFFFF
#define FAT16_FREE 0x0000

#define DATA_START_LBA (FS_BASE_LBA + FS_RESERVED_SECTORS + (FS_NUM_FATS * FS_SECTORS_PER_FAT) + FS_ROOT_DIR_SECTORS)
#define ROOT_DIR_LBA (FS_BASE_LBA + FS_RESERVED_SECTORS + (FS_NUM_FATS * FS_SECTORS_PER_FAT))
#define TOTAL_CLUSTERS (FS_TOTAL_SECTORS - FS_RESERVED_SECTORS - (FS_NUM_FATS * FS_SECTORS_PER_FAT) - FS_ROOT_DIR_SECTORS)
#define FAT_ENTRY_COUNT (TOTAL_CLUSTERS + 2)

#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20

#define ROOT_CLUSTER 0

#define MAX_PATH_PARTS 16
#define MAX_PATH_LEN 64
#define FILE_BUFFER_LIMIT (SECTOR_SIZE * 8)

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

    read_sector(cluster_to_lba(dir_cluster), sector_buffer);
    int off = index * 32;
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

    read_sector(cluster_to_lba(dir_cluster), sector_buffer);
    int off = index * 32;
    for (int i = 0; i < 32; i++) sector_buffer[off + i] = ((const uint8_t*)in)[i];
    ata_write_sector(cluster_to_lba(dir_cluster), sector_buffer);
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
            ata_write_sector(cluster_to_lba(c), sector_buffer);
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
        for (int i = 0; i < SECTOR_SIZE; i++) {
            sector_buffer[i] = (offset < size) ? (uint8_t)data[offset++] : 0;
        }
        ata_write_sector(cluster_to_lba(cur), sector_buffer);

        if (fat[cur] == FAT16_EOC) break;
        cur = fat[cur];
    }

    return true;
}

static bool read_chain_data(uint16_t first_cluster, char* out, int size) {
    uint16_t cur = first_cluster;
    int offset = 0;

    while (cur >= 2 && cur < FAT_ENTRY_COUNT && offset < size) {
        ata_read_sector(cluster_to_lba(cur), sector_buffer);
        for (int i = 0; i < SECTOR_SIZE && offset < size; i++) out[offset++] = (char)sector_buffer[i];

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
    bs.total_sectors_16 = FS_TOTAL_SECTORS;
    bs.media = 0xF8;
    bs.fat_size_16 = FS_SECTORS_PER_FAT;
    bs.sectors_per_track = 63;
    bs.num_heads = 16;
    bs.hidden_sectors = 0;
    bs.total_sectors_32 = 0;
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
    char read_buffer[FILE_BUFFER_LIMIT + 1];

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
                 (bs->total_sectors_16 == FS_TOTAL_SECTORS);

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

    ata_write_sector(cluster_to_lba(new_cluster), sector_buffer);
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
        int clusters = (len + SECTOR_SIZE - 1) / SECTOR_SIZE;
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
    print("------ "); print(cwd_path); print(" ------"); putchar('\n');
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

    /* count root files */
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
    print("Are you sure you would like to reset this system? (y/n): ");
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

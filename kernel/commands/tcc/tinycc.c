#include <stdint.h>
#include <stdbool.h>
#include "tinycc.h"

extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern void putchar(char c);
extern char get_key();
extern void path_prepend(char* path);
extern bool vfs_read_file(const char* path, char* buffer_out);
extern bool vfs_write_file(const char* path, const char* data);
extern int cursorX;

#define TCC_INPUT_MAX 64
#define TCC_SOURCE_MAX 512
#define TCC_OUTPUT_MAX 512
#define TCC_VAR_MAX 16
#define TCC_STACK_MAX 64
#define TCC_CODE_MAX 96
#define TCC_NAME_MAX 16
#define TCC_ERROR_MAX 96
#define TCC_LINE_MAX 128

typedef enum {
    OP_CONST,
    OP_LOAD,
    OP_STORE,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_CALL,
    OP_CALLSTR,
    OP_RET
} TinyOpcode;

typedef enum {
    BUILTIN_PRINT = 0,
    BUILTIN_PRINTLN = 1,
    BUILTIN_PRINTINT = 2,
    BUILTIN_BEEP = 3,
    BUILTIN_SLEEP = 4,
    BUILTIN_CLEAR = 5
} TinyBuiltinId;

typedef struct {
    TinyOpcode op;
    int a;
    int b;
    char text[64];
} TinyInstruction;

typedef struct {
    const char* name;
    TinyBuiltinId id;
    int int_arg_count;
    bool accepts_string;
} TinyBuiltin;

typedef struct {
    const char* src;
    int pos;
    char error[TCC_ERROR_MAX];
    TinyInstruction code[TCC_CODE_MAX];
    int code_count;
    char vars[TCC_VAR_MAX][TCC_NAME_MAX];
    int var_count;
} TinyParser;

static const TinyBuiltin tiny_builtins[] = {
    {"print", BUILTIN_PRINT, 0, true},
    {"println", BUILTIN_PRINTLN, 0, true},
    {"printint", BUILTIN_PRINTINT, 1, false},
    {"beep", BUILTIN_BEEP, 2, false},
    {"sleep", BUILTIN_SLEEP, 1, false},
    {"clear", BUILTIN_CLEAR, 0, false},
};

static bool tiny_streq(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static bool tiny_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool tiny_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool tiny_is_alnum(char c) {
    return tiny_is_alpha(c) || tiny_is_digit(c);
}

static void tiny_copy(char* dest, const char* src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static void tiny_set_error(TinyParser* parser, const char* msg) {
    if (parser->error[0] == '\0') {
        tiny_copy(parser->error, msg, TCC_ERROR_MAX);
    }
}

static void tiny_skip_ws(TinyParser* parser) {
    while (true) {
        unsigned char c = (unsigned char)parser->src[parser->pos];
        if ((c <= ' ' && c != '\0') || c == 127) {
            parser->pos++;
            continue;
        }
        if (c == '/' && parser->src[parser->pos + 1] == '/') {
            parser->pos += 2;
            while (parser->src[parser->pos] != '\0' && parser->src[parser->pos] != '\n') {
                parser->pos++;
            }
            continue;
        }
        break;
    }
}

static bool tiny_match_char(TinyParser* parser, char expected) {
    tiny_skip_ws(parser);
    if (parser->src[parser->pos] == expected) {
        parser->pos++;
        return true;
    }
    return false;
}

static bool tiny_expect_char(TinyParser* parser, char expected) {
    if (!tiny_match_char(parser, expected)) {
        char msg[32] = "Expected ' '";
        msg[10] = expected;
        tiny_set_error(parser, msg);
        return false;
    }
    return true;
}

static bool tiny_match_keyword(TinyParser* parser, const char* word) {
    int i = 0;
    tiny_skip_ws(parser);
    while (word[i] != '\0') {
        if (parser->src[parser->pos + i] != word[i]) {
            return false;
        }
        i++;
    }
    if (tiny_is_alnum(parser->src[parser->pos + i])) {
        return false;
    }
    parser->pos += i;
    return true;
}

static bool tiny_parse_identifier(TinyParser* parser, char* out, int max_len) {
    int i = 0;
    tiny_skip_ws(parser);
    if (!tiny_is_alpha(parser->src[parser->pos])) {
        tiny_set_error(parser, "Expected identifier");
        return false;
    }
    while (tiny_is_alnum(parser->src[parser->pos]) && i < max_len - 1) {
        out[i++] = parser->src[parser->pos++];
    }
    out[i] = '\0';
    while (tiny_is_alnum(parser->src[parser->pos])) {
        parser->pos++;
    }
    return true;
}

static bool tiny_parse_number(TinyParser* parser, int* value) {
    int sign = 1;
    int result = 0;
    tiny_skip_ws(parser);
    if (parser->src[parser->pos] == '-') {
        sign = -1;
        parser->pos++;
    }
    if (!tiny_is_digit(parser->src[parser->pos])) {
        tiny_set_error(parser, "Expected number");
        return false;
    }
    while (tiny_is_digit(parser->src[parser->pos])) {
        result = result * 10 + (parser->src[parser->pos] - '0');
        parser->pos++;
    }
    *value = result * sign;
    return true;
}

static bool tiny_parse_string(TinyParser* parser, char* out, int max_len) {
    int i = 0;
    tiny_skip_ws(parser);
    if (parser->src[parser->pos] != '"') {
        tiny_set_error(parser, "Expected string literal");
        return false;
    }
    parser->pos++;
    while (parser->src[parser->pos] != '\0' && parser->src[parser->pos] != '"') {
        char c = parser->src[parser->pos++];
        if (c == '\\') {
            char esc = parser->src[parser->pos++];
            if (esc == 'n') c = '\n';
            else if (esc == 't') c = '\t';
            else if (esc == '"') c = '"';
            else if (esc == '\\') c = '\\';
            else {
                tiny_set_error(parser, "Unsupported escape sequence");
                return false;
            }
        }
        if (i >= max_len - 1) {
            tiny_set_error(parser, "String literal too long");
            return false;
        }
        out[i++] = c;
    }
    if (parser->src[parser->pos] != '"') {
        tiny_set_error(parser, "Unterminated string literal");
        return false;
    }
    parser->pos++;
    out[i] = '\0';
    return true;
}

static int tiny_find_var(TinyParser* parser, const char* name) {
    for (int i = 0; i < parser->var_count; i++) {
        if (tiny_streq(parser->vars[i], name)) {
            return i;
        }
    }
    return -1;
}

static int tiny_add_var(TinyParser* parser, const char* name) {
    int existing = tiny_find_var(parser, name);
    if (existing >= 0) return existing;
    if (parser->var_count >= TCC_VAR_MAX) {
        tiny_set_error(parser, "Too many variables");
        return -1;
    }
    tiny_copy(parser->vars[parser->var_count], name, TCC_NAME_MAX);
    parser->var_count++;
    return parser->var_count - 1;
}

static const TinyBuiltin* tiny_find_builtin(const char* name) {
    for (unsigned int i = 0; i < sizeof(tiny_builtins) / sizeof(tiny_builtins[0]); i++) {
        if (tiny_streq(tiny_builtins[i].name, name)) {
            return &tiny_builtins[i];
        }
    }
    return 0;
}

static bool tiny_emit(TinyParser* parser, TinyOpcode op, int a, int b, const char* text) {
    if (parser->code_count >= TCC_CODE_MAX) {
        tiny_set_error(parser, "Program is too large");
        return false;
    }
    parser->code[parser->code_count].op = op;
    parser->code[parser->code_count].a = a;
    parser->code[parser->code_count].b = b;
    parser->code[parser->code_count].text[0] = '\0';
    if (text != 0) {
        tiny_copy(parser->code[parser->code_count].text, text, 64);
    }
    parser->code_count++;
    return true;
}

static bool tiny_parse_expr(TinyParser* parser);

static bool tiny_parse_factor(TinyParser* parser) {
    char name[TCC_NAME_MAX];
    int value = 0;

    tiny_skip_ws(parser);

    if (tiny_match_char(parser, '(')) {
        if (!tiny_parse_expr(parser)) return false;
        return tiny_expect_char(parser, ')');
    }

    if (tiny_is_digit(parser->src[parser->pos]) || parser->src[parser->pos] == '-') {
        if (!tiny_parse_number(parser, &value)) return false;
        return tiny_emit(parser, OP_CONST, value, 0, 0);
    }

    if (tiny_is_alpha(parser->src[parser->pos])) {
        if (!tiny_parse_identifier(parser, name, TCC_NAME_MAX)) return false;
        int slot = tiny_find_var(parser, name);
        if (slot < 0) {
            tiny_set_error(parser, "Unknown variable");
            return false;
        }
        return tiny_emit(parser, OP_LOAD, slot, 0, 0);
    }

    tiny_set_error(parser, "Expected expression");
    return false;
}

static bool tiny_parse_term(TinyParser* parser) {
    if (!tiny_parse_factor(parser)) return false;
    while (true) {
        tiny_skip_ws(parser);
        if (parser->src[parser->pos] == '*') {
            parser->pos++;
            if (!tiny_parse_factor(parser)) return false;
            if (!tiny_emit(parser, OP_MUL, 0, 0, 0)) return false;
        }
        else if (parser->src[parser->pos] == '/') {
            parser->pos++;
            if (!tiny_parse_factor(parser)) return false;
            if (!tiny_emit(parser, OP_DIV, 0, 0, 0)) return false;
        }
        else {
            break;
        }
    }
    return true;
}

static bool tiny_parse_expr(TinyParser* parser) {
    if (!tiny_parse_term(parser)) return false;
    while (true) {
        tiny_skip_ws(parser);
        if (parser->src[parser->pos] == '+') {
            parser->pos++;
            if (!tiny_parse_term(parser)) return false;
            if (!tiny_emit(parser, OP_ADD, 0, 0, 0)) return false;
        }
        else if (parser->src[parser->pos] == '-') {
            parser->pos++;
            if (!tiny_parse_term(parser)) return false;
            if (!tiny_emit(parser, OP_SUB, 0, 0, 0)) return false;
        }
        else {
            break;
        }
    }
    return true;
}

static bool tiny_parse_call_stmt(TinyParser* parser, const char* name) {
    const TinyBuiltin* builtin = tiny_find_builtin(name);
    int arg_count = 0;
    char string_arg[64];

    if (builtin == 0) {
        tiny_set_error(parser, "Unknown builtin function");
        return false;
    }
    if (!tiny_expect_char(parser, '(')) return false;

    if (builtin->accepts_string) {
        if (!tiny_parse_string(parser, string_arg, 64)) return false;
        if (!tiny_expect_char(parser, ')')) return false;
        if (!tiny_expect_char(parser, ';')) return false;
        return tiny_emit(parser, OP_CALLSTR, builtin->id, 0, string_arg);
    }

    if (builtin->int_arg_count > 0) {
        for (int i = 0; i < builtin->int_arg_count; i++) {
            if (!tiny_parse_expr(parser)) return false;
            arg_count++;
            if (i + 1 < builtin->int_arg_count) {
                if (!tiny_expect_char(parser, ',')) return false;
            }
        }
    }

    if (!tiny_expect_char(parser, ')')) return false;
    if (!tiny_expect_char(parser, ';')) return false;
    return tiny_emit(parser, OP_CALL, builtin->id, arg_count, 0);
}

static void tiny_skip_bom(TinyParser* parser) {
    if ((unsigned char)parser->src[parser->pos] == 0xEF &&
        (unsigned char)parser->src[parser->pos + 1] == 0xBB &&
        (unsigned char)parser->src[parser->pos + 2] == 0xBF) {
        parser->pos += 3;
    }
}

static bool tiny_seek_main(TinyParser* parser) {
    TinyParser probe = *parser;

    tiny_skip_bom(&probe);
    while (probe.src[probe.pos] != '\0') {
        TinyParser candidate = probe;
        int start_pos = candidate.pos;

        if ((tiny_match_keyword(&candidate, "int") || tiny_match_keyword(&candidate, "void")) &&
            tiny_match_keyword(&candidate, "main")) {
            *parser = candidate;
            parser->pos = start_pos;
            return true;
        }

        probe.pos++;
    }

    return false;
}

static bool tiny_parse_statement(TinyParser* parser) {
    char name[TCC_NAME_MAX];

    if (tiny_match_keyword(parser, "int")) {
        if (!tiny_parse_identifier(parser, name, TCC_NAME_MAX)) return false;
        int slot = tiny_add_var(parser, name);
        if (slot < 0) return false;
        if (tiny_match_char(parser, '=')) {
            if (!tiny_parse_expr(parser)) return false;
        }
        else {
            if (!tiny_emit(parser, OP_CONST, 0, 0, 0)) return false;
        }
        if (!tiny_expect_char(parser, ';')) return false;
        return tiny_emit(parser, OP_STORE, slot, 0, 0);
    }

    if (tiny_match_keyword(parser, "return")) {
        if (!tiny_parse_expr(parser)) return false;
        if (!tiny_expect_char(parser, ';')) return false;
        return tiny_emit(parser, OP_RET, 0, 0, 0);
    }

    if (!tiny_parse_identifier(parser, name, TCC_NAME_MAX)) return false;

    tiny_skip_ws(parser);
    if (parser->src[parser->pos] == '(') {
        return tiny_parse_call_stmt(parser, name);
    }
    if (parser->src[parser->pos] == '=') {
        int slot = tiny_find_var(parser, name);
        if (slot < 0) {
            tiny_set_error(parser, "Unknown variable");
            return false;
        }
        parser->pos++;
        if (!tiny_parse_expr(parser)) return false;
        if (!tiny_expect_char(parser, ';')) return false;
        return tiny_emit(parser, OP_STORE, slot, 0, 0);
    }

    tiny_set_error(parser, "Expected function call or assignment");
    return false;
}

static bool tiny_compile_source(const char* src, TinyInstruction* code, int* code_count, char* out_error) {
    TinyParser parser;
    parser.src = src;
    parser.pos = 0;
    parser.error[0] = '\0';
    parser.code_count = 0;
    parser.var_count = 0;

    if (!tiny_seek_main(&parser)) {
        tiny_set_error(&parser, "Could not find an int main() or void main() entry point");
    }
    else if (!tiny_match_keyword(&parser, "int") && !tiny_match_keyword(&parser, "void")) {
        tiny_set_error(&parser, "Only int main() and void main() entry points are supported");
    }
    else if (!tiny_match_keyword(&parser, "main")) {
        tiny_set_error(&parser, "Only main() entry points are supported");
    }
    else if (!tiny_expect_char(&parser, '(') || !tiny_expect_char(&parser, ')') || !tiny_expect_char(&parser, '{')) {
    }
    else {
        while (parser.error[0] == '\0') {
            tiny_skip_ws(&parser);
            if (parser.src[parser.pos] == '}') {
                parser.pos++;
                break;
            }
            if (parser.src[parser.pos] == '\0') {
                tiny_set_error(&parser, "Missing closing brace");
                break;
            }
            if (!tiny_parse_statement(&parser)) {
                break;
            }
        }
    }

    tiny_skip_ws(&parser);
    if (parser.error[0] == '\0' && parser.src[parser.pos] != '\0') {
        tiny_set_error(&parser, "Unexpected trailing input");
    }
    if (parser.error[0] == '\0' && (parser.code_count == 0 || parser.code[parser.code_count - 1].op != OP_RET)) {
        if (!tiny_emit(&parser, OP_CONST, 0, 0, 0) || !tiny_emit(&parser, OP_RET, 0, 0, 0)) {
        }
    }

    if (parser.error[0] != '\0') {
        tiny_copy(out_error, parser.error, TCC_ERROR_MAX);
        return false;
    }

    for (int i = 0; i < parser.code_count; i++) {
        code[i] = parser.code[i];
    }
    *code_count = parser.code_count;
    out_error[0] = '\0';
    return true;
}

static bool tiny_append_char(char* out, int* len, char c) {
    if (*len >= TCC_OUTPUT_MAX - 1) return false;
    out[*len] = c;
    (*len)++;
    out[*len] = '\0';
    return true;
}

static bool tiny_append_str(char* out, int* len, const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (!tiny_append_char(out, len, str[i])) return false;
    }
    return true;
}

static bool tiny_append_int(char* out, int* len, int value) {
    char buf[16];
    int i = 0;
    if (value == 0) {
        return tiny_append_char(out, len, '0');
    }
    if (value < 0) {
        if (!tiny_append_char(out, len, '-')) return false;
        value = -value;
    }
    while (value > 0 && i < 16) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    while (i > 0) {
        i--;
        if (!tiny_append_char(out, len, buf[i])) return false;
    }
    return true;
}

static bool tiny_append_escaped(char* out, int* len, const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c == '\n') {
            if (!tiny_append_str(out, len, "\\n")) return false;
        }
        else if (c == '\t') {
            if (!tiny_append_str(out, len, "\\t")) return false;
        }
        else if (c == '\\') {
            if (!tiny_append_str(out, len, "\\\\")) return false;
        }
        else {
            if (!tiny_append_char(out, len, c)) return false;
        }
    }
    return true;
}

static bool tiny_serialize_program(const TinyInstruction* code, int code_count, char* out, char* out_error) {
    int len = 0;
    out[0] = '\0';
    if (!tiny_append_str(out, &len, "MMS-TCC-1\n")) goto overflow;
    for (int i = 0; i < code_count; i++) {
        switch (code[i].op) {
            case OP_CONST:
                if (!tiny_append_str(out, &len, "CONST ") || !tiny_append_int(out, &len, code[i].a)) goto overflow;
                break;
            case OP_LOAD:
                if (!tiny_append_str(out, &len, "LOAD ") || !tiny_append_int(out, &len, code[i].a)) goto overflow;
                break;
            case OP_STORE:
                if (!tiny_append_str(out, &len, "STORE ") || !tiny_append_int(out, &len, code[i].a)) goto overflow;
                break;
            case OP_ADD:
                if (!tiny_append_str(out, &len, "ADD")) goto overflow;
                break;
            case OP_SUB:
                if (!tiny_append_str(out, &len, "SUB")) goto overflow;
                break;
            case OP_MUL:
                if (!tiny_append_str(out, &len, "MUL")) goto overflow;
                break;
            case OP_DIV:
                if (!tiny_append_str(out, &len, "DIV")) goto overflow;
                break;
            case OP_CALL:
                if (!tiny_append_str(out, &len, "CALL ") || !tiny_append_int(out, &len, code[i].a) || !tiny_append_char(out, &len, ' ') || !tiny_append_int(out, &len, code[i].b)) goto overflow;
                break;
            case OP_CALLSTR:
                if (!tiny_append_str(out, &len, "CALLSTR ") || !tiny_append_int(out, &len, code[i].a) || !tiny_append_char(out, &len, ' ') || !tiny_append_escaped(out, &len, code[i].text)) goto overflow;
                break;
            case OP_RET:
                if (!tiny_append_str(out, &len, "RET")) goto overflow;
                break;
        }
        if (!tiny_append_char(out, &len, '\n')) goto overflow;
    }
    if (!tiny_append_str(out, &len, "END\n")) goto overflow;
    out_error[0] = '\0';
    return true;
overflow:
    tiny_copy(out_error, "Compiled output exceeds filesystem file size", TCC_ERROR_MAX);
    return false;
}

static bool tiny_compile_to_text(const char* src, char* out_program, char* out_error) {
    TinyInstruction code[TCC_CODE_MAX];
    int code_count = 0;
    if (!tiny_compile_source(src, code, &code_count, out_error)) return false;
    return tiny_serialize_program(code, code_count, out_program, out_error);
}

static int tiny_parse_int_at(const char* text, int* index) {
    int sign = 1;
    int value = 0;
    if (text[*index] == '-') {
        sign = -1;
        (*index)++;
    }
    while (text[*index] >= '0' && text[*index] <= '9') {
        value = value * 10 + (text[*index] - '0');
        (*index)++;
    }
    return value * sign;
}

static void tiny_unescape_into(char* dest, const char* src) {
    int i = 0;
    int j = 0;
    while (src[i] != '\0' && src[i] != '\n') {
        if (src[i] == '\\') {
            i++;
            if (src[i] == 'n') dest[j++] = '\n';
            else if (src[i] == 't') dest[j++] = '\t';
            else if (src[i] == '\\') dest[j++] = '\\';
            else dest[j++] = src[i];
            if (src[i] != '\0') i++;
        }
        else {
            dest[j++] = src[i++];
        }
    }
    dest[j] = '\0';
}

static bool tiny_vm_call(int builtin_id, int* stack, int* sp, const char* text, int* runtime_error) {
    if (builtin_id == BUILTIN_PRINT) {
        print(text);
        return true;
    }
    if (builtin_id == BUILTIN_PRINTLN) {
        println(text);
        return true;
    }
    if (builtin_id == BUILTIN_CLEAR) {
        extern void clear_screen();
        clear_screen();
        return true;
    }
    if (builtin_id == BUILTIN_PRINTINT) {
        if (*sp < 1) {
            *runtime_error = 1;
            return false;
        }
        printint(stack[--(*sp)]);
        return true;
    }
    if (builtin_id == BUILTIN_SLEEP) {
        extern void sleep(uint32_t count);
        if (*sp < 1) {
            *runtime_error = 1;
            return false;
        }
        sleep((uint32_t)stack[--(*sp)]);
        return true;
    }
    if (builtin_id == BUILTIN_BEEP) {
        extern void beep(uint32_t frequency, uint32_t count);
        int duration;
        int frequency;
        if (*sp < 2) {
            *runtime_error = 1;
            return false;
        }
        duration = stack[--(*sp)];
        frequency = stack[--(*sp)];
        beep((uint32_t)frequency, (uint32_t)duration);
        return true;
    }
    *runtime_error = 1;
    return false;
}

static bool tiny_execute_program(const char* program, int* exit_code) {
    int vars[TCC_VAR_MAX];
    int stack[TCC_STACK_MAX];
    int sp = 0;
    int pc = 0;
    int index = 0;
    int runtime_error = 0;
    char line[TCC_LINE_MAX];

    for (int i = 0; i < TCC_VAR_MAX; i++) vars[i] = 0;

    if (!(program[0] == 'M' && program[1] == 'M')) {
        println("Invalid bytecode file.");
        return false;
    }

    while (program[index] != '\0' && program[index] != '\n') index++;
    if (program[index] == '\n') index++;

    while (program[index] != '\0') {
        int line_len = 0;
        while (program[index] != '\0' && program[index] != '\n' && line_len < TCC_LINE_MAX - 1) {
            line[line_len++] = program[index++];
        }
        line[line_len] = '\0';
        if (program[index] == '\n') index++;
        if (line[0] == '\0') continue;
        if (tiny_streq(line, "END")) break;

        if (line[0] == 'C' && line[1] == 'O') {
            int pos = 6;
            stack[sp++] = tiny_parse_int_at(line, &pos);
        }
        else if (line[0] == 'L' && line[1] == 'O' && line[2] == 'A') {
            int pos = 5;
            int slot = tiny_parse_int_at(line, &pos);
            stack[sp++] = vars[slot];
        }
        else if (line[0] == 'S' && line[1] == 'T') {
            int pos = 6;
            int slot = tiny_parse_int_at(line, &pos);
            if (sp < 1) runtime_error = 1;
            else vars[slot] = stack[--sp];
        }
        else if (tiny_streq(line, "ADD")) {
            if (sp < 2) runtime_error = 1;
            else {
                stack[sp - 2] = stack[sp - 2] + stack[sp - 1];
                sp--;
            }
        }
        else if (tiny_streq(line, "SUB")) {
            if (sp < 2) runtime_error = 1;
            else {
                stack[sp - 2] = stack[sp - 2] - stack[sp - 1];
                sp--;
            }
        }
        else if (tiny_streq(line, "MUL")) {
            if (sp < 2) runtime_error = 1;
            else {
                stack[sp - 2] = stack[sp - 2] * stack[sp - 1];
                sp--;
            }
        }
        else if (tiny_streq(line, "DIV")) {
            if (sp < 2 || stack[sp - 1] == 0) runtime_error = 1;
            else {
                stack[sp - 2] = stack[sp - 2] / stack[sp - 1];
                sp--;
            }
        }
        else if (line[0] == 'C' && line[1] == 'A' && line[2] == 'L' && line[3] == 'L' && line[4] == ' ') {
            int pos = 5;
            int builtin_id = tiny_parse_int_at(line, &pos);
            int arg_count = 0;
            if (line[pos] == ' ') pos++;
            arg_count = tiny_parse_int_at(line, &pos);
            if (sp < arg_count) runtime_error = 1;
            else if (!tiny_vm_call(builtin_id, stack, &sp, "", &runtime_error)) {
            }
        }
        else if (line[0] == 'C' && line[1] == 'A' && line[2] == 'L' && line[3] == 'L' && line[4] == 'S') {
            int pos = 8;
            char text[64];
            int builtin_id = tiny_parse_int_at(line, &pos);
            if (line[pos] == ' ') pos++;
            tiny_unescape_into(text, &line[pos]);
            if (!tiny_vm_call(builtin_id, stack, &sp, text, &runtime_error)) {
            }
        }
        else if (tiny_streq(line, "RET")) {
            if (sp < 1) {
                runtime_error = 1;
            }
            else {
                *exit_code = stack[--sp];
                return true;
            }
        }
        else {
            runtime_error = 1;
        }

        if (runtime_error != 0 || sp < 0 || sp >= TCC_STACK_MAX) {
            print("Runtime error near instruction ");
            printint(pc);
            putchar('\n');
            return false;
        }
        pc++;
    }

    println("Program terminated without return.");
    return false;
}

static void tiny_prompt_path(const char* label, char* out_path) {
    int index = 0;
    bool running = true;
    print(label);
    while (running) {
        char key = get_key();
        if (!key) {
            continue;
        }
        if (key == '\n') {
            path_prepend(out_path);
            running = false;
        }
        else if (key == 8) {
            if (index > 0) {
                index--;
                out_path[index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if (index < TCC_INPUT_MAX - 4) {
            putchar(key);
            out_path[index++] = key;
            out_path[index] = '\0';
        }
    }
}

static void tiny_make_output_path(const char* src_path, char* out_path) {
    int i = 0;
    int dot = -1;
    while (src_path[i] != '\0' && i < TCC_INPUT_MAX - 1) {
        out_path[i] = src_path[i];
        if (src_path[i] == '.') dot = i;
        i++;
    }
    out_path[i] = '\0';
    if (dot < 0) dot = i;
    out_path[dot++] = '.';
    out_path[dot++] = 't';
    out_path[dot++] = 'b';
    out_path[dot++] = 'c';
    out_path[dot] = '\0';
}

void run_tcc_build() {
    char src_path[TCC_INPUT_MAX] = "";
    char out_path[TCC_INPUT_MAX] = "";
    char source[TCC_SOURCE_MAX];
    char program[TCC_OUTPUT_MAX];
    char error[TCC_ERROR_MAX];

    println("=== Tiny C build ===");
    println("Supported subset: int/void main() entry point, int vars, =, + - * /, print/println/printint/beep/sleep/clear, return.");
    tiny_prompt_path("C source file: ", src_path);
    putchar('\n');

    if (!vfs_read_file(src_path, source)) {
        print("Error: ");
        print(src_path);
        println(" not found.");
        return;
    }

    if (!tiny_compile_to_text(source, program, error)) {
        print("Compile error: ");
        println(error);
        return;
    }

    tiny_make_output_path(src_path, out_path);
    if (!vfs_write_file(out_path, program)) {
        println("Error: could not save compiled output.");
        return;
    }

    print("Built ");
    print(src_path);
    print(" -> ");
    print(out_path);
    putchar('\n');
}

void run_tcc_exec() {
    char program_path[TCC_INPUT_MAX] = "";
    char program[TCC_OUTPUT_MAX];
    int exit_code = 0;

    println("=== Tiny C execute ===");
    tiny_prompt_path("Compiled file: ", program_path);
    putchar('\n');

    if (!vfs_read_file(program_path, program)) {
        print("Error: ");
        print(program_path);
        println(" not found.");
        return;
    }

    if (tiny_execute_program(program, &exit_code)) {
        print("Program exited with code ");
        printint(exit_code);
        putchar('\n');
    }
}

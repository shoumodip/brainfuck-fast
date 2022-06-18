#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <errno.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

#define ASM_EXT ".fasm"
#define MEMORY_CAP 30000

const char *map_file(const char *file_path)
{
    FILE *f = fopen(file_path, "r");
    if (f == NULL) goto error;
    if (fseek(f, 0, SEEK_END) == -1) goto error;

    long size = ftell(f);
    if (size == -1) goto error;
    rewind(f);

    char *data = malloc(size + 1);
    if (data == NULL) goto error;
    if (size && fread(data, size, 1, f) == 0) goto error;
    if (fclose(f) == -1) goto error;

    data[size] = '\0';
    return data;

error:
    fprintf(stderr, "Error: could not read file '%s': %s\n",
            file_path, strerror(errno));

    if (f) fclose(f);
    if (data) free(data);

    exit(1);
}

long same_count(const char **input, const char *match)
{
    long count = 0;
    while (true) {
        if (**input == match[0]) {
            count++;
        } else if (**input == match[1]) {
            count--;
        } else {
            break;
        }
        *input += 1;
    }
    return count;
}

// Program START
typedef enum {
    OP_SHIFT,
    OP_ADD,
    OP_READ,
    OP_WRITE,
    OP_JZ,
    OP_JNZ,
} OpType;

typedef struct {
    OpType type;
    long operand;
    bool label;
} Op;

#define PROGRAM_CAP 16000
Op program[PROGRAM_CAP];
size_t program_size;

void op(OpType type, long operand)
{
    assert(program_size < PROGRAM_CAP);
    program[program_size].type = type;
    program[program_size].operand = operand;
    program_size++;
}

void patch(size_t index)
{
    program[index].operand = program_size;
}

#define JUMPS_CAP 256
size_t jumps[JUMPS_CAP];
size_t jumps_count;

void jumps_push(void)
{
    assert(jumps_count < JUMPS_CAP);
    jumps[jumps_count++] = program_size;
}

size_t jumps_pop(void)
{
    assert(jumps_count);
    return jumps[--jumps_count];
}
// Program END

// Cmd Buffer START
#define CMD_CAP 1024
char cmd[CMD_CAP];
size_t cmd_len;

void cmd_push(const char *src)
{
    const size_t len = strlen(src);
    assert(cmd_len + len < CMD_CAP);
    memcpy(cmd + cmd_len, src, len);
    cmd_len += len;
}

void cmd_end(void)
{
    cmd[cmd_len] = '\0';
}
// Cmd Buffer END

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: brainfuck INPUT\n");
        exit(1);
    }

    const char *input_path = argv[1];
    const char *input = map_file(input_path);
    while (*input) {
        switch (*input++) {
        case '>':
            op(OP_SHIFT, same_count(&input, "><") + 1);
            break;

        case '<':
            op(OP_SHIFT, same_count(&input, "><") - 1);
            break;

        case '+':
            op(OP_ADD, same_count(&input, "+-") + 1);
            break;

        case '-':
            op(OP_ADD, same_count(&input, "+-") - 1);
            break;

        case ',':
            op(OP_READ, 0);
            break;

        case '.':
            op(OP_WRITE, 0);
            break;

        case '[':
            jumps_push();
            op(OP_JZ, 0);
            break;

        case ']': {
            const size_t addr = jumps_pop();
            op(OP_JNZ, addr + 1);
            patch(addr);

            program[program_size].label = true;
            program[addr + 1].label = true;
        } break;
        }
    }

    const size_t input_size = strlen(input_path);
    const char *input_ext = memchr(input_path, '.', input_size);

    const size_t output_size = input_ext ? (size_t) (input_ext - input_path) : input_size;
    char *output_path = malloc(output_size + sizeof(ASM_EXT));

    memcpy(output_path, input_path, output_size);
    memcpy(output_path + output_size, ASM_EXT, sizeof(ASM_EXT));

    FILE *output = fopen(output_path, "w");
    if (output == NULL) {
        fprintf(stderr, "Error: could not write '%s': %s\n",
                output_path, strerror(errno));
        exit(1);
    }

    fprintf(output, "format elf64 executable\n");
    fprintf(output, "segment readable executable\n");

    fprintf(output, "mov rsi, memory\n");
    fprintf(output, "mov rdx, 1\n");

    for (size_t i = 0; i < program_size; ++i) {
        const Op op = program[i];

        if (op.label) {
            fprintf(output, "i%zu:\n", i);
        }

        switch (op.type) {
        case OP_SHIFT:
            fprintf(output, "add rsi, %ld\n", op.operand);
            break;

        case OP_ADD:
            fprintf(output, "add byte [rsi], %ld\n", op.operand);
            break;

        case OP_READ:
            fprintf(output, "mov rax, %d\n", SYS_read);
            fprintf(output, "mov rdi, %d\n", STDIN_FILENO);
            fprintf(output, "syscall\n");
            break;

        case OP_WRITE:
            fprintf(output, "mov rax, %d\n", SYS_write);
            fprintf(output, "mov rdi, %d\n", STDOUT_FILENO);
            fprintf(output, "syscall\n");
            break;

        case OP_JZ:
            fprintf(output, "mov rax, [rsi]\n");
            fprintf(output, "test al, al\n");
            fprintf(output, "jz i%zu\n", op.operand);
            break;

        case OP_JNZ:
            fprintf(output, "mov rax, [rsi]\n");
            fprintf(output, "test al, al\n");
            fprintf(output, "jnz i%zu\n", op.operand);
            break;

        default: assert(0 && "unreachable");
        }
    }

    fprintf(output, "i%zu:\n", program_size);
    fprintf(output, "mov rax, %d\n", SYS_exit);
    fprintf(output, "xor rdi, rdi\n");
    fprintf(output, "syscall\n");

    fprintf(output, "segment readable writable\n");
    fprintf(output, "memory: rb %d\n", MEMORY_CAP);
    fclose(output);

    cmd_push("fasm ");
    cmd_push(output_path);
    cmd_push(" >/dev/null");
    cmd_end();
    system(cmd);
    unlink(output_path);

    free(output_path);
    return 0;
}

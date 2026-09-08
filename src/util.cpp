#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "../include/kv/util.h"
#include <string.h>

/**
 * function: load_kv_data
 * args:
 *          - file_name     [const char*]       文件名
 *          - kv_list_ptr   [struct kv_t**]     键值对列表指针
 *          - n             [size_t*]           键值对列表已分配内存
 * return:
 *          - [size_t]      具体键值对数目
 */
size_t load_kv_data(const char* file_name, struct kv_t** kv_list_ptr, size_t* n)
{
    if (file_name == NULL || kv_list_ptr == NULL || n == NULL) {
        fprintf(stderr, "error: invalid args\n");
        return 0;
    }
    FILE* stream = fopen(file_name, "r");
    if (stream == NULL) {
        stream = fopen(file_name, "w");
        fclose(stream);
        stream = fopen(file_name, "r");
    }

    if (*kv_list_ptr == NULL) {
        *n = 256;
        *kv_list_ptr = (kv_t*)calloc(*n, sizeof(struct kv_t));
        if (*kv_list_ptr == NULL) {
            fprintf(stderr, "error: malloc failed\n");
            exit(1);
        }
    }

    size_t line_buf_size = 256;
    char* line_buf = (char*)calloc(line_buf_size, sizeof(char));

    size_t kv_cnt = 0;
    while ((getline(&line_buf, &line_buf_size, stream)) != -1) {
        /* 扩容 */
        if (kv_cnt + 1 > *n) {
            *n *= 2;
            struct kv_t* newbuf = (kv_t*)realloc(*kv_list_ptr, *n * sizeof(struct kv_t));
            if (newbuf == NULL) {
                fprintf(stderr, "error: malloc failed\n");
                exit(1);
            }
            *kv_list_ptr = newbuf;
        }

        char* value_start = line_buf + 5;
        memcpy(&(*kv_list_ptr)[kv_cnt].key, line_buf, sizeof(unsigned int));
        size_t string_len = strlen(value_start);
        (*kv_list_ptr)[kv_cnt].value = (char*)calloc(string_len + 1, sizeof(char));
        value_start[string_len - 1] = '\0';
        strcpy((*kv_list_ptr)[kv_cnt].value, value_start);
        ++kv_cnt;
    }

    free(line_buf);
    fclose(stream);
    return kv_cnt;
}

size_t store_kv_data(const char* file_name, struct kv_t** kv_list_ptr, size_t kv_cnt)
{
    if (file_name == NULL || kv_list_ptr == NULL) {
        fprintf(stderr, "error: invalid args\n");
        return 0;
    }
    FILE* stream = fopen(file_name, "w");
    if (stream == NULL) {
        fprintf(stderr, "error: cannot open file: %s\n", file_name);
        exit(1);
    }

    size_t i = 0;
    while (i < kv_cnt) {
        unsigned int key = (*kv_list_ptr)[i].key;
        char* value = (*kv_list_ptr)[i].value;
        fwrite(&key, sizeof(unsigned int), 1, stream);
        fwrite(",", sizeof(char), 1, stream);
        fwrite(value, sizeof(char), strlen(value), stream);
        fwrite("\n", sizeof(char), 1, stream);
        ++i;
    }

    fclose(stream);
    return 0;
}

int parser_command(token_t** tokens_ptr, int* n, token_t cmd)
{
    /* 解析命令 */
    /* tokens = "p" "key" "value" ... */
    if (tokens_ptr == NULL || n == NULL) {
        fprintf(stderr, "error: invalid args\n");
        return -1;
    }
    if (*tokens_ptr == NULL) {
        *tokens_ptr = (token_t*)calloc(4, sizeof(token_t));
    }
    token_t token;
    /* 指令长度 */
    int cmd_len = 0;
    /* 指令行分词 */
    while ((token = strsep(&cmd, ",")) != NULL) {
        if (cmd_len + 1 > *n) {
            *n *= 2;
            token_t* newline_buf = (token_t*)realloc(*tokens_ptr, *n * sizeof (token_t));
            *tokens_ptr = newline_buf;
        }
        /* token's size */
        int tsize = strlen(token);
        (*tokens_ptr)[cmd_len] = (token_t)malloc(tsize + 1);
        strcpy((*tokens_ptr)[cmd_len], token);
        ++cmd_len;
    }
    return cmd_len;
}

int valid_cmd(const token_t cmd)
{
    char ch = cmd[0];
    switch (cmd[0])
    {
        case 'p':
        case 'g':
        case 'd':
        case 'c':
        case 'a':
            return ch;
        default: {
            fprintf(stderr, "error: bad command '%c'\n", ch);
            return -1;
        }
    };
}

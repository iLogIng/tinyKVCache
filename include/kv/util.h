#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>

/* 分词类型 */
typedef char* token_t;

/* 键值对结构体 */
struct kv_t {
    unsigned int key;
    char* value;
};

/**
 * function: load_kv_data
 */
size_t load_kv_data(const char* file_name, struct kv_t** kv_list_ptr, size_t* n);

/**
 * function: store_kv_data
 */
size_t store_kv_data(const char* file_name, struct kv_t** kv_list_ptr, size_t kv_cnt);

/**
 * function: parser_command
 * args:
 *          - tokens_ptr    [token_t**]     命令分词缓冲区指针
 *          - n             [*int]          命令分词缓冲区大小指针
 *          - cmd           [token_t]       待解析的命令
 * return:
 *          - [int]                         命令长度
 */
int parser_command(token_t** tokens_ptr, int* n, token_t cmd);

/**
 * function: cmd
 * args:
 *          - cmd       [const token_t]     命令
 * return:
 *          - [int]                         单字符命令(若不存在，则返回 -1)
 */
int valid_cmd(const token_t cmd);

#endif
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/util.h"

int main(int argc, char* argv[])
{
    const int command_cnt = argc - 1;
    token_t* commands = argv + 1;

    /* 无待处理命令 */
    if (command_cnt == 0) {
        exit(EXIT_SUCCESS);
    }

    /* 持久化数据文件 */
    const char* file_name = "output/database.txt";

    /* 键值对列表缓冲 */
    size_t kv_buf_size = 256;
    struct kv_t* kv_list = (kv_t*)calloc(kv_buf_size, sizeof(struct kv_t));
    size_t kv_cnt = load_kv_data(file_name, &kv_list, &kv_buf_size);

    /* 指令缓冲 */
    int token_cnt = 4;
    token_t* tokens = (token_t*)calloc(token_cnt, sizeof(token_t));
    /* 行缓冲 */
    size_t line_buf_size = 256;
    char* line_buf = (char*)calloc(line_buf_size, sizeof(char));

    for (int i = 0; i < command_cnt; ++i) {
        /* 解析命令 */
        int cmd_len = parser_command(&tokens, &token_cnt, commands[i]);
        /* 目前的命令 */
        char c = '\0';
        if (strlen(tokens[0]) > 0) {
            c = valid_cmd(tokens[0]);
        }
        else {
            fprintf(stderr, "error: invalid cmd\n");
            continue;
        }
        /* 按命令分配操作 */
        switch(c) {
            case 'p': {
                /* p,key,value */
                if (cmd_len != 3) {
                    fprintf(stderr, "error: invalid args\n");
                    continue;
                }
                unsigned int target = strtoul(tokens[1], NULL, 10);
                int changed = 0;
                for (size_t j = 0; j < kv_cnt; ++j) {
                    if (kv_list[j].key == target) {
                        free(kv_list[j].value);
                        kv_list[j].value = (char*)calloc(strlen(tokens[2]) + 1, sizeof(char));
                        strcpy(kv_list[j].value, tokens[2]);
                        changed = 1;
                        break;
                    }
                }
                if (changed) {
                    break;
                }
                /* 扩容 */
                if (kv_cnt >= kv_buf_size) {
                    kv_buf_size *= 2;
                    struct kv_t* newbuf = (kv_t*)realloc(kv_list, kv_buf_size * sizeof(struct kv_t));
                    if (newbuf == NULL) {
                        fprintf(stderr, "error: malloc faild\n");
                        exit(1);
                    }
                    kv_list = newbuf;
                }
                kv_list[kv_cnt].key = target;
                kv_list[kv_cnt].value = (char*)calloc(strlen(tokens[2]) + 1, sizeof(char));
                strcpy(kv_list[kv_cnt].value, tokens[2]);
                ++kv_cnt;
                break;
            }
            case 'g': {
                /* g,key */
                if (cmd_len != 2) {
                    fprintf(stderr, "error: invalid args\n");
                    exit(1);
                }
                unsigned int target = strtoul(tokens[1], NULL, 10);
                /* 获取键值对 */
                /* key,value */
                size_t j = 0;
                while (j < kv_cnt) {
                    if (kv_list[j].key == target) {
                        fprintf(stdout, "%d,%s\n", kv_list[j].key, kv_list[j].value);
                        break;
                    }
                    ++j;
                }
                if (j == kv_cnt) {
                    fprintf(stdout, "%d not found\n", target);
                }
                break;
            }
            case 'd': {
                /* d,key */
                /* delete by key */
                if (cmd_len != 2) {
                    fprintf(stderr, "error: invalid args\n");
                    exit(1);
                }
                unsigned int target = strtoul(tokens[1], NULL, 10);
                /* key,value */
                size_t j = 0;
                for (; j < kv_cnt; ++j) {
                    if (kv_list[j].key == target) {
                        free(kv_list[j].value);
                        for (size_t k = j; k < kv_cnt - 1; ++k) {
                            kv_list[k] = kv_list[k + 1];
                        }
                        --kv_cnt;
                        break;
                    }
                }
                break;
            }
            case 'c': {
                for (size_t i = 0; i < kv_cnt; ++i) {
                    free(kv_list[i].value);
                    kv_list[i].value = NULL;
                }
                kv_cnt = 0;
                break;
            }
            case 'a': {
                if (cmd_len != 1) {
                    fprintf(stderr, "error: invalid args\n");
                    exit(1);
                }
                size_t j = 0;
                while (j < kv_cnt) {
                    fprintf(stdout, "%d,%s\n", kv_list[j].key, kv_list[j].value);
                    ++j;
                }
                break;
            }
            default: {
                fprintf(stderr, "error: bad command\n");
                break;
            }
        };
    }

    /* 保存键值对 */
    store_kv_data(file_name, &kv_list, kv_cnt);

    /* 释放键值对列表 */
    for (size_t i = 0; i < kv_cnt; ++i) {
        free(kv_list[i].value);
    }
    free(kv_list);

    /* 释放行缓冲 */
    free(line_buf);
    /* 释放指令缓冲 */
    for (int i = 0; i < token_cnt; ++i) {
        free(tokens[i]);
    }
    free(tokens);

}

#ifndef __MAILBOX_CLIENT_H__
#define __MAILBOX_CLIENT_H__

#include "stdint.h"
#include "stdbool.h"
#include "stdarg.h"
#ifndef MBPTR64
typedef uint32_t mbptr_t;
#else
typedef uint64_t mbptr_t;
#endif
typedef enum
{
    MB_IDLE = 0,
    MB_EXIT = 1,
    MB_CPRINT = 3,
    MB_MEMMOVE = 4,
    MB_MEMSET = 5,
    MB_MEMCMP = 6,
    MB_CALL = 7,
    MB_FILEACCESS = 8,
    MB_OTHER = 0x80000000
} mb_action_t;

typedef enum
{
    MB_ST_OTHER = 0,
    MB_ST_INIT = 1,
    MB_ST_READY = 2
} mb_state_t;

typedef enum
{
    MB_FILE_ACTION_OPEN = 0,
    MB_FILE_ACTION_CLOSE = 1,
    MB_FILE_ACTION_READ = 2,
    MB_FILE_ACTION_WRITE = 3,
    MB_FILE_ACTION_SEEK = 4
} mb_file_action_t;

#define MB_FILE_READ 0x1
#define MB_FILE_WRITE 0x2
#define MB_FILE_APPEND 0x4
#define MB_FILE_TRUNC 0x8
#define MB_FD uint32_t

#define MB_MAX_ARGS 20
#define MB_MAX_ENTRIES 8

#if defined(MB_CACHE_LINE) && (MB_CACHE_LINE != 256) && (MB_CACHE_LINE != 128) && (MB_CACHE_LINE != 64) && (MB_CACHE_LINE != 32)
Invalid MB_CACHE_LINE value !
#endif

#ifdef MB_CACHE_LINE
#define MB_CACHE_LINE_ALIGN __attribute__((aligned(MB_CACHE_LINE)))
#else
#define MB_CACHE_LINE_ALIGN
#endif

    typedef struct
{
    uint32_t action;
    uint32_t words;
    mbptr_t args[MB_MAX_ARGS];
} mb_req_entry;

typedef struct
{
    uint32_t words;
    mbptr_t rets;
} mb_resp_entry;

#ifndef MB_VERSION_MAJOR
#define MB_VERSION_MAJOR 0
#endif

#ifndef MB_VERSION_MINOR
#define MB_VERSION_MINOR 2
#endif

#define MB_VERSION (((uint32_t)(MB_VERSION_MAJOR) << 16) | (uint32_t)(MB_VERSION_MINOR))

typedef struct
{
    uint32_t _reserved;
    uint32_t idx_p;
    mb_req_entry queue[MB_MAX_ENTRIES];
    uint32_t idx_c MB_CACHE_LINE_ALIGN;
} mb_req_queue;

typedef struct
{
    uint32_t _reserved;
    uint32_t idx_p;
    mb_resp_entry queue[MB_MAX_ENTRIES];
    uint32_t idx_c MB_CACHE_LINE_ALIGN;
} mb_resp_queue;

typedef struct
{
    uint32_t version;
    uint32_t state;
    mb_req_queue req_queue;
    mb_resp_queue resp_queue;
} mb_channel MB_CACHE_LINE_ALIGN;

void mb_reset(mb_channel *ch);
void mb_send_nb(mb_channel *ch, const mb_req_entry *req);
mb_resp_entry *mb_send(mb_channel *ch, const mb_req_entry *req);
void mb_rpc_cprint(mb_channel *ch, const char *fmt_str, const char *file, uint32_t pos, uintptr_t args_len, const uintptr_t *args);
mbptr_t mb_rpc_call(mb_channel *ch, const char *method, uintptr_t args_len, const uintptr_t *args);
void mb_rpc_exit(mb_channel *ch, uint32_t code);
MB_FD mb_rpc_fopen(mb_channel *ch, const char *path, uint32_t flags);
void mb_rpc_fclose(mb_channel *ch, MB_FD fd);
uintptr_t mb_rpc_fread(mb_channel *ch, MB_FD fd, void *data, uintptr_t len);
uintptr_t mb_rpc_fwrite(mb_channel *ch, MB_FD fd, const void *data, uintptr_t len);
uintptr_t mb_rpc_fseek(mb_channel *ch, MB_FD fd, uintptr_t pos);
int32_t mb_rpc_memcmp(mb_channel *ch, const void *s1, const void *s2, uintptr_t size);
void *mb_rpc_memmove(mb_channel *ch, void *dst, const void *src, uintptr_t size);
void *mb_rpc_memset(mb_channel *ch, void *dst, int data, uintptr_t size);
#endif
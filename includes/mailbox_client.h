#ifndef __MAILBOX_CLIENT_H__
#define __MAILBOX_CLIENT_H__

#include "stdint.h"
#include "stdbool.h"
#include "stdarg.h"
#define NTH_ARG(A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, N, ...) N
#define COUNT_VARGS(...) NTH_ARG(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define va_args_to_ptr(first_arg, rest_args, rest_args_len, va_len, first_type, rest_type) \
    {                                                                                      \
        va_list args;                                                                      \
        va_start(args, (va_len));                                                          \
        first_arg = (first_type)va_arg(args, first_type);                                  \
        unsigned int i = 0;                                                                \
        for (i = 0; i < (rest_args_len); i++)                                              \
        {                                                                                  \
            (rest_args)[i] = (rest_type)va_arg(args, rest_type);                           \
        }                                                                                  \
        va_end(args);                                                                      \
    }

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
void mb_cprint(const char *fmt_str, const char *file, uint32_t pos, uintptr_t args_len, const uintptr_t *args);
mbptr_t mb_call(const char *method, uintptr_t args_len, const uintptr_t *args);
void mb_exit(uint32_t code);
MB_FD mb_fopen(const char *path, uint32_t flags);
void mb_fclose(MB_FD fd);
uintptr_t mb_fread(MB_FD fd, void *data, uintptr_t len);
uintptr_t mb_fwrite(MB_FD fd, const void *data, uintptr_t len);
uintptr_t mb_fseek(MB_FD fd, uintptr_t pos);
int32_t mb_memcmp(const void *s1, const void *s2, uintptr_t size);
void *mb_memmove(void *dst, const void *src, uintptr_t size);
void *mb_memset(void *dst, int data, uintptr_t size);

static inline void mb_printf_wrapper(const char *file, unsigned int line, uintptr_t args_len, ...)
{
    uintptr_t buf[16];
    const char *fmt;
    uintptr_t num_args = (args_len - 1) > 16 ? 16 : args_len - 1;
    va_args_to_ptr(fmt, buf, num_args, args_len, const char *, uintptr_t);
    mb_cprint(fmt, file, line, num_args, buf);
}
#define mb_printf(...) mb_printf_wrapper(__FILE__, __LINE__, COUNT_VARGS(__VA_ARGS__), __VA_ARGS__)
#define float_to_arg(f) ({           \
    float _f = (f);                  \
    (uintptr_t)(*(uint32_t *)(&_f)); \
})
static inline uintptr_t mb_call_wrapper(uintptr_t args_len, ...)
{
    uintptr_t buf[16];
    const char *method;
    uintptr_t num_args = (args_len - 1) > 16 ? 16 : args_len - 1;
    va_args_to_ptr(method, buf, num_args, args_len, const char *, uintptr_t);
    return (uintptr_t)(mb_call(method, num_args, buf));
}
#define mbcall(...) mb_call_wrapper(COUNT_VARGS(__VA_ARGS__), __VA_ARGS__)

#define bd_memmove(dest, src, size) mb_memmove((void *)(dest), (void *)(src), (uintptr_t)(size))

#define bd_memcpy(dest, src, size) mb_memmove((void *)(dest), (void *)(src), (uintptr_t)(size))

#define bd_memset(dest, data, size) mb_memset((void *)(dest), (unsigned int)((unsigned char)(data)), (uintptr_t)(size))

#define bd_memcmp(s1, s2, size) mb_memcmp((void *)(s1), (void *)(s2), (uintptr_t)(size))
#endif
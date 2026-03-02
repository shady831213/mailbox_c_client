#include "mailbox_client.h"
#include "stddef.h"
#define mb_read32(a) (*((volatile uint32_t *)(a)))
#define mb_write32(a, v) (*((volatile uint32_t *)(a)) = (v))
#define mb_readptr(a) (*((volatile mbptr_t *)(a)))
#define mb_writeptr(a, v) (*((volatile mbptr_t *)(a)) = (v))

#define idx_masked(ptr) ((ptr) & (MB_MAX_ENTRIES - 1))
#define idx_flag(ptr) ((ptr) & MB_MAX_ENTRIES == 0)

#define idx_p_masked(q) (idx_masked(mb_read32(&((q)->idx_p))))
#define idx_c_masked(q) (idx_masked(mb_read32(&((q)->idx_c))))
#define idx_p_flag(q) (idx_flag(mb_read32(&((q)->idx_p))))
#define idx_c_flag(q) (idx_flag(mb_read32(&((q)->idx_c))))
#define cur_p_entry(q) (&((q)->queue[idx_p_masked(q)]))
#define cur_c_entry(q) (&((q)->queue[idx_c_masked(q)]))
#define advance_p(q) ({                           \
    uint32_t next = mb_read32(&((q)->idx_p)) + 1; \
    mb_write32(&((q)->idx_p), next);              \
})

#define advance_c(q) ({                           \
    uint32_t next = mb_read32(&((q)->idx_c)) + 1; \
    mb_write32(&((q)->idx_c), next);              \
})
#define full(q) (idx_c_masked(q) == idx_p_masked(q) && idx_c_flag(q) != idx_p_flag(q))
#define empty(q) (idx_c_masked(q) == idx_p_masked(q) && idx_c_flag(q) == idx_p_flag(q))

__attribute__((weak)) mbptr_t __mb_save_irq()
{
    return 0;
}

__attribute__((weak)) void __mb_restore_irq(mbptr_t flags)
{
}

__attribute__((weak)) void __mb_rfence(mbptr_t start, uintptr_t size)
{
}

__attribute__((weak)) void __mb_wfence(mbptr_t start, uintptr_t size)
{
}

inline bool is_ready(mb_channel *ch)
{
    return mb_read32(&ch->state) == MB_ST_READY;
}

inline void mb_reset(mb_channel *ch)
{
    mb_write32(&ch->version, MB_VERSION);
    mb_write32(&ch->state, MB_ST_INIT);
    mb_write32(&ch->req_queue.idx_p, 0);
    mb_write32(&ch->req_queue.idx_c, 0);
    mb_write32(&ch->resp_queue.idx_p, 0);
    mb_write32(&ch->resp_queue.idx_c, 0);
    __mb_wfence((mbptr_t)((uintptr_t)(ch)), sizeof(mb_channel));
}

static inline bool req_can_put(mb_channel *ch)
{
    return !full(&ch->req_queue);
}

static inline bool resp_can_get(mb_channel *ch)
{
    return !empty(&ch->resp_queue);
}

static inline mbptr_t put_req(mb_channel *ch, const mb_req_entry *req)
{
    mb_req_entry *entry = cur_p_entry(&ch->req_queue);
    *entry = *req;
    return (mbptr_t)((uintptr_t)(entry));
}

static inline mb_resp_entry *get_resp(mb_channel *ch)
{
    return cur_c_entry(&ch->resp_queue);
}

static inline mbptr_t commit_req(mb_channel *ch)
{
    advance_p(&ch->req_queue);
    return (mbptr_t)((uintptr_t)(&ch->req_queue.idx_p));
}

static inline mbptr_t ack_resp(mb_channel *ch)
{
    advance_c(&ch->resp_queue);
    return (mbptr_t)((uintptr_t)(&ch->resp_queue.idx_c));
}

static inline void try_send(mb_channel *ch, const mb_req_entry *req, bool *ready)
{
    __mb_rfence((mbptr_t)((uintptr_t)(ch)), sizeof(mb_channel));
    if (!is_ready(ch))
    {
        *ready = false;
        return;
    }
    if (!req_can_put(ch))
    {
        *ready = false;
        return;
    }
    mbptr_t entry = put_req(ch, req);
    __mb_wfence(entry, sizeof(mb_req_entry));
    mbptr_t ptr_ptr = commit_req(ch);
    __mb_wfence(ptr_ptr, sizeof(uint32_t));
    *ready = true;
}

static inline mb_resp_entry *try_recv(mb_channel *ch, bool *ready)
{
    __mb_rfence((mbptr_t)((uintptr_t)(ch)), sizeof(mb_channel));
    if (!resp_can_get(ch))
    {
        *ready = false;
        return 0;
    }
    mb_resp_entry *resp = get_resp(ch);
    mbptr_t ptr_ptr = ack_resp(ch);
    __mb_wfence(ptr_ptr, sizeof(uint32_t));
    *ready = true;
    return resp;
}

inline void mb_send_nb(mb_channel *ch, const mb_req_entry *req)
{
    mbptr_t flag = __mb_save_irq();
    bool ready = false;
    do
    {
        try_send(ch, req, &ready);
    } while (!ready);
    __mb_restore_irq(flag);
}

inline mb_resp_entry *mb_send(mb_channel *ch, const mb_req_entry *req)
{
    mbptr_t flag = __mb_save_irq();
    {
        bool ready = false;
        do
        {
            try_send(ch, req, &ready);
        } while (!ready);
    }
    mb_resp_entry *resp = NULL;
    {
        bool ready = false;
        do
        {
            resp = try_recv(ch, &ready);
        } while (!ready);
    }
    __mb_restore_irq(flag);
    return resp;
}

void mb_rpc_cprint(mb_channel *ch, const char *fmt_str, const char *file, uint32_t pos, uintptr_t args_len, const uintptr_t *args)
{
    mb_req_entry entry;
    entry.action = MB_CPRINT;
    entry.words = (uint32_t)(args_len) + 3;
    entry.args[0] = (mbptr_t)((uintptr_t)(fmt_str));
    entry.args[1] = (mbptr_t)((uintptr_t)(file));
    entry.args[2] = (mbptr_t)(pos);
    int i = 0;
    for (i = 0; i < args_len; i++)
    {
        entry.args[3 + i] = args[i];
    }
    mb_send_nb(ch, &entry);
}

mbptr_t mb_rpc_call(mb_channel *ch, const char *method, uintptr_t args_len, const uintptr_t *args)
{
    mb_req_entry entry;
    entry.action = MB_CALL;
    entry.words = (uint32_t)(args_len) + 1;
    entry.args[0] = (mbptr_t)((uintptr_t)(method));
    int i = 0;
    for (i = 0; i < args_len; i++)
    {
        entry.args[1 + i] = args[i];
    }
    return mb_send(ch, &entry)->rets;
}

void mb_rpc_exit(mb_channel *ch, uint32_t code)
{
    mb_req_entry entry;
    entry.action = MB_EXIT;
    entry.words = 1;
    entry.args[0] = (mbptr_t)(code);
    mb_send_nb(ch, &entry);
}

MB_FD mb_rpc_fopen(mb_channel *ch, const char *path, uint32_t flags)
{
    mb_req_entry entry;
    entry.action = MB_FILEACCESS;
    entry.words = 3;
    entry.args[0] = (mbptr_t)(MB_FILE_ACTION_OPEN);
    entry.args[1] = (mbptr_t)((uintptr_t)(path));
    entry.args[2] = (mbptr_t)(flags);

    return (MB_FD)(mb_send(ch, &entry)->rets);
}

void mb_rpc_fclose(mb_channel *ch, MB_FD fd)
{
    mb_req_entry entry;
    entry.action = MB_FILEACCESS;
    entry.words = 2;
    entry.args[0] = (mbptr_t)(MB_FILE_ACTION_CLOSE);
    entry.args[1] = (mbptr_t)(fd);
    mb_send_nb(ch, &entry);
}

uintptr_t mb_rpc_fread(mb_channel *ch, MB_FD fd, void *data, uintptr_t len)
{
    mb_req_entry entry;
    entry.action = MB_FILEACCESS;
    entry.words = 4;
    entry.args[0] = (mbptr_t)(MB_FILE_ACTION_READ);
    entry.args[1] = (mbptr_t)(fd);
    entry.args[2] = (mbptr_t)((uintptr_t)(data));
    entry.args[3] = (mbptr_t)(len);

    return (uintptr_t)(mb_send(ch, &entry)->rets);
}

uintptr_t mb_rpc_fwrite(mb_channel *ch, MB_FD fd, const void *data, uintptr_t len)
{
    mb_req_entry entry;
    entry.action = MB_FILEACCESS;
    entry.words = 4;
    entry.args[0] = (mbptr_t)(MB_FILE_ACTION_WRITE);
    entry.args[1] = (mbptr_t)(fd);
    entry.args[2] = (mbptr_t)((uintptr_t)(data));
    entry.args[3] = (mbptr_t)(len);

    return (uintptr_t)(mb_send(ch, &entry)->rets);
}

uintptr_t mb_rpc_fseek(mb_channel *ch, MB_FD fd, uintptr_t pos)
{
    mb_req_entry entry;
    entry.action = MB_FILEACCESS;
    entry.words = 3;
    entry.args[0] = (mbptr_t)(MB_FILE_ACTION_SEEK);
    entry.args[1] = (mbptr_t)(fd);
    entry.args[2] = (mbptr_t)(pos);

    return (uintptr_t)(mb_send(ch, &entry)->rets);
}

int32_t mb_rpc_memcmp(mb_channel *ch, const void *s1, const void *s2, uintptr_t size)
{
    mb_req_entry entry;
    entry.action = MB_MEMCMP;
    entry.words = 3;
    entry.args[0] = (mbptr_t)((uintptr_t)(s1));
    entry.args[1] = (mbptr_t)((uintptr_t)(s2));
    entry.args[2] = (mbptr_t)(size);

    return (int32_t)(mb_send(ch, &entry)->rets);
}

void *mb_rpc_memmove(mb_channel *ch, void *dst, const void *src, uintptr_t size)
{
    mb_req_entry entry;
    entry.action = MB_MEMMOVE;
    entry.words = 3;
    entry.args[0] = (mbptr_t)((uintptr_t)(dst));
    entry.args[1] = (mbptr_t)((uintptr_t)(src));
    entry.args[2] = (mbptr_t)(size);

    mb_send(ch, &entry);
    return dst;
}

void *mb_rpc_memset(mb_channel *ch, void *dst, int data, uintptr_t size)
{
    mb_req_entry entry;
    entry.action = MB_MEMSET;
    entry.words = 3;
    entry.args[0] = (mbptr_t)((uintptr_t)(dst));
    entry.args[1] = (mbptr_t)(data);
    entry.args[2] = (mbptr_t)(size);

    mb_send(ch, &entry);
    return dst;
}
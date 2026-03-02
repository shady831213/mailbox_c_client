# mailbox_c_client
c impl client of  [mailbox_rs](https://github.com/shady831213/mailbox_rs)

## Compile Options
 - MBPTR64: enable 64bits args width
 - MB_CACHE_LINE=[256 | 128 | 64 | 32] : mb_channels alignment

## How to use
1. need to static create mailbox channels in section .mailbox, for example:

```c
#define MBS_NUM 8
//impl core_id for your platform
inline uintptr_t core_id() {
    //...
}
static mb_channel __attribute__((section(".mailbox")))  mbs[MBS_NUM] ;
inline mb_channel *get_mb_ch() {
#if (MBS_NUM == 1)
    return &mbs[0];
#else
    return &mbs[core_id()]
#endif
}

```

2. override __mb_save_irq, __mb_restore_irq, __mb_rfence, __mb_wfence if necessary

## Limination
- not support rust print
- not support spin locked mailbox channel

 [mailbox_rs](https://github.com/shady831213/mailbox_rs) is recommanded. C version is only for pure c env can not use  [vf_rs](https://github.com/shady831213/vfw_rs).

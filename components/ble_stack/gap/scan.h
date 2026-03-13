#ifndef SCAN_H
#define SCAN_H

#if defined(CONFIG_BT_SCAN)
int scan_init(void);
int scan_work_start(void);
#else
static inline int scan_init(void)
{
    return -1;
}
static inline int scan_work_start(void)
{
    return -1;
}
#endif // CONFIG_BT_SCAN

#endif /* SCAN_H */
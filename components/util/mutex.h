#ifndef WORKER_MUTEX_H
#define WORKER_MUTEX_H

#include <zephyr/kernel.h>

void mutex_worker_handler_lock(void);

void mutex_worker_handler_unlock(void);

void sem_gatt_client_rw_give(void);

int sem_gatt_client_rw_take(k_timeout_t timeout);

#endif /* WORKER_MUTEX_H */
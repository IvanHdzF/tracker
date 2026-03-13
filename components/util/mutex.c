#include "util/mutex.h"

#include <zephyr/kernel.h>

/* Mutex for both gatt server and gatt client request handlers, assumes they both want a specific
 * transport resource (Ex: UART, SPI) */
K_MUTEX_DEFINE(mutex_worker_handler);

/* Semaphore for gatt client read/write operations, needed for avoiding issues with K_FREE
 * corrupting data that is still on an active request */
K_SEM_DEFINE(sem_gatt_client_rw, 1, 1);

void mutex_worker_handler_lock(void)
{
  k_mutex_lock(&mutex_worker_handler, K_FOREVER);
}

void mutex_worker_handler_unlock(void)
{
  k_mutex_unlock(&mutex_worker_handler);
}

void sem_gatt_client_rw_give(void)
{
  k_sem_give(&sem_gatt_client_rw);
}

int sem_gatt_client_rw_take(k_timeout_t timeout)
{
  return k_sem_take(&sem_gatt_client_rw, timeout);
}
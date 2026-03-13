#ifndef SERVICE_DI_H
#define SERVICE_DI_H

#define FW_VER_STR_SIZE 8

#include <stdint.h>

typedef struct __attribute__((packed))
{
  uint8_t fw_ver_major;
  uint8_t fw_ver_minor;
} fw_ver_data_t;

#endif /* SERVICE_DI_H */
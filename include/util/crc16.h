#ifndef INC_CRC16_H_
#define INC_CRC16_H_

#include <stdbool.h>
#include <stdint.h>

#define CRC_POLY_MODBUS 0x8005
#define CRC_POLY_BUYPASS 0x8005
#define CRC_POLY_CCITT 0x1021
#define CRC_POLY_DEFAULT 0x1021

#define CRC_INIT_DEFAULT 0x0000
#define CRC_INIT_CCITT_AUG 0x1D0F
#define CRC_INIT_XMODEM 0x0000  // poly = CCITT
#define CRC_INIT_CCITT_NEG 0xFFFF
#define CRC_INIT_MODBUS 0xFFFF
#define CRC_INIT_BUYPASS 0x0000

#define CRC_TABLE_SIZE 256

typedef struct
{
  bool table_init;
  uint16_t table[CRC_TABLE_SIZE];
  uint16_t initial_val;
} crc16_cb_t;

/*****************************************************************************/
/*!
 * \brief   Initialize the CRC16 module.
 *
 * \param [in]  uint16_t poly16     The 16-bit polynomial to use for the
 *                                  CRC calculation.
 * \param [in]  uint16_t init16     The 16-bit initial value to use for the
 *                                  CRC calculation.
 *
 * \returns void
 *
 *****************************************************************************/
void crc16_init(uint16_t poly16, uint16_t init16);

/*****************************************************************************/
/*!
 * \brief   Resets the CRC16 module.
 *
 * \details Resets the CRC Initial value to 0x0000.
 *          Resets the CRC Lookup Table Initialize flag to false so that the
 *          module thinks that the Lookup Table is uninitialized.
 *          This module's main purpose will be in testing. It is unlikely it
 *          will be used in production code.
 *
 * \returns void
 *
 *****************************************************************************/
void crc16_reset(void);

/*****************************************************************************/
/*!
 * \brief Determine is the CRC is correct for the given array of bytes.
 *
 * \details
 * The function takes a pointer to a byte array and the size of the byte
 * array. It then verifies that the CRC of the byte array is correct.  It
 * is assumed that the last two bytes of the array are the
 * 16 bit CRC for CRC-CCITT (0xFFFF).
 *
 * \param [in]  ba          A pointer to the byte array.
 * \param [in]  array_size  The number of bytes in the array.
 *
 * \returns bool - That is True if the CRC is correct for the data portion of
 *          the array and False if the CRC of the data does not match the CRC.
 *
 *****************************************************************************/
bool crc16_is_crc_correct(uint8_t *ba, int32_t array_size);

/*****************************************************************************/
/*!
 * \brief Retrieves the CRC value
 *
 * \details
 * The function takes a pointer to a byte array and calculates the
 * 16 bit CRC (CRC-CCITT (0xFFFF)) for the data.  The CRC is returned.
 *
 * @returns uint16_t - The 16 bit CRC of the data bytes in the array.
 *
 *****************************************************************************/
uint16_t crc16_get_crc(uint8_t *ba, int32_t array_size);

#endif /* INC_CRC16_H_ */

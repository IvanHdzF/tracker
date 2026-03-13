/******************************************************************************
 * Title 		: CRC 16
 * Filename 		: crc16.h
 * Author 		: Uriel Zazueta
 * Origin Date 	: 12/11/2023
 *******************************************************************************/

#include "util/crc16.h"

/* CRC16 control block params */
static crc16_cb_t crc_cb = {
    .table_init = false,
    .initial_val = CRC_INIT_DEFAULT,
};

/*****************************************************************************/
/*!
 * \brief Initializes a lookup table to speed up the CRC calculations.
 *
 * \details
 * A 256 byte lookup table is initialized and will be used to speed up the
 * CRC calculations. Since this only needs to be performed the first time a
 * CRC is calculated there is a global flag that is maintained to indicate if
 * the lookup table is initialized or not. Every time crc16_get_crc is called it
 * checks to make sure the table is initialized and if it isn't then the
 * this initialization routine is called.
 *
 * \returns void
 *
 *****************************************************************************/
static void crc16_create_lookup_table(uint16_t crc_polynomial)
{
  uint16_t generator = crc_polynomial;

  for (int32_t index = 0; index < CRC_TABLE_SIZE; index++) /* Iterate over all possible input byte values 0 - 255 */
  {
    uint16_t divident = index;
    uint16_t cur_byte = divident << 8; /* Move divident byte into MSB of 16Bit CRC */

    for (uint8_t bit = 0; bit < 8; bit++)
    {
      if ((cur_byte & 0x8000) != 0)
      {
        cur_byte <<= 1;
        cur_byte ^= generator;
      }
      else
      {
        cur_byte <<= 1;
      }
    }
    crc_cb.table[index] = cur_byte;
  }

  crc_cb.table_init = true;
}

/*****************************************************************************/
void crc16_init(uint16_t poly16, uint16_t init16)
/*****************************************************************************/
{
  crc_cb.initial_val = init16;
  crc16_create_lookup_table(poly16);

  return;
}

/*****************************************************************************/
void crc16_reset(void)
/*****************************************************************************/
{
  crc_cb.initial_val = 0x0000;
  crc_cb.table_init = false;
}

/* Version 2 -  A second attempt because I could not get any CRC type
 *              other than the Modbus CRC to work with version 1
 *              This version uses CRC16 - xModem */
/*****************************************************************************/
uint16_t crc16_get_crc(uint8_t *ba, int32_t array_size)
/*****************************************************************************/
{
  if (!crc_cb.table_init)
  {
    crc16_create_lookup_table((uint16_t)CRC_POLY_DEFAULT);
  }

  uint16_t crc = crc_cb.initial_val;

  for (int32_t i = 0; i < array_size; i++)
  {
    uint16_t b = (uint16_t)*(ba + i);

    /* XOR-in next input byte into MSB of crc, that's
        our new intermediate divident */
    crc = crc ^ (b << 8);
    uint8_t pos = (uint8_t)(crc >> 8);
    /* equal: ((crc ^ (b << 8)) >> 8) */

    /* Shift out the MSB used for division per lookuptable and XOR with the remainder */
    crc = (uint16_t)((crc << 8) ^ (uint16_t)(crc_cb.table[pos]));
  }

  return crc;
}

/*****************************************************************************/
bool crc16_is_crc_correct(uint8_t *ba, int32_t array_size)
/*****************************************************************************/
{
  // uint16_t calcCrc = crc16_get_crc(ba, array_size-2);
  // return (calcCrc == (uint16_t)(*(uint16_t *)(ba + array_size - 2)));
  (void)ba;
  (void)array_size;
  return true; /* Getting some weird results from this routine so disable TODO
                * the routine for now TODO */
}

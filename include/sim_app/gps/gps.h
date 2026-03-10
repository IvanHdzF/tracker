#ifndef GPS_H
#define GPS_H

#include <zephyr/drivers/modem/simcom-sim7000.h>

#ifdef __cplusplus
extern "C" {    
#endif



int sim_gps_start(void);

int sim_gps_get_data(struct sim7000_gnss_data *data);

int sim_gps_stop(void);

int sim_gps_start_xtra(void);

#ifdef __cplusplus
}       
#endif

#endif /* GPS_H */
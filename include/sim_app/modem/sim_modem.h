#ifndef SIM_MODEM_H
#define SIM_MODEM_H

#ifdef __cplusplus
extern "C" {    
#endif

int sim_modem_check_ready(void);
void sim_modem_print_info(void);

#ifdef __cplusplus
}       
#endif

#endif /* SIM_MODEM_H */
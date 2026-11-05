#ifndef BSP_ADC_H
#define BSP_ADC_H

void init_vrefint_reciprocal(void);
float get_temperature(void);
float get_battery_voltage(void);

#endif

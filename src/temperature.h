#if !defined(_TEMPERATURE_H_)
    #define _TEMPERATURE_H_

#include <math.h>


#define CONSTANTE_RESISTENCIA_1 10000
#define CONSTANTE_A_TEMP_1 0.001129148
#define CONSTANTE_B_TEMP_1 0.000234125
#define CONSTANTE_C_TEMP_1 0.0000000876741

#define CONSTANTE_RESISTENCIA_2 10000
#define CONSTANTE_A_TEMP_2 0.001129148
#define CONSTANTE_B_TEMP_2 0.000234125
#define CONSTANTE_C_TEMP_2 0.0000000876741

float resistanceFromAdc(float adcReading) {
    if (adcReading > 3.28)
        adcReading = 2;
    return CONSTANTE_RESISTENCIA_1 * adcReading / (3.29 - adcReading);
}

float temperatureFromResistance(float resistance) {
    float invTemperaturaK = CONSTANTE_A_TEMP_1 + CONSTANTE_B_TEMP_1 * log(resistance) + CONSTANTE_C_TEMP_1 * pow(log(resistance), 3);
    if (invTemperaturaK != 0.0)
        return -273.15 + 1.0 / invTemperaturaK;
    else
        return 0.001;
}


#endif // _TEMPERATURE_H_

# 1 "delay.c"
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 288 "<built-in>" 3
# 1 "<command line>" 1
# 1 "<built-in>" 2
# 1 "/Applications/microchip/mplabx/v6.15/packs/Microchip/PIC18Fxxxx_DFP/1.4.151/xc8/pic/include/language_support.h" 1 3
# 2 "<built-in>" 2
# 1 "delay.c" 2
# 1 "./delay.h" 1
# 44 "./delay.h"
# 1 "./stdutils.h" 1
# 48 "./stdutils.h"
typedef signed char sint8_t;
typedef unsigned char uint8_t;

typedef int sint16_t;
typedef unsigned short uint16_t;

typedef signed long int sint32_t;
typedef unsigned long uint32_t;
# 143 "./stdutils.h"
typedef enum
{
   E_FALSE,
   E_TRUE
}Boolean_et;

typedef enum
{
    E_FAILED,
    E_SUCCESS,
    E_BUSY,
    E_TIMEOUT
}Status_et;

typedef enum
{
 E_BINARY=2,
 E_DECIMAL = 10,
 E_HEX = 16
}NumericSystem_et;
# 45 "./delay.h" 2
# 63 "./delay.h"
void DELAY_us(uint16_t us_count);
void DELAY_ms(uint16_t ms_count);
void DELAY_sec(uint16_t var_delaySecCount_u16);
# 2 "delay.c" 2
# 15 "delay.c"
void DELAY_us(uint16_t us_count)
{
    while (us_count != 0)
    {
        us_count--;
    }
}
# 33 "delay.c"
void DELAY_ms(uint16_t ms_count)
{
    while (ms_count != 0)
    {
        DELAY_us(300u);
        ms_count--;
    }
}
# 54 "delay.c"
void DELAY_sec(uint16_t sec_count)
{
    while (sec_count != 0) {
        DELAY_ms(1000);
        sec_count--;
    }
}

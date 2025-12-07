/*
 * File:   main_piee2.c
 * Autor: Welington Correia
 * RA: 10.636.77
 * Sequenciador eletrônico para Limpeza de Filtro de Manga
 * 
 * Created on 06 de dezembro de 2025, 17:36
 * Base copia piee1
 */

#include <stdio.h>
#include "config_header.h"
#include <pic18f4550.h>
#include "msdelay.h"
#include "lcd_4b.h"
#include "i2c.h"


/*********************Definições beep*****************************/
#define beep        LATD0  
/*********************Definições led*****************************/
#define led_run     LATD1  
#define falha       LATD2 
#define ativo       LATD3
/*********************Definições reles*****************************/
#define KEY2_TRIS		(TRISAbits.TRISA0) //key1
#define	KEY2_IO          (PORTAbits.RA0)
#define KEY1_TRIS		(TRISAbits.TRISA0) //key2
#define	KEY1_IO          (PORTAbits.RA0)
#define KEY4_TRIS		(TRISAbits.TRISA0) //key3
#define	KEY4_IO          (PORTAbits.RA0)
#define KEY3_TRIS		(TRISAbits.TRISA0) //key4
#define	KEY3_IO          (PORTAbits.RA0)
/*********************Definições saídas*****************************/
#define V1          LATC0  
#define V2          LATC1   
#define V3          LATC2 
#define V4          LATC4
#define V5          LATC5
#define V6          LATC6 
#define V7          LATC7 
#define V8          LATD7
/*********************Definições botoes*****************************/
#define Btliga      PORTCbits.RC0  
#define Btdesliga   PORTCbits.RC1
#define Btinterlock PORTCbits.RC2 

/******************** I2C *****************************************/
//#define device_id_write 0xD0
//#define device_id_read 0xD1
//int sec,min,hour;
//int Day,Date,Month,Year;
////I2c
//char secs[10],mins[10],hours[10];
//char date[10],month[10],year[10];
//char Clock_type = 0x06;
//char AM_PM = 0x05;
//char days[7] = {'0','1','2','3','4','5','6'};
/*********************Definições FSM*****************************/
typedef enum {
    STATE_IDLE,         // Estado inicial/ocioso
    STATE_INIT,         // Inicialização do sistema
    STATE_READY,        // Pronto para operar
    STATE_STARTING,     // Partida do motor (estrela)
    STATE_RUNNING,      // Motor em funcionamento (triângulo)
    STATE_STOPPING,     // Parada do motor
    STATE_BLOCKED,      // Bloqueado por intertravamento
    STATE_FAILURE       // Estado de falha
} system_state_t;

typedef struct {
    system_state_t  current_state;
    system_state_t  next_state;
    unsigned int    timer_counter;
    unsigned int    start_timer;
    unsigned int    blink_counter;
    unsigned int    beep_counter;
    unsigned char   last_liga_state;
    unsigned char   last_desliga_state;
    unsigned char   last_interlock_state;
} fsm_data_t;

//DS1307 I2C
typedef struct
{
  uint8_t sec;
  uint8_t min;
  uint8_t hour;
  uint8_t weekDay;
  uint8_t date;
  uint8_t month;
  uint8_t year;  
}rtc_t;

rtc_t rtc;
char data[50];

#define C_Ds1307ReadMode_U8   0xD1u  // DS1307 ID
#define C_Ds1307WriteMode_U8  0xD0u  // DS1307 ID

#define C_Ds1307SecondRegAddress_U8   0x00u   // Address to access Ds1307 SEC register
#define C_Ds1307DateRegAddress_U8     0x04u   // Address to access Ds1307 DATE register
#define C_Ds1307ControlRegAddress_U8  0x07u   // Address to access Ds1307 CONTROL register

/*********************Declarações Proto-Type*****************************/

     

void Message(unsigned int);
void Beep(unsigned int);

//void RTC_Read_Clock(char read_clock_address);
//void RTC_Read_Calendar(char read_calendar_address);
void RTC_Init(void);
void RTC_SetDateTime(rtc_t *rtc);
void RTC_GetDateTime(rtc_t *rtc);

void menu_ajustar_rtc(void);
void converteDecToHex(int converte);

uint8_t acerto;

// Funções da FSM
void FSM_Init(fsm_data_t *fsm);
void FSM_Update(fsm_data_t *fsm);
void FSM_ExecuteState(fsm_data_t *fsm);
void FSM_CheckTransitions(fsm_data_t *fsm);
void FSM_UpdateDisplay(fsm_data_t *fsm);
void atualiza_data();

/*********************Variáveis Globais*****************************/
fsm_data_t system_fsm;

int main(void)
{
    // Configuração do sistema
    OSCCON = 0x72; 
                   
	// Inicialização de portas
    LCD_Init();
    
    RTC_Init();
    rtc.hour = 0x19; //  10:40:20 am
    rtc.min =  0x31;
    rtc.sec =  0x30;

    rtc.date = 0x03; //1st Jan 2016
    rtc.month = 0x03;
    rtc.year = 0x21;
    rtc.weekDay = 3; // Friday: 5th day of week considering monday as first day.
    TRISD = 0x00;
    TRISC = 0x00;
    PORTC = 0x00;
    
    // Inicialização dos relés


    // Inicialização da FSM
    FSM_Init(&system_fsm);    

    // Sequência inicial    
    Beep(2);
    Message(1);
    MSdelay(3000);
    Message(2);
    MSdelay(3000);
    Message(3);
    MSdelay(3000);
    Message(4);
    Beep(1);
    MSdelay(3000);

 
	while(1){
//        RTC_Read_Clock(0);              /*gives second,minute and hour*/
//        I2C_Stop();
                
        // Atualização da FSM (não bloqueante)
        FSM_Update(&system_fsm);
        
        // Pequeno delay para estabilidade
        MSdelay(10); 
    }		
}

/****************************Funções FSM********************************/

void FSM_Init(fsm_data_t *fsm)
{
    fsm->current_state = STATE_INIT;
    fsm->next_state = STATE_INIT;
    fsm->timer_counter = 0;
    fsm->start_timer = 0;
    fsm->blink_counter = 0;
    fsm->beep_counter = 0;
    fsm->last_liga_state = 1;        // Assume botão não pressionado (pull-up)
    fsm->last_desliga_state = 1;
    fsm->last_interlock_state = 1;
    
    // Inicialização dos relés

    ativo = 0;
    falha = 0;
    led_run = 0;
}

void FSM_Update(fsm_data_t *fsm)
{
    // Verifica transições de estado
    FSM_CheckTransitions(fsm);
    
    // Executa ações do estado atual
    FSM_ExecuteState(fsm);
    
    // Atualiza display
    FSM_UpdateDisplay(fsm);
    
    // Atualiza contadores de tempo
    fsm->timer_counter++;
    fsm->blink_counter++;
    
    // Controla o LED de run (pisca a cada 500ms)
    if(fsm->blink_counter >= 50) {  // 50 * 10ms = 500ms
        fsm->blink_counter = 0;
        led_run ^= 1;
    }
}
void FSM_CheckTransitions(fsm_data_t *fsm)
{
    unsigned char current_interlock = Btinterlock;
    
    // Verifica mudança no botão interlock (borda de descida)
    unsigned char interlock_pressed = (fsm->last_interlock_state == 1 && current_interlock == 0);
    fsm->last_interlock_state = current_interlock;
    
    // Verifica botão liga (borda de descida)
    unsigned char liga_pressed = (fsm->last_liga_state == 1 && Btliga == 0);
    fsm->last_liga_state = Btliga;
    
    // Verifica botão desliga (borda de descida)
    unsigned char desliga_pressed = (fsm->last_desliga_state == 1 && Btdesliga == 0);
    fsm->last_desliga_state = Btdesliga;
    
    // Transições baseadas no estado atual
    switch(fsm->current_state)
    {
        case STATE_INIT:
            if(fsm->timer_counter > 300) {  // 3 segundos de inicialização
                fsm->next_state = STATE_READY;
            }
            break;
            
        case STATE_READY:
            if(current_interlock == 0) {  // Intertravamento ativo
                fsm->next_state = STATE_BLOCKED;
            }
            else if(liga_pressed) {
                fsm->next_state = STATE_STARTING;
                fsm->start_timer = 0;
            }
            break;
            
        case STATE_STARTING:
            if(current_interlock == 0) {
                fsm->next_state = STATE_STOPPING;
            }
            else if(fsm->start_timer >= 30) {  // 30 segundos em estrela
                fsm->next_state = STATE_RUNNING;
            }
            else if(desliga_pressed) {
                fsm->next_state = STATE_STOPPING;
            }
            break;
            
        case STATE_RUNNING:
            if(current_interlock == 0) {
                fsm->next_state = STATE_STOPPING;
            }
            else if(desliga_pressed) {
                fsm->next_state = STATE_STOPPING;
            }
            break;
            
        case STATE_STOPPING:
            if(fsm->timer_counter > 50) {  // 500ms para desligar
                if(current_interlock == 0) {
                    fsm->next_state = STATE_BLOCKED;
                }
                else {
                    fsm->next_state = STATE_READY;
                }
            }
            break;
            
        case STATE_BLOCKED:
            if(current_interlock == 1) {  // Intertravamento liberado
                fsm->next_state = STATE_READY;
            }
            break;
            
        case STATE_FAILURE:
            // Recuperação manual após reset
            if(fsm->timer_counter > 1000) {  // 10 segundos
                fsm->next_state = STATE_READY;
            }
            break;
            
        default:
            fsm->next_state = STATE_IDLE;
            break;
    }
     // Aplica transição de estado
    if(fsm->current_state != fsm->next_state) {
        fsm->current_state = fsm->next_state;
        fsm->timer_counter = 0;  // Reinicia timer ao mudar de estado
    }
}
void FSM_ExecuteState(fsm_data_t *fsm)
{
    switch(fsm->current_state)
    {
        case STATE_IDLE:
            // Nada a fazer
            break;
            
        case STATE_INIT:
            // Inicialização do sistema
            ativo = 0;
            break;
            
        case STATE_READY:
            // Estado pronto
            ativo = 0;
            break;
            
        case STATE_STARTING:
            // Partida em estrela
//            k1 = 1;
//            k3 = 1;
//            k2 = 0;
            ativo = 0;
            
            // Incrementa timer de partida
            if(fsm->timer_counter >= 100) {  // 100 * 10ms = 1 segundo
                fsm->timer_counter = 0;
                fsm->start_timer++;
            }
            break;
            
        case STATE_RUNNING:
            // Funcionamento em triângulo
//            k1 = 1;
//            k2 = 1;
//            k3 = 0;
            ativo = 0;
            break;
            
        case STATE_STOPPING:
            // Desligamento
//            k1 = 0;
//            k2 = 0;
//            k3 = 0;
            break;
            
        case STATE_BLOCKED:
            // Bloqueado por intertravamento
//            k1 = 0;
//            k2 = 0;
//            k3 = 0;
            ativo = 1;
            break;
            
        case STATE_FAILURE:
            // Estado de falha
//            k1 = 0;
//            k2 = 0;
//            k3 = 0;
            falha = 1;
            break;
    }
}
void FSM_UpdateDisplay(fsm_data_t *fsm)
{
    // Atualiza display apenas a cada 100ms para evitar flicker
    static unsigned int display_counter = 0;
    display_counter++;
    
    if(display_counter >= 10) {  // 10 * 10ms = 100ms
        display_counter = 0;
        
        switch(fsm->current_state)
        {
            case STATE_INIT:
                LCD_String_xy(2,1,"Status: Inicializando");
                break;
                
            case STATE_READY:
                LCD_String_xy(2,1,"Status: Pronto       ");
                break;
                
            case STATE_STARTING:
                LCD_String_xy(2,1,"Status: Partindo     ");
                break;
                
            case STATE_RUNNING:
                LCD_String_xy(2,1,"Status: Funcionando  ");
                break;
                
            case STATE_STOPPING:
                LCD_String_xy(2,1,"Status: Parando      ");
                break;
                
            case STATE_BLOCKED:
                LCD_String_xy(2,1,"Status: Bloqueado    ");
                break;
                
            case STATE_FAILURE:
                LCD_String_xy(2,1,"Status: Falha        ");
                break;
                
            default:
                LCD_String_xy(2,1,"Status: Desconhecido ");
                break;
        }
    }
}
/****************************Functions********************************/

void Message(unsigned int msg){
    LCD_Clear();
    switch(msg){
        case 1:
            LCD_String_xy(1,7,"UNIUBE");
            LCD_String_xy(2,0,"Engenharia Eletrica");
            LCD_String_xy(3,2,"Projeto Integrado");
            LCD_String_xy(4,3,"Em Eletrica II");
            break;
        case 2:
            LCD_String_xy(1,2,"Welington Correia");
            LCD_String_xy(2,4,"RA : 1063677");
            LCD_String_xy(3,1,"Seq.Eletr.Limpeza");
            LCD_String_xy(4,3,"Filtro de Manga");
            break;
        case 3:
            LCD_String_xy(4,2,"Seja Bem Vindo!");
            atualiza_data();
 
            break;
        case 4:
//            LCD_String_xy(1,1,"Filtro Manga");
//            LCD_String_xy(2,1,"Status:");
//            LCD_String_xy(3,1,"Temp. Motor:##,#");
//            LCD_String_xy(4,1,"Temp. Macal:##,#");
              atualiza_data();

              LCD_String_xy(4,2,"Filtro Manga");
            break;
    }

}
//Sequenciador eletrônico para Limpeza de Filtro de Manga

void Beep(unsigned val){
 unsigned int i;
 for(i=0;i<val;i++)
    {
     beep = 1;
     MSdelay(500);
     beep = 0;
     MSdelay(500);
    }

 
}


/************************ I2C ******************************/
//void RTC_Read_Clock(char read_clock_address)
//{
//////    I2C_Start(device_id_write);
////    I2C_Start();
////    I2C_Write(read_clock_address);     /* address from where time needs to be read*/
//////    I2C_Repeated_Start(device_id_read);
////    sec = I2C_Read(0);                 /*read data and send ack for continuous reading*/
////    min = I2C_Read(0);                 /*read data and send ack for continuous reading*/
////    hour= I2C_Read(1);                 /*read data and send nack for indicating stop reading*/
//    
//}

//void RTC_Read_Calendar(char read_calendar_address)
//{   
//////    I2C_Start(device_id_write);
////    I2C_Start();
////    I2C_Write(read_calendar_address); /* address from where time needs to be read*/
//////    I2C_Repeated_Start(device_id_read);
////    Day = I2C_Read(0);                /*read data and send ack for continuous reading*/
////    Date = I2C_Read(0);               /*read data and send ack for continuous reading*/
////    Month = I2C_Read(0);              /*read data and send ack for continuous reading*/
////    Year = I2C_Read(1);               /*read data and send nack for indicating stop reading*/
////    I2C_Stop();
//}

void atualiza_data(){
    RTC_GetDateTime(&rtc);
    sprintf(data,"Hora:%2x:%2x:%2x    AB\nDate:%2x/%2x/%2x",(uint16_t)rtc.hour,(uint16_t)rtc.min,(uint16_t)rtc.sec,(uint16_t)rtc.date,(uint16_t)rtc.month,(uint16_t)rtc.year);
    LCD_String_xy(2,0,data);

//    if(hour & (1<<Clock_type)){     /* check clock is 12hr or 24hr */
//
//         if(hour & (1<<AM_PM)){      /* check AM or PM */
//             LCD_String_xy(1,17,"PM");
//         }
//         else{
//             LCD_String_xy(1,17,"AM");
//         }
//
//         hour = hour & (0x1f);
//         sprintf(secs,"%x ",sec);   /*%x for reading BCD format from RTC DS1307*/
//         sprintf(mins,"%x:",min);    
//         sprintf(hours,"Hora:%x:",hour);  
//         LCD_String_xy(1,0,hours);            
//         LCD_String(mins);
//         LCD_String(secs);
//  }
// else{
//
//     hour = hour & (0x3f);
//     sprintf(secs,"%x ",sec);   /*%x for reading BCD format from RTC DS1307*/
//     sprintf(mins,"%x:",min);    
//     sprintf(hours,"Hora:%x:",hour);  
//     LCD_String_xy(1,0,hours);            
//     LCD_String(mins);
//     LCD_String(secs); 
// }
//
//     RTC_Read_Calendar(3);        /*gives day, date, month, year*/        
//     I2C_Stop();
//     sprintf(date,"Data %x-",Date);
//     sprintf(month,"%x-",Month);
//     sprintf(year,"%x ",Year);
//     LCD_String_xy(2,0,date);
//     LCD_String(month);
//     LCD_String(year);
//
//     /* find day */
//     switch(days[Day])
//     {
//         case '0':
//                     LCD_String("Dom");
//                     break;
//         case '1':
//                     LCD_String("Seg");
//                     break;                
//         case '2':
//                     LCD_String("Tec");
//                     break;                
//         case '3':   
//                     LCD_String("Qua");
//                     break;                
//         case '4':
//                     LCD_String("Qui");
//                     break;
//         case '5':
//                     LCD_String("Sex");
//                     break;                
//         case '6':
//                     LCD_String("Sab");
//                     break;
//         default: 
//                     break;
//
//     }
}

/***************************************************************************************************
                         void RTC_Init(void)
****************************************************************************************************
 * I/P Arguments: none.
 * Return value    : none

 * description :This function is used to Initialize the Ds1307 RTC.
***************************************************************************************************/
void RTC_Init(void)
{
    I2C_Init();                             // Initialize the I2c module.
    I2C_Start();                            // Start I2C communication

    I2C_Write(C_Ds1307WriteMode_U8);        // Connect to DS1307 by sending its ID on I2c Bus
    I2C_Write(C_Ds1307ControlRegAddress_U8);// Select the Ds1307 ControlRegister to configure Ds1307

    I2C_Write(0x00);                        // Write 0x00 to Control register to disable SQW-Out

    I2C_Stop();                             // Stop I2C communication after initializing DS1307
}

/***************************************************************************************************
                    void RTC_SetDateTime(rtc_t *rtc)
****************************************************************************************************
 * I/P Arguments: rtc_t *: Pointer to structure of type rtc_t. Structure contains hour,min,sec,day,date,month and year 
 * Return value    : none

 * description  :This function is used to update the Date and time of Ds1307 RTC.
                 The new Date and time will be updated into the non volatile memory of Ds1307.
        Note: The date and time should be of BCD format, 
             like 0x12,0x39,0x26 for 12hr,39min and 26sec.    
                  0x15,0x08,0x47 for 15th day,8th month and 47th year.                 
***************************************************************************************************/
void RTC_SetDateTime(rtc_t *rtc)
{
    I2C_Start();                          // Start I2C communication

    I2C_Write(C_Ds1307WriteMode_U8);      // connect to DS1307 by sending its ID on I2c Bus
    I2C_Write(C_Ds1307SecondRegAddress_U8); // Request sec RAM address at 00H

    I2C_Write(rtc->sec);                    // Write sec from RAM address 00H
    I2C_Write(rtc->min);                    // Write min from RAM address 01H
    I2C_Write(rtc->hour);                    // Write hour from RAM address 02H
    I2C_Write(rtc->weekDay);                // Write weekDay on RAM address 03H
    I2C_Write(rtc->date);                    // Write date on RAM address 04H
    I2C_Write(rtc->month);                    // Write month on RAM address 05H
    I2C_Write(rtc->year);                    // Write year on RAM address 06h

    I2C_Stop();                              // Stop I2C communication after Setting the Date
}
/***************************************************************************************************
                     void RTC_GetDateTime(rtc_t *rtc)
****************************************************************************************************
 * I/P Arguments: rtc_t *: Pointer to structure of type rtc_t. Structure contains hour,min,sec,day,date,month and year 
 * Return value    : none

 * description  :This function is used to get the Date(d,m,y) from Ds1307 RTC.

    Note: The date and time read from Ds1307 will be of BCD format, 
          like 0x12,0x39,0x26 for 12hr,39min and 26sec.    
               0x15,0x08,0x47 for 15th day,8th month and 47th year.              
***************************************************************************************************/
void RTC_GetDateTime(rtc_t *rtc)
{
    I2C_Start();                            // Start I2C communication

    I2C_Write(C_Ds1307WriteMode_U8);        // connect to DS1307 by sending its ID on I2c Bus
    I2C_Write(C_Ds1307SecondRegAddress_U8); // Request Sec RAM address at 00H

    I2C_Stop();                                // Stop I2C communication after selecting Sec Register

    I2C_Start();                            // Start I2C communication
    I2C_Write(C_Ds1307ReadMode_U8);            // connect to DS1307(Read mode) by sending its ID

    rtc->sec = I2C_Read(1);                // read second and return Positive ACK
    rtc->min = I2C_Read(1);                 // read minute and return Positive ACK
    rtc->hour= I2C_Read(1);               // read hour and return Negative/No ACK
    rtc->weekDay = I2C_Read(1);           // read weekDay and return Positive ACK
    rtc->date= I2C_Read(1);              // read Date and return Positive ACK
    rtc->month=I2C_Read(1);            // read Month and return Positive ACK
    rtc->year =I2C_Read(0);             // read Year and return Negative/No ACK

    I2C_Stop();                              // Stop I2C communication after reading the Date
}

//Ajustar RTC DF1307
void menu_ajustar_rtc(void){
    char data[50];
    unsigned int i=0;
    char cvt1=0;
    LCD_Command(0x01);
    LCD_String_xy(1,1,"Ajustar Data e Hora ");
    
    while(1){
        if(i<200){
            LCD_Command(0x01);
            for(i=0; i<200; i++){
                LCD_String_xy(1,1,"Ajustar Hora        ");
                RTC_GetDateTime(&rtc); 
                sprintf(data,"Hora:%2x:%2x:%2x    \nDate:%2x/%2x/%0x",(uint16_t)rtc.hour,(uint16_t)rtc.min,(uint16_t)rtc.sec,(uint16_t)rtc.date,(uint16_t)rtc.month,(uint16_t)rtc.year);
                LCD_String_xy(2,1,data);

                if(!KEY2_IO){
                    while(!KEY2_IO);
                    break;

                }
                else if (!KEY3_IO){
                    i=0;
                    while(!KEY3_IO);MSdelay(300);
                    cvt1++;
                    if(cvt1>23){cvt1=0;}
                    converteDecToHex(cvt1);
                    rtc.hour = acerto;
                    RTC_SetDateTime(&rtc);
                }
                else if (!KEY4_IO){
                    i=0;
                    while(!KEY4_IO);MSdelay(300);
                    cvt1--;
                    if(cvt1 == 255){cvt1 = 23;}
                    converteDecToHex(cvt1);
                    rtc.hour = acerto;
                    RTC_SetDateTime(&rtc);
                }


            }
            LCD_Command(0x01);
            for(i=0; i<200; i++){
                LCD_String_xy(1,1,"Ajustar Minutos     ");
                RTC_GetDateTime(&rtc); 
                sprintf(data,"Hora:%2x:%2x:%2x    \nDate:%2x/%2x/%0x",(uint16_t)rtc.hour,(uint16_t)rtc.min,(uint16_t)rtc.sec,(uint16_t)rtc.date,(uint16_t)rtc.month,(uint16_t)rtc.year);
                LCD_String_xy(2,1,data);

                if(!KEY2_IO){
                    while(!KEY2_IO);
                    break;

                }
                else if (!KEY3_IO){
                    i=0;
                    while(!KEY3_IO);MSdelay(300);
                    cvt1++;
                    if(cvt1>59){cvt1=0;}
                    converteDecToHex(cvt1);
                    rtc.min = acerto;
                    RTC_SetDateTime(&rtc);
                }
                else if (!KEY4_IO){
                    i=0;
                    while(!KEY4_IO);MSdelay(300);
                    cvt1--;
                    if(cvt1 == 255){cvt1 = 59;}
                    converteDecToHex(cvt1);
                    rtc.min = acerto;
                    RTC_SetDateTime(&rtc);
                }


            }
            
            LCD_Command(0x01);
            for(i=0; i<200; i++){
                LCD_String_xy(1,1,"Ajustar Dia         ");
                RTC_GetDateTime(&rtc); 
                sprintf(data,"Hora:%2x:%2x:%2x    \nDate:%2x/%2x/%0x",(uint16_t)rtc.hour,(uint16_t)rtc.min,(uint16_t)rtc.sec,(uint16_t)rtc.date,(uint16_t)rtc.month,(uint16_t)rtc.year);
                LCD_String_xy(2,1,data);

                if(!KEY2_IO){
                    while(!KEY2_IO);
                    break;

                }
                else if (!KEY3_IO){
                    i=0;
                    while(!KEY3_IO);MSdelay(300);
                    cvt1++;
                    if(cvt1>31){cvt1=1;}
                    converteDecToHex(cvt1);
                    rtc.date = acerto;
                    RTC_SetDateTime(&rtc);
                }
                else if (!KEY4_IO){
                    i=0;
                    while(!KEY4_IO);MSdelay(300);
                    cvt1--;
                    if(cvt1 == 255){cvt1 = 31;}
                    converteDecToHex(cvt1);
                    rtc.date = acerto;
                    RTC_SetDateTime(&rtc);
                }


            }            
            
            LCD_Command(0x01);
            for(i=0; i<200; i++){
                LCD_String_xy(1,1,"Ajustar Mes         ");
                RTC_GetDateTime(&rtc); 
                sprintf(data,"Hora:%2x:%2x:%2x    \nDate:%2x/%2x/%0x",(uint16_t)rtc.hour,(uint16_t)rtc.min,(uint16_t)rtc.sec,(uint16_t)rtc.date,(uint16_t)rtc.month,(uint16_t)rtc.year);
                LCD_String_xy(2,1,data);

                if(!KEY2_IO){
                    while(!KEY2_IO);
                    break;

                }
                else if (!KEY3_IO){
                    i=0;
                    while(!KEY3_IO);MSdelay(300);
                    cvt1++;
                    if(cvt1>12){cvt1=1;}
                    converteDecToHex(cvt1);
                    rtc.month = acerto;
                    RTC_SetDateTime(&rtc);
                }
                else if (!KEY4_IO){
                    i=0;
                    while(!KEY4_IO);MSdelay(300);
                    cvt1--;
                    if(cvt1 == 255){cvt1 = 12;}
                    converteDecToHex(cvt1);
                    rtc.month = acerto;
                    RTC_SetDateTime(&rtc);
                }


            }            
            
            
            LCD_Command(0x01);
            for(i=0; i<200; i++){
                LCD_String_xy(1,1,"Ajustar Ano         ");
                RTC_GetDateTime(&rtc); 
                sprintf(data,"Hora:%2x:%2x:%2x    \nDate:%2x/%2x/%0x",(uint16_t)rtc.hour,(uint16_t)rtc.min,(uint16_t)rtc.sec,(uint16_t)rtc.date,(uint16_t)rtc.month,(uint16_t)rtc.year);
                LCD_String_xy(2,1,data);

                if(!KEY2_IO){
                    while(!KEY2_IO);
                    break;

                }
                else if (!KEY3_IO){
                    i=0;
                    while(!KEY3_IO);MSdelay(300);
                    cvt1++;
                    if(cvt1>99){cvt1=1;}
                    converteDecToHex(cvt1);
                    rtc.year = acerto;
                    RTC_SetDateTime(&rtc);
                }
                else if (!KEY4_IO){
                    i=0;
                    while(!KEY4_IO);MSdelay(300);
                    cvt1--;
                    if(cvt1 == 255){cvt1 = 99;}
                    converteDecToHex(cvt1);
                    rtc.year = acerto;
                    RTC_SetDateTime(&rtc);
                }


            }     
            
           
        }
        i = 200;
        LCD_Command(0x01);    
        LCD_String_xy(1,1,"Sair Ajuste         ");        
        if(KEY2_IO){
            while(KEY2_IO);
            break;

        }        
        
    }





}


void converteDecToHex(int converte){
    switch(converte){
        case 0:
            acerto = 0x00;
            break;
        case 1:
            acerto = 0x01;
            break;
        case 2:
            acerto = 0x02;
            break;
        case 3:
            acerto = 0x03;
            break;
        case 4:
            acerto = 0x04;
            break;
        case 5:
            acerto = 0x05;
            break;
        case 6:
            acerto = 0x06;
            break;
        case 7:
            acerto = 0x07;
            break;
        case 8:
            acerto = 0x08;
            break;
        case 9:
            acerto = 0x09;
            break;
        case 10:
            acerto = 0x10;
            break;
        case 11:
            acerto = 0x11;
            break;
        case 12:
            acerto = 0x12;
            break;
        case 13:
            acerto = 0x13;
            break;
        case 14:
            acerto = 0x14;
            break;
        case 15:
            acerto = 0x15;
            break;
        case 16:
            acerto = 0x16;
            break;
        case 17:
            acerto = 0x17;
            break;
        case 18:
            acerto = 0x18;
            break;
        case 19:
            acerto = 0x19;
            break;
        case 20:
            acerto = 0x20;
            break;
        case 21:
            acerto = 0x21;
            break;
        case 22:
            acerto = 0x22;
            break;
        case 23:
            acerto = 0x23;
            break;

        case 24:
            acerto = 0x24;
            break;
        case 25:
            acerto = 0x25;
            break;
        case 26:
            acerto = 0x26;
            break;
        case 27:
            acerto = 0x27;
            break;
        case 28:
            acerto = 0x28;
            break;
        case 29:
            acerto = 0x29;
            break;
        case 30:
            acerto = 0x30;
            break;
        case 31:
            acerto = 0x31;
            break;
        case 32:
            acerto = 0x32;
            break;
        case 33:
            acerto = 0x33;
            break;
        case 34:
            acerto = 0x34;
            break;
        case 35:
            acerto = 0x35;
            break;
        case 36:
            acerto = 0x36;
            break;
        case 37:
            acerto = 0x37;
            break;
        case 38:
            acerto = 0x38;
            break;
        case 39:
            acerto = 0x39;
            break;
        case 40:
            acerto = 0x40;
            break;
        case 41:
            acerto = 0x41;
            break;
        case 42:
            acerto = 0x42;
            break;
        case 43:
            acerto = 0x43;
            break;
        case 44:
            acerto = 0x44;
            break;
        case 45:
            acerto = 0x45;
            break;
        case 46:
            acerto = 0x46;
            break;
        case 47:
            acerto = 0x47;
            break;            
            
        case 48:
            acerto = 0x48;
            break;
        case 49:
            acerto = 0x49;
            break;
        case 50:
            acerto = 0x50;
            break;
        case 51:
            acerto = 0x51;
            break;
        case 52:
            acerto = 0x52;
            break;
        case 53:
            acerto = 0x53;
            break;
        case 54:
            acerto = 0x54;
            break;            
            
            
        case 55:
            acerto = 0x55;
            break;
        case 56:
            acerto = 0x56;
            break;
        case 57:
            acerto = 0x57;
            break;
        case 58:
            acerto = 0x58;
            break;
        case 59:
            acerto = 0x59;
            break;            
            
           
             
        default :
            acerto = 0x00;
            break;
            
    }
}



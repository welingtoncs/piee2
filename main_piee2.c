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
#define device_id_write 0xD0
#define device_id_read 0xD1
int sec,min,hour;
int Day,Date,Month,Year;
//I2c
char secs[10],mins[10],hours[10];
char date[10],month[10],year[10];
char Clock_type = 0x06;
char AM_PM = 0x05;
char days[7] = {'0','1','2','3','4','5','6'};
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
/*********************Declarações Proto-Type*****************************/

     

void Message(unsigned int);
void Beep(unsigned int);

void RTC_Read_Clock(char read_clock_address);
void RTC_Read_Calendar(char read_calendar_address);

// Funções da FSM
void FSM_Init(fsm_data_t *fsm);
void FSM_Update(fsm_data_t *fsm);
void FSM_ExecuteState(fsm_data_t *fsm);
void FSM_CheckTransitions(fsm_data_t *fsm);
void FSM_UpdateDisplay(fsm_data_t *fsm);

/*********************Variáveis Globais*****************************/
fsm_data_t system_fsm;

int main(void)
{
    // Configuração do sistema
    OSCCON = 0x72; 
                   
	// Inicialização de portas
    LCD_Init();
    I2C_Init();
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
        RTC_Read_Clock(0);              /*gives second,minute and hour*/
        I2C_Stop();
        
               
        
        
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
            LCD_String_xy(1,2,"Seja Bem Vindo!");
            if(hour & (1<<Clock_type)){     /* check clock is 12hr or 24hr */
            
                    if(hour & (1<<AM_PM)){      /* check AM or PM */
                        LCD_String_xy(2,14,"PM");
                    }
                    else{
                        LCD_String_xy(2,14,"AM");
                    }

                    hour = hour & (0x1f);
                    sprintf(secs,"%x ",sec);   /*%x for reading BCD format from RTC DS1307*/
                    sprintf(mins,"%x:",min);    
                    sprintf(hours,"Tim:%x:",hour);  
                    LCD_String_xy(2,0,hours);            
                    LCD_String(mins);
                    LCD_String(secs);
             }
            else{

                hour = hour & (0x3f);
                sprintf(secs,"%x ",sec);   /*%x for reading BCD format from RTC DS1307*/
                sprintf(mins,"%x:",min);    
                sprintf(hours,"Tim:%x:",hour);  
                LCD_String_xy(2,0,hours);            
                LCD_String(mins);
                LCD_String(secs); 
            }
        
                RTC_Read_Calendar(3);        /*gives day, date, month, year*/        
                I2C_Stop();
                sprintf(date,"Cal %x-",Date);
                sprintf(month,"%x-",Month);
                sprintf(year,"%x ",Year);
                LCD_String_xy(3,0,date);
                LCD_String(month);
                LCD_String(year);

                /* find day */
                switch(days[Day])
                {
                    case '0':
                                LCD_String("Dom");
                                break;
                    case '1':
                                LCD_String("Seg");
                                break;                
                    case '2':
                                LCD_String("Tec");
                                break;                
                    case '3':   
                                LCD_String("Qua");
                                break;                
                    case '4':
                                LCD_String("Qui");
                                break;
                    case '5':
                                LCD_String("Sex");
                                break;                
                    case '6':
                                LCD_String("Sab");
                                break;
                    default: 
                                break;

                }
            break;
        case 4:
            LCD_String_xy(1,1,"Filtro Manga");
            LCD_String_xy(2,1,"Status:");
            LCD_String_xy(3,1,"Temp. Motor:##,#");
            LCD_String_xy(4,1,"Temp. Macal:##,#");
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
void RTC_Read_Clock(char read_clock_address)
{
    I2C_Start(device_id_write);
    I2C_Write(read_clock_address);     /* address from where time needs to be read*/
    I2C_Repeated_Start(device_id_read);
    sec = I2C_Read(0);                 /*read data and send ack for continuous reading*/
    min = I2C_Read(0);                 /*read data and send ack for continuous reading*/
    hour= I2C_Read(1);                 /*read data and send nack for indicating stop reading*/
    
}

void RTC_Read_Calendar(char read_calendar_address)
{   
    I2C_Start(device_id_write);
    I2C_Write(read_calendar_address); /* address from where time needs to be read*/
    I2C_Repeated_Start(device_id_read);
    Day = I2C_Read(0);                /*read data and send ack for continuous reading*/
    Date = I2C_Read(0);               /*read data and send ack for continuous reading*/
    Month = I2C_Read(0);              /*read data and send ack for continuous reading*/
    Year = I2C_Read(1);               /*read data and send nack for indicating stop reading*/
    I2C_Stop();
}


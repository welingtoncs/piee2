/*
 * File:   main_piee2.c
 * Autor: Welington Correia
 * RA: 10.636.77
 * Sequenciador eletrônico para Limpeza de Filtro de Manga
 * 
 * Created on 06 de dezembro de 2025, 17:36
 * Base copia piee1
 */

#include "config_header.h"
#include <pic18f4550.h>

/*********************Definições LCD*****************************/
#define RS LATB0  
#define EN LATB1  
#define ldata LATB   
#define LCD_Port TRISB 
/*********************Definições beep*****************************/
#define beep LATD0  
/*********************Definições led*****************************/
#define led_run LATD1  
#define falha LATD2 
#define interlock LATD3
/*********************Definições reles*****************************/
#define k1 LATD4  
#define k2 LATD5  
#define k3 LATD6 
/*********************Definições otoes*****************************/
#define Btliga PORTCbits.RC0  
#define Btdesliga PORTCbits.RC1
#define Btinterlock PORTCbits.RC2 

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
    system_state_t current_state;
    system_state_t next_state;
    unsigned int timer_counter;
    unsigned int start_timer;
    unsigned int blink_counter;
    unsigned int beep_counter;
    unsigned char last_liga_state;
    unsigned char last_desliga_state;
    unsigned char last_interlock_state;
} fsm_data_t;
/*********************Declarações Proto-Type*****************************/

void MSdelay(unsigned int);       
void LCD_Init();                   
void LCD_Command(unsigned char);  
void LCD_Char(unsigned char);   
void LCD_String(const char *);    
void LCD_String_xy(char, char, const char *);
void LCD_Clear();                  
void Message(unsigned int);
void Beep(unsigned int);

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
    TRISD = 0x00;
    TRISC = 0xFF;
    PORTC = 0x00;
    
    // Inicialização dos relés
    k1 = 0;
    k2 = 0;
    k3 = 0;

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
    k1 = 0;
    k2 = 0;
    k3 = 0;
    interlock = 0;
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
            k1 = 0;
            k2 = 0;
            k3 = 0;
            interlock = 0;
            break;
            
        case STATE_READY:
            // Estado pronto
            k1 = 0;
            k2 = 0;
            k3 = 0;
            interlock = 0;
            break;
            
        case STATE_STARTING:
            // Partida em estrela
            k1 = 1;
            k3 = 1;
            k2 = 0;
            interlock = 0;
            
            // Incrementa timer de partida
            if(fsm->timer_counter >= 100) {  // 100 * 10ms = 1 segundo
                fsm->timer_counter = 0;
                fsm->start_timer++;
            }
            break;
            
        case STATE_RUNNING:
            // Funcionamento em triângulo
            k1 = 1;
            k2 = 1;
            k3 = 0;
            interlock = 0;
            break;
            
        case STATE_STOPPING:
            // Desligamento
            k1 = 0;
            k2 = 0;
            k3 = 0;
            break;
            
        case STATE_BLOCKED:
            // Bloqueado por intertravamento
            k1 = 0;
            k2 = 0;
            k3 = 0;
            interlock = 1;
            break;
            
        case STATE_FAILURE:
            // Estado de falha
            k1 = 0;
            k2 = 0;
            k3 = 0;
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
            break;
        case 4:
            LCD_String_xy(1,1,"Rele Estr-Triangulo");
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

void LCD_Init()
{
    LCD_Port = 0;       
    MSdelay(15);        
    LCD_Command(0x02);  
    LCD_Command(0x28);  
                          
	LCD_Command(0x01);  
    LCD_Command(0x0c);  
	LCD_Command(0x06);  	   
}

void LCD_Command(unsigned char cmd )
{
	ldata = (ldata & 0x0f) |(0xF0 & cmd);   
	RS = 0;   
	EN = 1;   
	NOP();
	EN = 0;
	MSdelay(1);
    ldata = (ldata & 0x0f) | (cmd<<4);  
	EN = 1;
	NOP();
	EN = 0;
	MSdelay(3);
}


void LCD_Char(unsigned char dat)
{
	ldata = (ldata & 0x0f) | (0xF0 & dat);  
	RS = 1;  
	EN = 1;  
	NOP();
	EN = 0;
	MSdelay(1);
    ldata = (ldata & 0x0f) | (dat<<4); 
	EN = 1;  
	NOP();
	EN = 0;
	MSdelay(3);
}

void LCD_String(const char *msg)
{
	while((*msg)!=0)
	{		
	  LCD_Char(*msg);
	  msg++;	
    }
}

void LCD_String_xy(char row,char pos,const char *msg)
{
    char location=0;
    if(row==1)
    {
        location=(0x80) | ((pos) & 0x0f); 
        LCD_Command(location);
    }
    if(row==2)
    {
        location=(0xC0) | ((pos) & 0x0f); 
        LCD_Command(location);
    }
    if(row==3)
    {
        location=(0x94) | ((pos) & 0x0f); 
        LCD_Command(location);
    }
    if(row==4)
    {
        location=(0xD4) | ((pos) & 0x0f);  
        LCD_Command(location);
    }
     
    LCD_String(msg);
}

void LCD_Clear()
{
   	LCD_Command(0x01);  
    MSdelay(3);
}

void MSdelay(unsigned int val)
{
 unsigned int i,j;
 for(i=0;i<val;i++)
     for(j=0;j<165;j++);  
}

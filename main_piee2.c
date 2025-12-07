/*
 * File:   main_piee2.c
 * Autor: Welington Correia
 * RA: 10.636.77
 * Sequenciador eletrônico para Limpeza de Filtro de Manga
 * 
 * Created on 06 de dezembro de 2025, 17:36
 */

#include <stdio.h>
#include <string.h>
#include "config_header.h"
#include <pic18f4550.h>
#include "msdelay.h"
#include "lcd_4b.h"
#include "i2c.h"

/**************** Saidas *****************************************/
/********************* beep*****************************/
#define beep            LATD0  
/*********************Definições led*****************************/
#define led_run         LATD1  
#define led_falha       LATD2 
#define led_ativo       LATD3

/******************** Definições saídas - válvulas ***************/
#define Val1              LATC0  
#define Val2              LATC1   
#define Val3              LATC2 
#define Val4              LATC2
#define Val5              LATC2
#define Val6              LATC6 
#define Val7              LATC7 
#define Val8              LATD7

/********************** Entradas **********************************/
/*********************Definições botões*****************************/
#define Bt_man_aut      PORTAbits.RA2 
#define Bt_inc          PORTAbits.RA3 
#define Bt_dow          PORTAbits.RA4 
#define Bt_ent          PORTAbits.RA5 
#define Bt_reset        PORTAbits.RA6

/*********************Definições Analógica*****************************/
#define An_pres         PORTAbits.RA0 

/*********************Definições do Sistema*****************************/
#define NUM_VALVULAS    8
#define TEMPO_VALVULA   200      // 2 segundos (200 * 10ms)
#define TEMPO_INTERVALO 500      // 5 segundos (500 * 10ms)
// RA: 10.636.77 ? 1+0+6+3+6+7+7 = 30 segundos
#define TEMPO_CICLO     3000     // 30 segundos (3000 * 10ms)

/******************** I2C *****************************************/
// DS1307 RTC
typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t weekDay;
    uint8_t date;
    uint8_t month;
    uint8_t year;  
} rtc_t;

// 24C32 EEPROM
#define EEPROM_ADDR_WRITE 0xA0
#define EEPROM_ADDR_READ  0xA1
#define EEPROM_CICLO_ADDR 0x0000  // Endereço para armazenar contador de ciclos

/*********************Variáveis Globais*****************************/
rtc_t rtc;
char lcd_buffer[21];  // Buffer para 20 caracteres + null terminator
char hora_buffer[9];  // HH:MM:SS
char data_buffer[11]; // DD/MM/AAAA
uint16_t contador_ciclos = 0;
uint8_t modo_operacao = 0;      // 0=Manual, 1=Automático
uint8_t valvula_selecionada = 0;
uint8_t ciclo_em_andamento = 0;
uint16_t temporizador = 0;
uint16_t adc_pressao = 0;
uint8_t falha_pressao = 0;

/*********************Declarações Prototype*****************************/
void System_Init(void);
void Display_Mensagens_Iniciais(void);
void Display_Menu_Principal(void);
void Beep(uint8_t tipo);
void Sequenciador_Automatico(void);
void Modo_Manual(void);
uint8_t Ler_EEPROM(uint16_t endereco);
void Escrever_EEPROM(uint16_t endereco, uint8_t dado);
void Ler_Contador_EEPROM(void);
void Salvar_Contador_EEPROM(void);
void RTC_Init(void);
void RTC_GetDateTime(rtc_t *rtc);
void Format_Hora(char *buffer, rtc_t *rtc);
void Format_Data(char *buffer, rtc_t *rtc);
void ADC_Init(void);
uint16_t Ler_ADC(uint8_t canal);
void Verificar_Falha_Pressao(void);
void Atualizar_Display_Principal(void);

/*********************Função Principal*****************************/
int main(void) {
    // Configuração do sistema
    OSCCON = 0x72; 
    System_Init();

    
    // Sequência inicial
    Beep(1);  // Bipe longo de 2 segundos
    Display_Mensagens_Iniciais();
    Beep(2);  // Bipe duplo
    
    // Inicializar RTC com valores padrão
//    rtc.hour = 0x12;
//    rtc.min = 0x00;
//    rtc.sec = 0x00;
//    rtc.date = 0x07;
//    rtc.month = 0x12;
//    rtc.year = 0x25;
    
    while(1) {
        // Ler pressão diferencial
        adc_pressao = Ler_ADC(0);
        
        // Verificar falha de pressão
        Verificar_Falha_Pressao();
//        
//        // Verificar botão reset
//        if(Bt_reset == 0) {
//            MSdelay(50);  // Debounce
//            if(Bt_reset == 0) {
//                if(modo_operacao == 1) {  // Modo automático
//                    contador_ciclos = 0;
//                    Salvar_Contador_EEPROM();
//                    Beep(2);  // Bipe duplo para confirmar reset
//                } else {  // Modo manual
//                    // Aciona todas as válvulas
//                    PORTC = 0xFF;
//                    Val8 = 1;
//                    MSdelay(2000);
//                    PORTC = 0x00;
//                    Val8 = 0;
//                }
//                while(Bt_reset == 0);  // Aguarda soltar botão
//            }
//        }
//        
//        // Verificar mudança de modo
//        static uint8_t last_man_aut = 1;
//        if(Bt_man_aut == 0) {
//            MSdelay(50);
//            if(Bt_man_aut == 0) {
//                modo_operacao = !modo_operacao;  // Alterna entre Manual e Auto
//                ciclo_em_andamento = 0;
//                PORTC = 0x00;  // Desliga todas as válvulas
//                Val8 = 0;
//                led_ativo = 0;
//                Beep(1);  // Bipe curto para confirmar mudança
//            }
//            while(Bt_man_aut == 0);  // Aguarda soltar botão
//        }
//        last_man_aut = Bt_man_aut;
//        
//        // Executar modo atual
//        if(modo_operacao == 0) {
//            Modo_Manual();
//        } else {
//            modo_operacao == 1;
//            Sequenciador_Automatico();
//        }
        
        // LED run piscando (indicador de sistema ativo)
        static uint16_t blink_timer = 0;
        if(blink_timer++ >= 50) {  // Pisca a cada 500ms
            blink_timer = 0;
            led_run ^= 1;
        }
        
        // Delay não-bloqueante (10ms)
        MSdelay(10);
        temporizador++;
    }
    
    return 0;
}

/*********************Inicialização do Sistema*****************************/
void System_Init(void) {
    // Inicialização de portas
    TRISD = 0x00;   // Saídas: beep, LEDs, Val8
    TRISC = 0x00;   // Saídas: Val1-Val7
    PORTC = 0x00;   // Desliga todas as válvulas
    PORTD = 0x00;   // Desliga beep e LEDs
    
    // Configurar entradas
    TRISAbits.TRISA2 = 1;  // Bt_man_aut
    TRISAbits.TRISA3 = 1;  // Bt_inc
    TRISAbits.TRISA4 = 1;  // Bt_dow
    TRISAbits.TRISA5 = 1;  // Bt_ent
    TRISAbits.TRISA6 = 1;  // Bt_reset
    TRISAbits.TRISA0 = 1;  // An_pres (ADC)
    
    // Inicializações
    LCD_Init();
    RTC_Init();
    ADC_Init();
    I2C_Init();
    
    // Ler contador de ciclos da EEPROM
    Ler_Contador_EEPROM();
}

void Display_Mensagens_Iniciais(void) {
    // Primeira tela
    LCD_Clear();
    LCD_String_xy(1, 7, "UNIUBE");
    LCD_String_xy(2, 0, "Engenharia Eletrica");
    LCD_String_xy(3, 2, "Projeto Integrado");
    LCD_String_xy(4, 3, "Em Eletrica II");
    MSdelay(3000);
    
    // Segunda tela
    LCD_Clear();
    LCD_String_xy(1, 2, "Welington Correia");
    LCD_String_xy(2, 4, "RA : 1063677");
    LCD_String_xy(3, 1, "Seq.Eletr.Limpeza");
    LCD_String_xy(4, 3, "Filtro de Manga");
    MSdelay(3000);
    
    // Terceira tela
    LCD_Clear();
    LCD_String_xy(1, 2, "Seja Bem Vindo!");
    
    // Obter data e hora do RTC
    RTC_GetDateTime(&rtc);
    Format_Hora(hora_buffer, &rtc);
    Format_Data(data_buffer, &rtc);
    
    LCD_String_xy(2, 0, hora_buffer);
    LCD_String_xy(3, 0, data_buffer);
    
    // Mostrar número de ciclos
    sprintf(lcd_buffer, "Ciclos: %04d", contador_ciclos);
    LCD_String_xy(4, 0, lcd_buffer);
    MSdelay(3000);
    
    // Quarta tela (tela principal inicial)
    LCD_Clear();
    LCD_String_xy(1, 3, "Filtro de Manga");
    
    // Mostrar data e hora juntos
    sprintf(lcd_buffer, "%s  %s", hora_buffer, data_buffer);
    LCD_String_xy(2, 0, lcd_buffer);
    
    // Modo de operação
    LCD_String_xy(3, 0, modo_operacao ? "Modo: Automatico   " : "Modo: Manual       ");
    
    // Ciclos
    sprintf(lcd_buffer, "Ciclos: %04d", contador_ciclos);
    LCD_String_xy(4, 0, lcd_buffer);
    MSdelay(3000);
}


void Atualizar_Display_Principal(void) {
    static uint16_t display_timer = 0;
    
    if(display_timer++ >= 100) {  // Atualizar a cada 1 segundo
        display_timer = 0;
        
        // Linha 1: Título fixo
        LCD_String_xy(1, 3, "Filtro de Manga    ");
        
        // Linha 2: Data e Hora
        RTC_GetDateTime(&rtc);
        Format_Hora(hora_buffer, &rtc);
        Format_Data(data_buffer, &rtc);
        
        sprintf(lcd_buffer, "%s  %s", hora_buffer, data_buffer);
        LCD_String_xy(2, 0, lcd_buffer);
        
        // Linha 3: Modo de operação e status
        if(falha_pressao) {
            sprintf(lcd_buffer, "FALHA! Press Alta  ");
        } else if(modo_operacao) {
            if(ciclo_em_andamento) {
                sprintf(lcd_buffer, "Auto - Ciclo Ativo ");
            } else {
                sprintf(lcd_buffer, "Auto - Aguardando  ");
            }
        } else {
            sprintf(lcd_buffer, "Manual - V:%d      ", valvula_selecionada + 1);
        }
        LCD_String_xy(3, 0, lcd_buffer);
        
        // Linha 4: Ciclos e pressão
        uint16_t pressao_percent = (adc_pressao * 100) / 1024;
        sprintf(lcd_buffer, "C:%04d P:%3d%%     ", contador_ciclos, pressao_percent);
        LCD_String_xy(4, 0, lcd_buffer);
    }
}

void Beep(uint8_t tipo) {
    switch(tipo) {
        case 1:  // Bipe longo (2 segundos)
            beep = 1;
            MSdelay(2000);
            beep = 0;
            break;
            
        case 2:  // Bipe duplo
            beep = 1; MSdelay(250); beep = 0; MSdelay(100);
            beep = 1; MSdelay(250); beep = 0;
            break;
            
        case 3:  // Bipe curto (confirmação)
            beep = 1;
            MSdelay(100);
            beep = 0;
            break;
    }
}

/*********************Sequenciador Automático*****************************/
void Sequenciador_Automatico(void) {
    static uint8_t valvula_atual = 0;
    static uint16_t estado_timer = 0;
    static uint8_t estado = 0;  // 0=espera, 1=válvula ligada, 2=intervalo, 3=entre ciclos
    
    if(falha_pressao) {
        ciclo_em_andamento = 0;
        led_ativo = 0;
        PORTC = 0x00;
        Val8 = 0;
        return;
    }
    
    switch(estado) {
        case 0:  // Espera para iniciar ciclo
            ciclo_em_andamento = 0;
            led_ativo = 0;
            if(estado_timer++ >= TEMPO_CICLO) {
                estado = 1;
                estado_timer = 0;
                valvula_atual = 0;
                ciclo_em_andamento = 1;
                led_ativo = 1;
                contador_ciclos++;
                Salvar_Contador_EEPROM();
            }
            break;
            
        case 1:  // Válvula ligada
            // Acionar válvula atual
            switch(valvula_atual) {
                case 0: Val1 = 1; break;
                case 1: Val2 = 1; break;
                case 2: Val3 = 1; break;
                case 3: Val4 = 1; break;
                case 4: Val5 = 1; break;
                case 5: Val6 = 1; break;
                case 6: Val7 = 1; break;
                case 7: Val8 = 1; break;
            }
            
            if(estado_timer++ >= TEMPO_VALVULA) {
                // Desligar válvula atual
                PORTC = 0x00;
                Val8 = 0;
                
                estado = 2;
                estado_timer = 0;
                valvula_atual++;
                
                if(valvula_atual >= NUM_VALVULAS) {
                    // Todas válvulas foram acionadas
                    estado = 0;  // Volta para espera entre ciclos
                    estado_timer = 0;
                    ciclo_em_andamento = 0;
                    led_ativo = 0;
                }
            }
            break;
            
        case 2:  // Intervalo entre válvulas
            if(estado_timer++ >= TEMPO_INTERVALO) {
                estado = 1;
                estado_timer = 0;
            }
            break;
            
    }
}

/*********************Modo Manual*****************************/
void Modo_Manual(void) {
    static uint8_t last_inc = 1;
    static uint8_t last_dow = 1;
    static uint8_t last_ent = 1;
    
    // Seleção de válvula com Bt_inc e Bt_dow
    if(Bt_inc == 0 && last_inc == 1) {
        MSdelay(50);  // Debounce
        if(Bt_inc == 0) {
            valvula_selecionada++;
            if(valvula_selecionada >= NUM_VALVULAS) {
                valvula_selecionada = 0;
            }
        }
    }
    last_inc = Bt_inc;
    
    if(Bt_dow == 0 && last_dow == 1) {
        MSdelay(50);
        if(Bt_dow == 0) {
            if(valvula_selecionada == 0) {
                valvula_selecionada = NUM_VALVULAS - 1;
            } else {
                valvula_selecionada--;
            }
            //Beep(3);  // Bipe curto para feedback
        }
    }
    last_dow = Bt_dow;
    
    // Acionamento com Bt_ent
    if(Bt_ent == 0 && last_ent == 1) {
       MSdelay(50);
        if(Bt_ent == 0) {
            // Desliga todas as válvulas primeiro
            PORTC = 0x00;
            Val8 = 0;
            
            // Aciona a válvula selecionada por 2 segundos
            switch(valvula_selecionada) {
                case 0: Val1 = 1; break;
                case 1: Val2 = 1; break;
                case 2: Val3 = 1; break;
                case 3: Val4 = 1; break;
                case 4: Val5 = 1; break;
                case 5: Val6 = 1; break;
                case 6: Val7 = 1; break;
                case 7: Val8 = 1; break;
            }
            
            MSdelay(2000);  // Mantém ligado por 2 segundos
            
            // Desliga a válvula
            PORTC = 0x00;
            Val8 = 0;
            
            //Beep(2);  // Bipe duplo para confirmar acionamento
        }
    }
    last_ent = Bt_ent;
    
    // Mostrar válvula selecionada no display
//    static uint16_t manual_display_timer = 0;
//    if(manual_display_timer++ >= 100) {
//        manual_display_timer = 0;
//        sprintf(lcd_buffer, "Manual V:%d    ", valvula_selecionada + 1);
//        LCD_String_xy(2, 1, lcd_buffer);
//    }
}

/*********************EEPROM 24C32*****************************/
uint8_t Ler_EEPROM(uint16_t endereco) {
    uint8_t dado;
    
    I2C_Start();
    I2C_Write(EEPROM_ADDR_WRITE);
    I2C_Write((endereco >> 8) & 0xFF);  // Endereço alto
    I2C_Write(endereco & 0xFF);         // Endereço baixo
    I2C_Restart();
    I2C_Write(EEPROM_ADDR_READ);
    dado = I2C_Read(0);  // NACK no último byte
    I2C_Stop();
    
    return dado;
}

void Escrever_EEPROM(uint16_t endereco, uint8_t dado) {
    I2C_Start();
    I2C_Write(EEPROM_ADDR_WRITE);
    I2C_Write((endereco >> 8) & 0xFF);  // Endereço alto
    I2C_Write(endereco & 0xFF);         // Endereço baixo
    I2C_Write(dado);
    I2C_Stop();
    
    MSdelay(10);  // Espera escrita completar
}

void Ler_Contador_EEPROM(void) {
    uint8_t high = Ler_EEPROM(EEPROM_CICLO_ADDR);
    uint8_t low = Ler_EEPROM(EEPROM_CICLO_ADDR + 1);
    contador_ciclos = (high << 8) | low;
    
    // Se for a primeira leitura (valores 0xFFFF), inicializa com 0
    if(contador_ciclos == 0xFFFF) {
        contador_ciclos = 0;
    }
}

void Salvar_Contador_EEPROM(void) {
    Escrever_EEPROM(EEPROM_CICLO_ADDR, (contador_ciclos >> 8) & 0xFF);
    Escrever_EEPROM(EEPROM_CICLO_ADDR + 1, contador_ciclos & 0xFF);
}

/*********************RTC DS1307*****************************/
void RTC_Init(void) {
    I2C_Init();
    
    I2C_Start();
    I2C_Write(0xD0);        // Endereço DS1307 em modo escrita
    I2C_Write(0x07);        // Registro de controle
    I2C_Write(0x00);        // Desabilitar saída SQW
    I2C_Stop();
}

void RTC_GetDateTime(rtc_t *rtc) {
    I2C_Start();
    I2C_Write(0xD0);        // Endereço DS1307 em modo escrita
    I2C_Write(0x00);        // Endereço do registro de segundos
    I2C_Restart();
    I2C_Write(0xD1);        // Endereço DS1307 em modo leitura
    
    rtc->sec = I2C_Read(1);
    rtc->min = I2C_Read(1);
    rtc->hour = I2C_Read(1);
    rtc->weekDay = I2C_Read(1);
    rtc->date = I2C_Read(1);
    rtc->month = I2C_Read(1);
    rtc->year = I2C_Read(0);
    
    I2C_Stop();
}

void Format_Hora(char *buffer, rtc_t *rtc) {
    // Converter BCD para decimal e formatar HH:MM:SS
    uint8_t hora = ((rtc->hour >> 4) * 10) + (rtc->hour & 0x0F);
    uint8_t min = ((rtc->min >> 4) * 10) + (rtc->min & 0x0F);
    uint8_t seg = ((rtc->sec >> 4) * 10) + (rtc->sec & 0x0F);
    
    sprintf(buffer, "%02d:%02d:%02d", hora, min, seg);
}

void Format_Data(char *buffer, rtc_t *rtc) {
    // Converter BCD para decimal e formatar DD/MM/AAAA
    uint8_t dia = ((rtc->date >> 4) * 10) + (rtc->date & 0x0F);
    uint8_t mes = ((rtc->month >> 4) * 10) + (rtc->month & 0x0F);
    uint8_t ano = ((rtc->year >> 4) * 10) + (rtc->year & 0x0F) + 2000;
    
    sprintf(buffer, "%02d/%02d/%04d", dia, mes, ano);
}

/*********************ADC para Pressão*****************************/
void ADC_Init(void) {
    ADCON1 = 0x0E;      // AN0 analógico, VREF+ = VDD, VREF- = VSS
    ADCON2 = 0xBE;      // A/D conversão clock = FOSC/64, justificado à direita
}

uint16_t Ler_ADC(uint8_t canal) {
    ADCON0 = (canal << 2) | 0x01;  // Seleciona canal e liga ADC
    MSdelay(1);                    // Tempo de aquisição
    
    ADCON0bits.GO = 1;            // Inicia conversão
    while(ADCON0bits.GO);         // Aguarda conversão
    
    return ((ADRESH << 8) | ADRESL);
}

void Verificar_Falha_Pressao(void) {
    // Se pressão > 80% do fundo de escala, ativa falha
    if(adc_pressao > 819) {  // 80% de 1024
        falha_pressao = 1;
        led_falha = 1;
    } else {
        falha_pressao = 0;
        led_falha = 0;
    }
}
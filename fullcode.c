#include <xc.h>
#include <stdint.h>

// PIC16F877A Configuration
#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = OFF
#pragma config BOREN = OFF
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT  = OFF
#pragma config CP   = OFF

#define _XTAL_FREQ 20000000  // 20 MHz

// DS3231 I2C Address
#define DS3231_ADDR 0xD0

// LCD Pins
#define RS RD0
#define EN RD1
#define D4 RD2
#define D5 RD3
#define D6 RD4
#define D7 RD5

// LED & BUZZER
#define LED RB0
#define BUZZER RB1

// ADC Channel for LM35
#define TEMP_CH 0
#define TEMP_THRESHOLD 50

// EEPROM Address for RTC init flag
#define EEPROM_FLAG_ADDR 0x00

// ===== Function Prototypes =====
void I2C_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_Write(uint8_t data);
uint8_t I2C_Read(uint8_t ack);

uint8_t BCD2DEC(uint8_t bcd);
uint8_t RTC_Read(uint8_t reg);
void RTC_Init(void);
void RTC_Set_Today(void);

void LCD_Init(void);
void LCD_Command(uint8_t cmd);
void LCD_Data(uint8_t data);
void LCD_String(const char* str);

void ADC_Init(void);
uint16_t ADC_Read(uint8_t channel);

void EEPROM_Write(uint8_t addr, uint8_t data);
uint8_t EEPROM_Read(uint8_t addr);

// ===== LCD Low-Level =====
void LCD_Enable(void)
{
    EN = 1;
    __delay_us(50);
    EN = 0;
    __delay_us(50);
}

void LCD_Command(uint8_t cmd)
{
    RS = 0;
    D4 = (cmd>>4) & 1;
    D5 = (cmd>>5) & 1;
    D6 = (cmd>>6) & 1;
    D7 = (cmd>>7) & 1;
    LCD_Enable();

    D4 = cmd & 1;
    D5 = (cmd>>1)&1;
    D6 = (cmd>>2)&1;
    D7 = (cmd>>3)&1;
    LCD_Enable();

    __delay_ms(2);
}

void LCD_Data(uint8_t data)
{
    RS = 1;
    D4 = (data>>4) & 1;
    D5 = (data>>5) & 1;
    D6 = (data>>6) & 1;
    D7 = (data>>7) & 1;
    LCD_Enable();

    D4 = data & 1;
    D5 = (data>>1)&1;
    D6 = (data>>2)&1;
    D7 = (data>>3)&1;
    LCD_Enable();

    __delay_ms(2);
}

void LCD_Init(void)
{
    __delay_ms(20); // LCD power-up
    LCD_Command(0x02); // 4-bit mode
    LCD_Command(0x28); // 2-line, 5x8
    LCD_Command(0x0C); // Display ON, cursor OFF
    LCD_Command(0x06); // Entry mode
    LCD_Command(0x01); // Clear display
    __delay_ms(2);
}

void LCD_String(const char* str)
{
    while(*str) LCD_Data(*str++);
}

// ===== EEPROM =====
void EEPROM_Write(uint8_t addr, uint8_t data)
{
    EEADR = addr; EEDATA = data;
    EECON1bits.EEPGD = 0; EECON1bits.WREN = 1;
    INTCONbits.GIE = 0;
    EECON2 = 0x55; EECON2 = 0xAA;
    EECON1bits.WR = 1;
    while(EECON1bits.WR);
    EECON1bits.WREN = 0; INTCONbits.GIE = 1;
}

uint8_t EEPROM_Read(uint8_t addr)
{
    EEADR = addr; EECON1bits.EEPGD = 0; EECON1bits.RD = 1;
    return EEDATA;
}

// ===== ADC =====
void ADC_Init(void){ ADCON1 = 0x80; ADCON0 = 0x01; }

uint16_t ADC_Read(uint8_t ch)
{
    ADCON0 &= 0xC5;
    ADCON0 |= ch << 3;
    __delay_ms(2);
    GO_nDONE = 1; while(GO_nDONE);
    return ((ADRESH<<8)+ADRESL);
}

// ===== I2C =====
void I2C_Init(void){ SSPCON = 0x28; SSPADD = ((_XTAL_FREQ/40000)-1); SSPSTAT = 0x00; }
void I2C_Start(void){ SEN=1; while(SEN); }
void I2C_Stop(void){ PEN=1; while(PEN); }
void I2C_Write(uint8_t data){ SSPBUF = data; while(BF); while(SSPIF==0); SSPIF=0; }
uint8_t I2C_Read(uint8_t ack){ RCEN=1; while(!BF); uint8_t t=SSPBUF; ACKDT=(ack)?0:1; ACKEN=1; while(ACKEN); return t; }

// ===== RTC =====
uint8_t BCD2DEC(uint8_t bcd){ return ((bcd/16*10)+(bcd%16)); }

uint8_t RTC_Read(uint8_t reg)
{
    I2C_Start();
    I2C_Write(DS3231_ADDR);
    I2C_Write(reg);
    I2C_Start();
    I2C_Write(DS3231_ADDR|1);
    uint8_t data = I2C_Read(0);
    I2C_Stop();
    return data;
}

void RTC_Init(void)
{
    I2C_Start();
    I2C_Write(DS3231_ADDR);
    I2C_Write(0x00);
    I2C_Write(0x00); // CH=0
    I2C_Stop();
}

void RTC_Set_Today(void)
{
    uint8_t flag = EEPROM_Read(EEPROM_FLAG_ADDR);
    if(flag==1) return; // Already set

    // Example: 02/01/2026 21:30:00
    I2C_Start();
    I2C_Write(DS3231_ADDR);
    I2C_Write(0x00);
    I2C_Write(0x00); // Seconds
    I2C_Write(0x30); // Minutes
    I2C_Write(0x21); // Hours (21 = 9 PM)
    I2C_Write(0x05); // Day of week
    I2C_Write(0x02); // Date
    I2C_Write(0x01); // Month
    I2C_Write(0x26); // Year 26 (2026)
    I2C_Stop();

    EEPROM_Write(EEPROM_FLAG_ADDR,1); // Set flag
}

// ===== MAIN LOOP =====
void main(void)
{
    TRISD = 0x00; TRISB0 = 0; TRISB1 = 0; TRISC3=1; TRISC4=1;
    LED=0; BUZZER=0;

    I2C_Init(); RTC_Init(); LCD_Init(); ADC_Init();
    RTC_Set_Today();

    while(1)
    {
        uint8_t sec = BCD2DEC(RTC_Read(0x00)&0x7F);
        uint8_t min = BCD2DEC(RTC_Read(0x01));
        uint8_t hr  = BCD2DEC(RTC_Read(0x02)&0x3F);
        uint8_t date= BCD2DEC(RTC_Read(0x04));
        uint8_t mon = BCD2DEC(RTC_Read(0x05));
        uint8_t yr  = BCD2DEC(RTC_Read(0x06));

        uint16_t temp_adc = ADC_Read(TEMP_CH);
        uint16_t temp_c   = (temp_adc*500UL)/1023;

        if(hr>=18) LED=1; else LED=0;
        if(temp_c>TEMP_THRESHOLD) BUZZER=1; else BUZZER=0;

        // LINE 1: TIME + DATE
        LCD_Command(0x80);
        LCD_Data('0'+hr/10); LCD_Data('0'+hr%10); LCD_Data(':');
        LCD_Data('0'+min/10); LCD_Data('0'+min%10); LCD_Data(':');
        LCD_Data('0'+sec/10); LCD_Data('0'+sec%10); LCD_Data(' ');
        LCD_Data('0'+date/10); LCD_Data('0'+date%10); LCD_Data('/');
        LCD_Data('0'+mon/10); LCD_Data('0'+mon%10); LCD_Data('/');
        LCD_Data('0'+yr/10); LCD_Data('0'+yr%10);

        // LINE 2: ALERT / TEMP
        LCD_Command(0xC0);
        if(temp_c>TEMP_THRESHOLD) LCD_String("Temp High!     ");
        else if(hr>=18) LCD_String("Evening Alert  ");
        else
        {
            LCD_Data('0'+temp_c/10);
            LCD_Data('0'+temp_c%10);
            LCD_Data('C');
            LCD_Data(' '); LCD_Data(' ');
        }

        __delay_ms(1000);
    }
}

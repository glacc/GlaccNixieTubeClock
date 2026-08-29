/*
    Copyright (C) 2026 Glacc

    This file is part of GlaccNixieTubeClockFirmware.

    GlaccNixieTubeClockFirmware is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    GlaccNixieTubeClockFirmware is distributed in the hope that it
    will be useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with GlaccNixieTubeClockFirmware. If not,
    see <https://www.gnu.org/licenses/>. 
*/

// STC15W204
// 12MHz

#include <stc12.h>

// pin setup --------------------------

// 74595&colon pin
#define NDAT    P1_0
#define NCLK    P1_1
#define RCLK    P1_2

#define COLON   P1_4

// DS1302 pins
SBIT(P5_4, 0xC8, 4);
SBIT(P5_5, 0xC8, 5);    // these pins are undefined in <stc12.h>

#define SDAT    P5_4
#define SCLK    P5_5
#define CE      P1_3

// switch pins
#define SW1     P3_2
#define SW2     P3_3
#define SW3     P3_6
#define SW4     P3_7

// settings ---------------------------

const short key_repeat_delay = 500;
const unsigned char key_repeat_interval = 175;

const unsigned char update_interval = 100;

const char auto_cycling_interval_index = 0;
__code const unsigned char shuffle_cycling_step_interval[] = { 200, 8 };

__code const char auto_cycling_mins[] = { 0x00, 0x15, 0x30, 0x45 };
const char auto_cycling_secs[] = { 5, 15, 25, 35, 45, 55 };

const unsigned char divider = 1;

// custom settings address in DS1302 --

const unsigned char addr1302_brightness = 0xC0;

const unsigned char addr1302_seed0_0 = 0xC2;
const unsigned char addr1302_seed0_1 = 0xC4;
const unsigned char addr1302_seed0_2 = 0xC6;
const unsigned char addr1302_seed0_3 = 0xC8;
const unsigned char addr1302_seed0_checksum = 0xCA;

const unsigned char addr1302_seed1_0 = 0xCC;
const unsigned char addr1302_seed1_1 = 0xCE;
const unsigned char addr1302_seed1_2 = 0xD0;
const unsigned char addr1302_seed1_3 = 0xD2;
const unsigned char addr1302_seed1_checksum = 0xD4;

const unsigned char addr1302_curr_seed = 0xD6;

// variables --------------------------

// time
unsigned char sec = 0;
unsigned char min = 0;
unsigned char hour = 0;
unsigned char sec_old = 0;

// key press
char key_pressed = 0;
char key_pressed_old = 0;
short key_repeat_delay_tick = 0;
short key_repeat_tick = 0;

signed char key_repeat_count = -1;

// update interval
unsigned char update_interval_tick = 0;
volatile char update_request = 0;

// shuffle/cycling
char auto_cycling = 0;
char shuffle_enabled = 0;
unsigned char shuffle_cycling_tick = 0;
char shuffle_cycling_count = 0;

volatile char randomize_req = 0;
char digits_randomize[4][10];
unsigned long xorshift_seed = 1234567890;
unsigned char curr_seed = 0;

char no_shuffle_old = 1;

// display
unsigned char digits_pending[4] = { 0, 0, 0, 0 };
unsigned char digits[4] = { 0, 0, 0, 0 };
volatile char digits_prepared = 0;

unsigned char brightness = 4;

unsigned char divider_tick = 0;

char tube_counter = 0;
char colon = 0;
__code const unsigned char tube_selector[8] =
{
    0x20, 0x40, 0x80, 0x10,
    0x00, 0x00, 0x00, 0x00
};

// utilties ---------------------------

#ifdef NOP_STRETCH

inline void _nop(void)
{
    __asm nop __endasm;
}

#else

#define _nop()

#endif

// shuffle ----------------------------

unsigned long XORShift32Next(void)
{
    xorshift_seed ^= xorshift_seed << 13;
    xorshift_seed ^= xorshift_seed >>  7;
    xorshift_seed ^= xorshift_seed << 17;
    return xorshift_seed;
}

void ResetShuffleDigits(void)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 10; j++)
            digits_randomize[i][j] = j;
    }
}

// 74595 ------------------------------

void Write595(unsigned char _data)
{
    char i = 0;
    while (i < 8)
    {
        NDAT = ((_data & 0x80) != 0) ? 1 : 0;
        _data <<= 1;

        NCLK = 1;
        _nop();
        NCLK = 0;
        _nop();

        i ++ ;
    }

    RCLK = 1;

    _nop();

    RCLK = 0;
}

// DS1302 -----------------------------

char Read1302(char addr)
{
    SCLK = 0;
    _nop();
    _nop();
    _nop();
    CE = 1;
    
    char i = 0;
    while (i < 8)
    {
        SDAT = addr & 1;
        _nop();
        _nop();
        _nop();
        SCLK = 1;
        _nop();
        _nop();
        _nop();
        SCLK = 0;
        addr >>= 1;
        i ++ ;
    }
    
    SDAT = 1;
    char data = 0;
    _nop();
    _nop();
    _nop();
    
    i = 0;
    while (i < 8)
    {
        data >>= 1;
        
        if (SDAT == 1) data |= 0x80;
        
        SCLK = 1;
        SDAT = 1;
        _nop();
        _nop();
        _nop();
        SCLK = 0;
        _nop();
        _nop();
        _nop();
        
        i ++ ;
    }
    
    CE = 0;
    _nop();
    _nop();
    _nop();
    SCLK = 1;
    _nop();
    _nop();
    _nop();
    SDAT = 0;
    _nop();
    _nop();
    _nop();
    SDAT = 1;
    _nop();
    _nop();
    _nop();
    
    return data;
}

void Write1302(char addr, char data)
{
    SCLK = 0;
    _nop();
    _nop();
    _nop();
    CE = 1;
    
    char i = 0;
    while (i < 8)
    {
        SDAT = addr & 1;
        _nop();
        _nop();
        _nop();
        SCLK = 1;
        _nop();
        _nop();
        _nop();
        SCLK = 0;
        addr >>= 1;
        i ++ ;
    }
    
    _nop();
    
    i = 0;
    while (i < 8)
    {
        SDAT = data & 1;
        _nop();
        _nop();
        _nop();
        SCLK = 1;
        _nop();
        _nop();
        _nop();
        SCLK = 0;
        data >>= 1;
        i ++ ;
    }
    
    CE = 0;
}

void SetWriteProtection1302(char enabled)
{
    Write1302(0x8E, enabled ? 0x80 : 0x00);
}

void Init1302(void)
{
    SetWriteProtection1302(0);  //Write protection off

    if (Read1302(0x81) & 0x80)
    {
        Write1302(0x80, 0x00);  //Sec = 0;  CH = 0
        Write1302(0x82, 0x00);  //Min = 0;
        Write1302(0x84, 0x00);  //Hour = 0; 24 hour mode
    }

    SetWriteProtection1302(1);
}

void WriteWhileWriteProtectionOn1302(char addr, char data)
{
    SetWriteProtection1302(0);
    Write1302(addr, data);
    SetWriteProtection1302(1);
}

// debugging using UART ---------------

#ifdef DEBUG

char busy = 0;

SFR(T2H, 0xD6);
SFR(T2L, 0xD7);

void InitUART(void)
{
    //9600bps@12.000MHz
    SCON = 0x50;    //8位数据,可变波特率
    AUXR |= 0x01;   //串口1选择定时器2为波特率发生器
    AUXR |= 0x04;   //定时器时钟1T模式
    T2L = 0xC7;     //设置定时初始值
    T2H = 0xFE;     //设置定时初始值
    AUXR |= 0x10;   //定时器2开始计时
    ES = 1;
}

void UARTInterrupt(void) __interrupt (4) __using (1)
{
    if (RI)
        RI = 0;

    if (TI) 
    {
        TI = 0;
        busy = 0;
    }
}

void SendChar(char _char)
{
    while (busy);
    busy = 1;
    SBUF = _char;
}

#endif

// initialization ---------------------

void InitPins(void)
{
    NDAT = NCLK = RCLK = SDAT = SCLK = CE = 0;
}

void InitTMR0(void)
{
    // 12.000MHz, 1ms
    
    AUXR &= 0x7F;   // 12T mode
    TMOD &= 0xF0;

    TL0 = 0x18;
    TH0 = 0xFC;     // set counter

    TF0 = 0;        // clear TF0
    TR0 = 1;        // start

    ET0 = 1;        // enable TMR0 interrupt
}

// TMR0 interrupt routine -------------

void TMR0Interrupt(void) __interrupt (1) __using (1)
{
    divider_tick++;

    if (divider_tick >= divider)
    {
        Write595(tube_selector[tube_counter] | ((tube_counter < 4) ? digits[tube_counter] : digits[0]));
        COLON = ((tube_counter < 2) && (colon != 0));

        if (digits_prepared)
        {
            digits[0] = digits_pending[0];
            digits[1] = digits_pending[1];
            digits[2] = digits_pending[2];
            digits[3] = digits_pending[3];
            digits_prepared = 0;
        }

        tube_counter ++ ;
        if (tube_counter >= 8 - brightness)
            tube_counter = 0;

        divider_tick = 0;
    }

    // key repeat
    if (key_pressed)
    {
        if (key_repeat_delay_tick < key_repeat_delay)
            key_repeat_delay_tick++;
        else
            key_repeat_tick++;
    }

    // update
    update_interval_tick ++ ;
    if (update_interval_tick >= update_interval)
    {
        update_interval_tick = 0;

        update_request = 1;
    }

    // shuffle
    if (shuffle_enabled || auto_cycling)
    {
        shuffle_cycling_tick++;

        char interval_index = shuffle_enabled ? (shuffle_enabled - 1) : auto_cycling_interval_index;

        if (shuffle_cycling_tick >= shuffle_cycling_step_interval[interval_index])
        {
            shuffle_cycling_count++;
            if (shuffle_cycling_count >= 10)
            {
                if (shuffle_enabled)
                    randomize_req = 1;

                shuffle_cycling_count = 0;
            }

            update_request = 1;

            shuffle_cycling_tick = 0;
        }
    }
    else
    {
        shuffle_cycling_tick = 0;
        shuffle_cycling_count = 0;
    }

    TF0 = 0;    // clear TF0 flag
}

// custom settings --------------------

void SaveRandomSeed(void)
{
    unsigned char seed0 = xorshift_seed & 0xFF;
    unsigned char seed1 = (xorshift_seed >>  8) & 0xFF;
    unsigned char seed2 = (xorshift_seed >> 16) & 0xFF;
    unsigned char seed3 = (xorshift_seed >> 24) & 0xFF;

    unsigned char checksum = seed0 + seed1 + seed2 + seed3;

    SetWriteProtection1302(0);

    if (!curr_seed)
    {
        Write1302(addr1302_seed0_0, seed0);
        Write1302(addr1302_seed0_1, seed1);
        Write1302(addr1302_seed0_2, seed2);
        Write1302(addr1302_seed0_3, seed3);
        Write1302(addr1302_seed0_checksum, checksum);
    }
    else
    {
        Write1302(addr1302_seed1_0, seed0);
        Write1302(addr1302_seed1_1, seed1);
        Write1302(addr1302_seed1_2, seed2);
        Write1302(addr1302_seed1_3, seed3);
        Write1302(addr1302_seed1_checksum, checksum);
    }

    Write1302(addr1302_curr_seed, curr_seed);

    SetWriteProtection1302(1);
}

void ReadAndCheckCustomSettings(void)
{
    // brightness
    unsigned char brightness_1302 = Read1302(addr1302_brightness + 1);
    #ifdef DEBUG
    SendChar(brightness_1302);
    #endif

    if (brightness_1302 <= 4)
        brightness = brightness_1302;
    else
        WriteWhileWriteProtectionOn1302(addr1302_brightness, brightness);

    // seed
    curr_seed = Read1302(addr1302_curr_seed + 1);

    char has_valid_seed = 2;

    if (curr_seed > 1)
        has_valid_seed = 0;

    while (has_valid_seed)
    {
        unsigned char seed0_1302, seed1_1302, seed2_1302, seed3_1302;
        unsigned char checksum_1302;

        if (!curr_seed)
        {
            seed0_1302 = Read1302(addr1302_seed0_0 + 1);
            seed1_1302 = Read1302(addr1302_seed0_1 + 1);
            seed2_1302 = Read1302(addr1302_seed0_2 + 1);
            seed3_1302 = Read1302(addr1302_seed0_3 + 1);

            checksum_1302 = Read1302(addr1302_seed0_checksum + 1);
        }
        else
        {
            seed0_1302 = Read1302(addr1302_seed1_0 + 1);
            seed1_1302 = Read1302(addr1302_seed1_1 + 1);
            seed2_1302 = Read1302(addr1302_seed1_2 + 1);
            seed3_1302 = Read1302(addr1302_seed1_3 + 1);

            checksum_1302 = Read1302(addr1302_seed1_checksum + 1);
        }

        #ifdef DEBUG
        SendChar(seed0_1302);
        SendChar(seed1_1302);
        SendChar(seed2_1302);
        SendChar(seed3_1302);
        SendChar(checksum_1302);
        #endif

        unsigned char checksum = seed0_1302 + seed1_1302 + seed2_1302 + seed3_1302;

        if (checksum_1302 == checksum)
        {
            xorshift_seed = (seed0_1302 | (((unsigned long)seed1_1302) << 8) | (((unsigned long)seed2_1302) << 16) | (((unsigned long)seed3_1302) << 24));
            
            #ifdef DEBUG
            SendChar(0xDD);
            #endif

            break;
        }
        #ifdef DEBUG
        else
            SendChar(0xEE);
        #endif

        curr_seed = curr_seed ? 0 : 1;

        has_valid_seed--;
    }

    if (!has_valid_seed)
    {
        curr_seed = 1;
        SaveRandomSeed();
        curr_seed = 0;
        SaveRandomSeed();

        #ifdef DEBUG
        SendChar(0xFF);
        #endif

        return;
    }
}

// events when switch pressed ---------

void OnSW1Press(void)
{
    // hour++
    hour ++ ;
    if ((hour & 0x0F) >= 10)
        hour = (hour & 0x30) + 0x10;
    if (((hour & 0x30) == 0x20) && ((hour & 0x0F) == 4))
        hour = 0;

    WriteWhileWriteProtectionOn1302(0x84, hour & 0x3F);
}

void OnSW2Press(void)
{
    // minute++
    min ++ ;
    if ((min & 0x0F) >= 10)
        min = (min & 0x70) + 0x10;
    if ((min & 0x70) >= 0x60)
        min = 0;

    WriteWhileWriteProtectionOn1302(0x82, min & 0x7F);
}

void OnSW3Press(void)
{
    // reset second
    WriteWhileWriteProtectionOn1302(0x80, 0x00);

    colon = 0xFF;
}

void OnSW4Release(void)
{
    // brightness adjust
    brightness ++ ;
    if (brightness > 4)
        brightness = 0;

    WriteWhileWriteProtectionOn1302(addr1302_brightness, brightness);
}

void OnSW4Repeat(void)
{
    // manual shuffle enable
    if (shuffle_enabled == 0)
        shuffle_cycling_count = 0;
    
    shuffle_enabled++;
    if (shuffle_enabled > sizeof(shuffle_cycling_step_interval))
        shuffle_enabled = 0;
}

// main -------------------------------

void main(void)
{
    InitPins();

    Init1302();

    InitTMR0();

#ifdef DEBUG
    InitUART();
#endif

    EA = 1;

    ResetShuffleDigits();

    ReadAndCheckCustomSettings();

    while (1)
    {
        // key detection
        SW1 = SW2 = SW3 = SW4 = 1;

        char sw1_down = !SW1;
        char sw2_down = !SW2;
        char sw3_down = !SW3;
        char sw4_down = !SW4;

        char key_down_count = 0;

        if (sw1_down)
            key_down_count++;
        if (sw2_down)
            key_down_count++;
        if (sw3_down)
            key_down_count++;
        if (sw4_down)
            key_down_count++;

        if (key_down_count == 1)
        {
            if (!key_pressed)
            {
                if (sw1_down)
                    key_pressed = 1;
                if (sw2_down)
                    key_pressed = 2;
                if (sw3_down)
                    key_pressed = 3;
                if (sw4_down)
                    key_pressed = 4;
            }
        }
        else if (!key_down_count)
            key_pressed = 0;

        if (key_pressed)
        {
            char repeat_key = (key_repeat_tick >= key_repeat_interval);

            if (repeat_key)
            {
                sec  = Read1302(0x81) & 0x7F;
                min  = Read1302(0x83) & 0x7F;
                hour = Read1302(0x85) & 0x3F;

                switch (key_pressed)
                {
                    case 1:
                        OnSW1Press();
                        break;
                    case 2:
                        OnSW2Press();
                        break;
                    case 3:
                        OnSW3Press();
                        break;
                    case 4:
                        if (key_repeat_count == 1)
                            OnSW4Repeat();
                        break;
                    default:
                        break;
                }

                #ifdef DEBUG
                SendChar(hour);
                SendChar(min);
                SendChar(sec);
                SendChar(0xAA);
                SendChar(sw1_down);
                SendChar(sw2_down);
                SendChar(sw3_down);
                SendChar(sw4_down);
                SendChar(key_pressed);
                SendChar(0xBB);
                #endif

                key_repeat_tick %= key_repeat_interval;

                if (key_repeat_count < 10)
                    key_repeat_count++;
            }
        }
        else
        {
            switch (key_pressed_old)
            {
                case 4:
                    if (key_repeat_count == 0)
                        OnSW4Release();
                    break;
                default:
                    break;
            }

            key_repeat_count = -1;
            key_repeat_delay_tick = 0;
            key_repeat_tick = key_repeat_interval;
        }

        key_pressed_old = key_pressed;

        // randomized shuffle
        if (randomize_req)
        {
            for (int i = 0; i < 4; i++)
            {
                char last = digits_randomize[i][9];

                // random swap
                char a = ((char)XORShift32Next()) % 10;
                char b = ((char)XORShift32Next()) % 10;
                if (b == a)
                {
                    b++;
                    if (b >= 10)
                        b = 0;
                }
                char temp = digits_randomize[i][a];
                digits_randomize[i][a] = digits_randomize[i][b];
                digits_randomize[i][b] = temp;

                // break neighbour number
                if (digits_randomize[i][0] == last)
                {
                    char temp = digits_randomize[i][0];
                    digits_randomize[i][0] = digits_randomize[i][9];
                    digits_randomize[i][9] = temp;
                }
            }

            randomize_req = 0;
        }

        // update
        if (update_request)
        {
            sec = Read1302(0x81) & 0x7F;
            min = Read1302(0x83) & 0x7F;

            char colon_update = 1;

            if (shuffle_enabled)
            {
                if (shuffle_cycling_step_interval[shuffle_enabled - 1] < 50)
                {
                    colon_update = 0;

                    colon = 0x00;
                }
            }

            if (colon_update)
            {
                if (sec_old != sec)
                    colon = ~colon;
            }

            sec_old = sec;

            // auto cycling check
            char in_auto_cycling_min = 0;
            for (int i = 0; i < sizeof(auto_cycling_mins); i++)
            {
                if (min == auto_cycling_mins[i])
                {
                    in_auto_cycling_min = 1;
                    break;
                }
            }

            auto_cycling = 0;
            if (in_auto_cycling_min)
            {
                char sec_binary = ((sec >> 4) * 10) + (sec & 0xF);
                
                for (int i = 0; i < sizeof(auto_cycling_secs); i++)
                {
                    char sec_to_compare = auto_cycling_secs[i];

                    if (sec_binary >= sec_to_compare)
                        auto_cycling = auto_cycling ? 0 : 1;

                    if (sec_binary < sec_to_compare)
                        break;
                }
            }

            char no_shuffle = !shuffle_enabled && !auto_cycling;

            if (!no_shuffle_old && no_shuffle)
                ResetShuffleDigits();
            
            if (!digits_prepared)
            {
                if (no_shuffle)
                {
                    hour = Read1302(0x85) & 0x3F;

                    digits_pending[0] = (hour >> 4) & 0x03;
                    digits_pending[1] = hour & 0x0F;
                    digits_pending[2] = (min >> 4) & 0x07;
                    digits_pending[3] = min & 0x0F;
                }
                else
                {
                    if (shuffle_enabled)
                    {
                        digits_pending[0] = digits_randomize[0][shuffle_cycling_count];
                        digits_pending[1] = digits_randomize[1][shuffle_cycling_count];
                        digits_pending[2] = digits_randomize[2][shuffle_cycling_count];
                        digits_pending[3] = digits_randomize[3][shuffle_cycling_count];
                    }
                    else
                    {
                        digits_pending[0] =
                        digits_pending[1] =
                        digits_pending[2] =
                        digits_pending[3] = shuffle_cycling_count;
                    }
                }
            }
            digits_prepared = 1;

            no_shuffle_old = no_shuffle;

            SaveRandomSeed();

            update_request = 0;
        }

        XORShift32Next();

        PCON |= 0x01;
    }
}

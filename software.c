#include <stc12.h>

// -------------------------

//#define DEBUG

// -------------------------

#define	NDAT		P1_0
#define	NCLK		P1_1
#define	RCLK		P1_2

#define	COLON		P1_4

// -------------------------

SBIT(P5_4, 0xC8, 4);
SBIT(P5_5, 0xC8, 5);

#define	SDAT		P5_4
#define	SCLK		P5_5
#define	CE			P1_3

// -------------------------

#define	SW1			P3_2
#define	SW2			P3_3
#define	SW3			P3_6
#define	SW4			P3_7

// -------------------------

inline void _nop()
{
	__asm nop __endasm;
}

// -------------------------

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

// -------------------------

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
	char _data = 0;
	_nop();
	_nop();
	_nop();
	
	i = 0;
	while (i < 8)
	{
		_data >>= 1;
		
		if (SDAT == 1) _data |= 0x80;
		
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
	
	return _data;
}

void Write1302(char addr, char _data)
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
		SDAT = _data & 1;
		_nop();
		_nop();
		_nop();
		SCLK = 1;
		_nop();
		_nop();
		_nop();
		SCLK = 0;
		_data >>= 1;
		i ++ ;
	}
	
	CE = 0;
}

void Init1302()
{
	Write1302(0x8E, 0x00);		//Write protection off

	char reg = Read1302(0x81);	//CH = 1 ?

	if (reg & 0x80)
	{
		Write1302(0x80, 0x00);		//Sec = 0;  CH = 0
		Write1302(0x82, 0x00);		//Min = 0;
		Write1302(0x84, 0x00);		//Hour = 0; 24 hour mode
	}

	Write1302(0x8E, 0x80);
}

// -------------------------

#ifdef DEBUG

char busy = 0;

SFR(T2H, 0xD6);
SFR(T2L, 0xD7);

void UartInit()			//9600bps@24.000MHz
{
	SCON = 0x50;		//8位数据,可变波特率
	AUXR |= 0x01;		//串口1选择定时器2为波特率发生器
	AUXR |= 0x04;		//定时器时钟1T模式
	T2L = 0xC7;			//设置定时初始值
	T2H = 0xFE;			//设置定时初始值
	AUXR |= 0x10;		//定时器2开始计时
	ES = 1;
}

void UartInterrupt() __interrupt (4) __using (1)
{
	/*
    if (RI)
	{
		RI = 0;
	}
	*/
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

// -------------------------

void InitPins()
{
	NDAT = NCLK = RCLK = SDAT = SCLK = CE = 0;
}

// -------------------------

int tick = 0;

char keyPrs = 0;

char updReq = 0;
unsigned char sec = 0;
unsigned char min = 0;
unsigned char hour = 0;
unsigned char secOld = 0;
char digit[4] = {0, 0, 0, 0};

char brightness = 4;

char tubeCounter = 0;
char colon = 0;
__code char TubeSel[8] = {0x20, 0x40, 0x80, 0x10};

// -------------------------

void Tmr0Init()			//@24.000MHz
{
	AUXR &= 0x7F;
	TMOD &= 0xF0;
	TL0 = 0x18;			//设置定时初始值
	TH0 = 0xFC;			//设置定时初始值
	TF0 = 0;
	TR0 = 1;
	ET0 = 1;
}

void Tmr0Interrupt() __interrupt (1) __using (1)
{
	Write595(TubeSel[tubeCounter] | ((tubeCounter < 4) ? digit[tubeCounter] : digit[0]));
	COLON = ((tubeCounter < 2) && (colon != 0));

	tubeCounter ++ ;
	if (tubeCounter >= 8 - brightness)
		tubeCounter = 0;

	tick ++ ;
	if (tick >= 100)
	{
		updReq = 1;
		tick = 0;
	}

	TF0 = 0;
}

// -------------------------

void main()
{
	InitPins();

	Tmr0Init();
#ifdef DEBUG
	UartInit();
#endif
	EA = 1;

	Init1302();

	while (1)
	{
		SW1 = SW2 = SW3 = SW4 = 1;
		if (!SW1 || !SW2 || !SW3 || !SW4)
		{
			if (!keyPrs)
			{
			#ifdef DEBUG
				char pressedKey = 0;
			#endif

				sec = Read1302(0x81) & 0x7F;
				min = Read1302(0x83) & 0x7F;
				hour = Read1302(0x85) & 0x3F;

				if (!SW4)		//Brightness
				{
					brightness ++ ;
					if (brightness > 4)
						brightness = 0;

				#ifdef DEBUG
					pressedKey = 4;
				#endif
				}
				else if (!SW3)	//Second reset
				{
					Write1302(0x8E, 0x00);
					Write1302(0x80, 0x00);
					Write1302(0x8E, 0x80);

					colon = 0xFF;

				#ifdef DEBUG
					pressedKey = 3;
				#endif
				}
				else if (!SW2)	//Minute
				{
					min ++ ;
					if ((min & 0x0F) >= 10)
						min = (min & 0x70) + 0x10;
					if ((min & 0x70) >= 0x60)
						min = 0;

					Write1302(0x8E, 0x00);
					Write1302(0x82, min & 0x7F);
					Write1302(0x8E, 0x80);

				#ifdef DEBUG
					pressedKey = 2;
				#endif
				}
				else if (!SW1)	//Hour
				{
					hour ++ ;
					if ((hour & 0x0F) >= 10)
						hour = (hour & 0x30) + 0x10;
					if (((hour & 0x30) == 0x20) && ((hour & 0x0F) == 4))
						hour = 0;

					Write1302(0x8E, 0x00);
					Write1302(0x84, hour & 0x3F);
					Write1302(0x8E, 0x80);

				#ifdef DEBUG
					pressedKey = 1;
				#endif
				}

			#ifdef DEBUG
				SendChar(hour);
				SendChar(min);
				SendChar(sec);
				SendChar(0xAA);
				SendChar(SW1 == 0);
				SendChar(SW2 == 0);
				SendChar(SW3 == 0);
				SendChar(SW4 == 0);
				SendChar(0xBB);
			#endif

			}
			keyPrs = 1;
		}
		else
			keyPrs = 0;

		if (updReq)
		{
			sec = Read1302(0x81) & 0x7F;
			min = Read1302(0x83) & 0x7F;
			hour = Read1302(0x85) & 0x3F;

			if (secOld != sec)
				colon = ~colon;

			secOld = sec;

			// -------------------------

			digit[0] = (hour >> 4) & 0x03;
			digit[1] = hour & 0x0F;
			digit[2] = (min >> 4) & 0x07;
			digit[3] = min & 0x0F;

			// -------------------------

			updReq = 0;
		}
	};
}
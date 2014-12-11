FX2LIBDIR=lib/fx2lib/
BASENAME = parallel
SOURCES=parallel.c wave_6800.c
A51_SOURCES=dscr.a51
PID=0x1004
SDCCFLAGS=--std-sdcc99
CODE_SIZE?=--code-size 0x3E00
XRAM_LOC?=--xram-loc 0xE000

include $(FX2LIBDIR)lib/fx2.mk

waveformat_and_all: format_waves all

format_waves:
	sed -i.bak s/\ xdata/\ __xdata/g wave_6800.c
	sed -i.bak s/fx2\.h/fx2macros\.h/g wave_6800.c
	sed -i.bak s/fx2sdly\.h/delay\.h/g wave_6800.c
	sed -i.bak s/SYNCDELAY\ /SYNCDELAY4\ /g wave_6800.c
	sed -i.bak s/SYNCDELAY\;/SYNCDELAY4\;/g wave_6800.c

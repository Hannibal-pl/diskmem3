/*
Copyright (c) 2013, Konrad Rzepecki <hannibal@astral.lodz.pl>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OFMERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define VERSION			"0.2.1"
#define DEFAULT_BAUDRATE	38400
#define DEFAULT_DESTMEM		0x131C
#define DEFAULT_DEVICE		"/dev/ttyUSB0"
#define DEFAULT_JUMPPOINT	0x60B68E4
/*#define DEFAULT_JUMPPOINT	0x60B70C4*/
#define MAX_COMMAND_LEN		512

extern int baudrate;
extern bool consoleoutput;
extern bool datawarning;
extern unsigned destmem;
extern char *device;
extern char *exploit;
extern unsigned exlen;
extern unsigned jumppoint;
extern FILE *logfile;
extern int sfd;

extern void closeserial(void);
extern bool initserial(int baudrate);
extern bool isecho(char *str);
extern int openserial(void);
extern void parseparams(int argc, char *argv[]);
extern int selectbaud(int baudrate);
extern bool sendbyte(char byte);
extern bool sendcommand(char *format, ...);
extern bool setspeed(int baudrate, int *realset);
extern void uflood(unsigned sec);

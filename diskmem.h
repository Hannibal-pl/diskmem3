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
#include <stdbool.h>

#define VERSION			"0.2.1"
#define DEFAULT_DESTMEM		0x131C
#define DEFAULT_DEVICE		"/dev/ttyUSB0"
#define DEFAULT_JUMPPOINT	0x60B68E4
/*#define DEFAULT_JUMPPOINT	0x60B70C4*/

extern int baudrate;
extern bool consoleoutput;
extern bool datawarning;
extern unsigned destmem;
extern char *device;
extern char *exploit;
extern unsigned exlen;
extern unsigned jumppoint;
extern FILE *logfile;

extern void parseparams(int argc, char *argv[]);

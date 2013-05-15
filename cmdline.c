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

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "diskmem.h"

const struct option longopt[] = {
	{"console", 1, NULL, 'c'},
	{"baudrate", 1, NULL, 'd'},
	{"device", 1, NULL, 'd'},
	{"exploit", 1, NULL, 'e'},
	{"help", 0, NULL, 'h'},
	{"logfile", 1, NULL, 'l'},
	{"destmem", 1, NULL, 'm'},
	{"jumppoint", 1, NULL, 'p'},
	{"version", 0, NULL, 'v'},
	{NULL, 0, NULL, 0}};

void parseparams(int argc, char *argv[]) {
	int opt;
	int err;
	FILE *exfile;

	while (true) {
		opt = getopt_long(argc, argv, "b:c:d:e:hl:m:p:vw:?", longopt, NULL);
		if (opt == -1) {
			break;
		}

		switch (opt) {
			case 'b':
				if (optarg == NULL) {
					printf("You must specify baudrate.\n");
					exit(EINVAL);
				}
				baudrate = atoi(optarg);
				if (baudrate <= 0) {
					printf("Baudrate must be grater than 0.\n");
					exit(EINVAL);
				}
				break;
			case 'c':
				if (optarg != NULL) {
					if ((!strcmp(optarg, "ON")) || (!strcmp(optarg, "1"))) {
						consoleoutput = true;
					} else if ((!strcmp(optarg, "OFF")) || (!strcmp(optarg, "0"))) {
						consoleoutput = false;
					} else {
						printf("Initial console forwarding state must be \"ON\" or \"OFF\".\n");
						exit(EINVAL);
					}
				}
				break;
			case 'd':
				if (optarg == NULL) {
					printf("You must specify serial device.\n");
					exit(EINVAL);
				}
				device = strdup(optarg);
				break;
			case 'e':
				if (optarg == NULL) {
					printf("You must specify exploit filename.\n");
					exit(EINVAL);
				}
				exfile = fopen(optarg, "r");
				if (!exfile) {
					err = errno;
					printf("Cannot open exploit file \"%s\".\n", optarg);
					exit(err);
				}

				fseek(exfile, 0, SEEK_END);
				exlen = ftell(exfile);
				fseek(exfile, 0, SEEK_SET);

				exploit = (char *)malloc(exlen + 4);
				if (!exploit) {
					err = errno;
					printf("Cannot allocate memory for exploit.\n");
					fclose(exfile);
					exit(err);
				}

				fread(exploit, 1, exlen, exfile);
				fclose(exfile);
				break;
			case '?':
			case 'h':
				printf("Usage: %s [OPTIONS]\n", argv[0]);
				printf("  -b, --baudrate=BAUDRATE\tInitial serial baudrate. DEFAULT: %i.\n", DEFAULT_BAUDRATE);
				printf("  -c, --console=ON|OFF\t\tInitial console forwarding state. DEFAULT: ON.\n");
				printf("  -d, --device=DEVICE\t\tSerial port device. DEFAULT: %s.\n", DEFAULT_DEVICE);
				printf("  -e, --exploit=EXFILE\t\tFile with exploit (THUMB) to be installed. DEFAULT: None.\n");
				printf("  -h, --help\t\t\tPrint this help and exit.\n");
				printf("  -l, --logfile=LOGFILE\t\tLog file for terminal output activity. DEFAULT: None.\n");
				printf("  -m, --destmem=ADDRESS\t\tExploit destination memory address. DEFAULT: 0x%08X.\n", DEFAULT_DESTMEM);
				printf("  -p, --jumppoint=ADDRESS\tWrite exploit jump address at this point. DEFAULT: 0x%08X.\n", DEFAULT_JUMPPOINT);
				printf("\t\t\t\tDefault value is \"Ctrl-E\" command handler in 7200.12 fw CC38\n");
				printf("  -v, --version\t\t\tPrint version number and exit.\n");
				printf("  -w, --warning=ON|OFF\t\tInitial output data format warning state. DEFAULT: ON.\n");
				exit(0);
				break;
			case 'l':
				if (optarg == NULL) {
					printf("You must specify log filename.\n");
					exit(EINVAL);
				}
				logfile = fopen(optarg, "a");
				if (!logfile) {
					err = errno;
					printf("Cannot open log file \"%s\".\n", optarg);
					exit(err);
				}
				setvbuf(logfile, NULL, _IOLBF, 0);
				break;
			case 'm':
				if (optarg == NULL) {
					printf("You must specify exploit destination address.\n");
					exit(EINVAL);
				}
				destmem = strtoul(optarg, NULL, 0);
				if (destmem > 0x4000) {
					printf("WARNING: Exploit destination address is probably incorrect!\n");
				}
				break;
			case 'p':
				if (optarg == NULL) {
					printf("You must specify exploit jump point address.\n");
					exit(EINVAL);
				}
				jumppoint = strtoul(optarg, NULL, 0);
				if ((jumppoint < 0x400000) || (jumppoint > 0x10000000)) {
					printf("WARNING: Exploit jump point address is probably incorrect!\n");
				}
				break;
			case 'v':
				printf("%s: version: %s\n", argv[0], VERSION);
				exit(0);
				break;
			case 'w':
				if (optarg != NULL) {
					if ((!strcmp(optarg, "ON")) || (!strcmp(optarg, "1"))) {
						datawarning = true;
					} else if ((!strcmp(optarg, "OFF")) || (!strcmp(optarg, "0"))) {
						datawarning = false;
					} else {
						printf("Initial output data format warning state must be \"ON\" or \"OFF\".\n");
						exit(EINVAL);
					}
				}
				break;
		}
	}

	if (!device) {
		device = strdup(DEFAULT_DEVICE);
	}
}

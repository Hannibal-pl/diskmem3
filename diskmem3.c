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

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

#include "diskmem.h"

struct TRANSFER {
	volatile bool active;
	FILE *file;
	unsigned from;
	unsigned to;
	unsigned curr;
	unsigned step;
	char type;
	bool waswarning;
	bool write;
	void* writebuf;
	time_t time;
	void (*atend)(void);
};

bool consoleoutput = true;
bool datawarning = true;
bool interactive = false;
bool controlchars = true;
char readbuf[1025];
char stdinbuf[1025];
int baudrate = DEFAULT_BAUDRATE;
char *device = NULL;
char *exploit = NULL;
unsigned exlen = 0;
unsigned destmem = DEFAULT_DESTMEM;
unsigned jumppoint = DEFAULT_JUMPPOINT;
FILE *logfile = NULL;
struct TRANSFER transfer = { false, NULL, 0, 0, 0, 0, '\0', false, false, NULL, 0, NULL};
tcflag_t inlflag;

struct pollfd pfd[2];

FILE * openfile(unsigned from, unsigned to, char type) {
	char buf[32];

	if (to <= from) {
		return NULL;
	}

	switch (type) {
		case 'b':
			break;
		case 'w':
			if ((from & 1) || (to & 1)) {
				return NULL;
			}
			break;
		case 'd':
			if ((from & 3) || (to & 3)) {
				return NULL;
			}
			break;
		default:
			return NULL;
	}

	snprintf(buf, sizeof(buf), "%c0x%08X-0x%08X.dmp", type, from, to);

	return fopen(buf, "w");
}

bool peekcommand(int fd, unsigned addr, char type) {
	char buf[32];
	unsigned len;
	struct pollfd lpfd[1];

	lpfd[0].fd = fd;
	lpfd[0].events = POLLOUT;

	switch (type) {
		case 'b':
		case 'w':
			snprintf(buf, sizeof(buf), "%c%04x,%04x\n", type == 'b' ? '+' : '-', addr / 0x10000, addr % 0x10000);
			break;
		case 'd':
			snprintf(buf, sizeof(buf), "+%08x,,,4\n", addr);
			break;
		default:
			return false;
	}

	poll(lpfd, 1, -1);
	len = strlen(buf);
	if (write(fd, buf, len) != len) {
		return false;
	}

	return true;
}

bool pokecommand(int fd, unsigned addr, unsigned val, char type) {
	char buf[32];
	unsigned len;
	struct pollfd lpfd[1];

	lpfd[0].fd = fd;
	lpfd[0].events = POLLOUT;

	switch (type) {
		case 'b':
			snprintf(buf, sizeof(buf), "=%04x,%04x,%02x,1\n", addr / 0x10000, addr % 0x10000, val & 0xFF);
			break;
		case 'w':
			snprintf(buf, sizeof(buf), "=%04x,%04x,%04x,2\n", addr / 0x10000, addr % 0x10000, val & 0xFFFF);
			break;
		case 'd':
			snprintf(buf, sizeof(buf), "=%04x,%04x,%08x,4\n", addr / 0x10000, addr % 0x10000, val);
			break;
		default:
			return false;
	}

	poll(lpfd, 1, -1);
	len = strlen(buf);
	if (write(fd, buf, len) != len) {
		return false;
	}

	return true;
}

int newtransfer(unsigned from, unsigned to, char type) {
	int err;

	if (transfer.active) {
		printf("Transfer already in progress.\n");
		return EBUSY;
	}

	switch (type) {
		case 'b':
			transfer.step = 1;
			break;
		case 'w':
			if ((from & 1) || (to & 1)) {
				printf("In \"word-size\" transfers - start and stop addres must be 2 aligned.\n");
				return EINVAL;
			}
			transfer.step = 2;
			break;
		case 'd':
			if ((from & 3) || (to & 3)) {
				printf("In \"dword-size\" transfers - start and stop address must be 4 aligned.\n");
				return EINVAL;
			}
			transfer.step = 4;
			break;
		default:
			printf("Unknown transfer size.\n");
			return EINVAL;
	}

	if (from >= to) {
		printf("Start address must be grater than stop address.\n");
		return EINVAL;
	}

	transfer.from = transfer.curr = from;
	transfer.to = to;
	transfer.type = type;
	transfer.file = openfile(from, to, type);
	if (!transfer.file) {
		err = errno;
		printf("Cannot open destination file.\n");
		return err;
	}

	transfer.time = time(NULL);
	transfer.waswarning = false;
	transfer.write = false;
	transfer.atend = NULL;
	transfer.active = true;

	return 0;
}

void installexploit(void) {
	char buf[32];

	snprintf(buf, sizeof(buf), "=%04X,%04X,%08X,4\n", jumppoint / 0x10000, jumppoint % 0x10000, destmem + 1);
	write(sfd, buf, strlen(buf));
}

int writeexploit() {
	if (transfer.active) {
		printf("Transfer already in progress.\n");
		return EBUSY;
	}

	transfer.step = 1;
	transfer.from = transfer.curr = destmem;
	transfer.to = destmem + exlen + 1;
	transfer.type = 'b';
	transfer.file = NULL;
	transfer.time = time(NULL);
	transfer.waswarning = false;
	transfer.write = true;
	transfer.writebuf = exploit;
	transfer.atend = installexploit;
	transfer.active = true;

	return 0;
}


void endtransfer(void) {
	unsigned all = transfer.to - transfer.from;
	unsigned tilnow = transfer.curr - transfer.from;
	unsigned dt = time(NULL) - transfer.time;

	if (!transfer.active) {
		return;
	}

	if (transfer.curr == transfer.to) {
		printf("Ending completed transfer.\n");
		printf("Copied %d bytes in %d seconds with average speed %.3f kiB/s.\n", all, dt, (double)(all) / ((double)(dt * 1024)));
	} else {
		printf("Ending uncompleted transfer (%.1f%% done).\n", ((double)(tilnow)) * 100.0 / ((double)(all)));
		printf("Copied %d bytes in %d seconds with average speed %.3f kiB/s.\n", tilnow, dt, (double)(tilnow) / ((double)(dt * 1024)));
	}

	if (transfer.waswarning) {
		printf("There was data format warning(s) during transfer, assume that dumped data may be inaccurate of event totally broken!\n");
	}

	transfer.active = false;

	if (transfer.atend) {
		transfer.atend();
	}

	if (transfer.file) {
		fclose(transfer.file);
		transfer.file = NULL;
	}

	transfer.type = '\0';
	transfer.time = 0;
	transfer.waswarning = false;
	transfer.write = false;
	transfer.from = transfer.to = transfer.curr = transfer.step = 0;
}

void infotransfer(void) {
	unsigned all = transfer.to - transfer.from;
	unsigned tilnow = transfer.curr - transfer.from;

	if (!transfer.active) {
		printf("No transfer in progress.\n");
	} else {
		printf("Transfered %u out of %u bytes (%.1f%% done).\n", tilnow, all, ((double)(tilnow)) * 100.0 / ((double)(all)));
	}
}

int getparams(char *buf, unsigned *params, int max) {
	int i;
	char *newbuf;

	for (i = 0; i < max; i++) {
		params[i] = strtoul(buf, NULL, 0);
		newbuf = strchr(buf, ';');
		if (!newbuf) {
			break;
		}
		buf = newbuf + 1;
	}

	return i + 1;
}

void uflood(void) {
	struct pollfd lpfd[1];
	time_t tm = time(NULL) + 10;
	char buf[1025];
	int len;

	lpfd[0].fd = sfd;
	lpfd[0].events = POLLOUT | POLLIN;

	printf("\"U\" flood start.\n");
	while(time(NULL) < tm) {
		poll(lpfd, 1, -1);
		if (lpfd[0].revents == POLLIN) {
			len = read(sfd, buf, sizeof(buf) - 1);
			if (len > 0) {
				buf[len] = '\0';
				if (strstr(buf, "Encountered abort") || strstr(buf, "Flash code disabled")) {
					printf("\"U\" flood stopped - target achieved.\n");
					printf("%s", buf);
					return;
				}
			}
		}
		if (lpfd[0].revents == POLLOUT) {
			write(sfd, "UUUUUUUUUUUUUUUU", 16);
		}
	}
	printf("\"U\" flood end - no effect.\n");
}

void stdinprocess(void) {
	int speed;
	int realspeed;
	unsigned params[2];
	int count;
	char ch;
	int i;
	struct termios inopts;

	count = read(0, stdinbuf, sizeof(stdinbuf) - 1);
	stdinbuf[count] = 0;

	if (interactive) {
		for (i = 0; i < count; i++) {
			if (stdinbuf[i] != '`') {
				write(sfd, &stdinbuf[i], 1);
			} else {
				interactive = false;
				write(sfd, "/T\n", 3);
				printf("Disabling interactive mode - hit I [Enter] to enable it again.\n");
				break;
			}
		}
		return;
	}

	for (i = 0; i < count; i++) {
		ch = tolower(stdinbuf[i]);
		switch (ch) {
			case 'a':
				datawarning = !datawarning;
				printf("Unknown output data format warning is %s.\n", datawarning ? "enabled" : "disabled");
				break;
			case 'b':
			case 'w':
			case 'd':
				if (transfer.active) {
					printf("Transfer in progress, cannot start another yet.\n");
				} else {
					count = getparams(&stdinbuf[i + 1], params, 2);
					if (count >= 2) {
						newtransfer(params[0], params[1], ch);
						peekcommand(sfd, transfer.curr, transfer.type);
					} else {
						printf("To few parameters to \"%c\" command.\n", ch);
					}
				}
				break;
			case 'c':
				consoleoutput = !consoleoutput;
				printf("Console output forwading %s.\n", consoleoutput ? "enabled" : "disabled");
				break;
			case 'e':
				exit(0);
			case 'h':
			case '?':
				printf("\n");
				printf("a\t\t- output data format warning toggle.\n");
				printf("b[from];[to]\t- read memory by byte to file.\n");
				printf("c\t\t- toggle console output forwarding.\n");
				printf("d[from];[to]\t- read (4 aligned) memory by dword to file.\n");
				printf("e\t\t- exit program.\n");
				printf("h\t\t- print this help.\n");
				printf("i\t\t- interactive mode, hit \"`\" (backtick) to exit.\n");
				printf("l\t\t- print disk info.\n");
				printf("o\t\t- toggle control chracters on stdin.\n");
				printf("r\t\t- write and install exploit.\n");
				printf("s[speed]\t- set serial speed.\n");
				printf("t\t\t- print transfer status.\n");
				printf("u\t\t- 10s long u flood.\n");
				printf("w[from];[to]\t- read (2 aligned) memory by word to file.\n");
				printf("x\t\t- stop actvie transfer.\n");
				printf("z\t\t- init terminal.\n");
				break;
			case 'i':
				if (transfer.active) {
					printf("Transfer in progress, cannot enter interactive mode now.\n");
				} else {
					printf("Enabling interactive mode - hit \"`\" (backtick) to exit.");
					write(sfd, "\x1A", 1);
					consoleoutput = true;
					interactive = true;
				}
				break;
			case 'l':
				if (transfer.active) {
					printf("Command not available during transfer.\n");
				} else {
					write(sfd, "\x0C", 1);
				}
				break;
			case 'o':
				controlchars = !controlchars;
				printf("Control characters on stdin %s.\n", controlchars ? "enabled" : "disabled");
				tcgetattr(0, &inopts);
				if (controlchars) {
					inopts.c_lflag = inlflag;
				} else {
					inlflag = inopts.c_lflag;
					printf("Please note that combinations like \"Ctrl-C\" are not available from now.\n");
					inopts.c_lflag &= ~(ICANON | IEXTEN | ISIG);
				}
				tcsetattr(0, TCSAFLUSH, &inopts);
				break;
			case 'r':
				if (!exploit) {
					printf("Exploit was not loaded.\n");
				} else if (transfer.active) {
					printf("Cannot run exploint during transfer.\n");
				} else {
					writeexploit();
					pokecommand(sfd, transfer.curr, *(int *)transfer.writebuf, transfer.type);
				}
				break;
			case 's':
				if (transfer.active) {
					printf("Transfer in progress, baudrate change not available.\n");
				} else {
					speed = strtoul(&stdinbuf[i + 1], NULL, 0);
					if (setspeed(sfd, speed, &realspeed)) {
						printf("Change baudrate to %i (requested %i).\n", realspeed, speed);
					} else {
						printf("Failed to change baudrate to %i.\n", speed);
					}
				}
				break;
			case 't':
				infotransfer();
				break;
			case 'u':
				uflood();
				break;
			case 'x':
				if (transfer.active) {
					transfer.atend = NULL;
					endtransfer();
				} else {
					printf("No transfer in progress.\n");
				}
				break;
			case 'z':
				if (transfer.active) {
					printf("Command not available during transfer.\n");
				} else {
					write(sfd, "\x1A", 1);
				}
				break;
			case '\n':
				printf("Empty comand 0 try \"h\" for help.\n");
				break;
			default:
				printf("Unknown command \"%c\" - try \"h\" for help.\n", ch);
				break;
		}

		for (++i; i < count; i++) {
			if (stdinbuf[i] == '\n') {
				i++;
				break;
			}
		}
	}
}

void datapeekprocess(char *buf, int len) {
	unsigned temp;
	char *eq;

	if (!transfer.active) {
		return;
	}

	if ((buf[0] == '+') || (buf[0] == '-') || strstr(buf, "T>")) {
		/* command echo */
		return;
	} else if ((transfer.step << 1) == (len & 0xFFFFFFFE)) {
		/* only value and \n (7200.10 and earlier) */
		temp = strtoul(buf, NULL, 16);
	} else {
		eq = strchr(buf, '=');
		if (eq) {
			temp = strtoul(eq + 2, NULL, 16);
		} else {
			if (datawarning) {
				printf("Warning! Unknown or broken output data format.\n");
			}
			temp = strtoul(buf, NULL, 16);
		}
	}

	switch (transfer.type) {
		case 'b':
			temp &= 0xFF;
			fwrite(&temp, 1, 1, transfer.file);
			break;
		case 'w':
			temp &= 0xFFFF;
			fwrite(&temp, 2, 1, transfer.file);
			break;
		case 'd':
			fwrite(&temp, 4, 1, transfer.file);
			break;
		default:
			printf("Unknown transfer type in impossible place.\n");
			exit(EINVAL);
			break;
	}

	transfer.curr += transfer.step;
	if (transfer.curr < transfer.to) {
		peekcommand(sfd, transfer.curr, transfer.type);
	} else {
		endtransfer();
	}
}

void datapokeprocess(char *buf, int len) {
	unsigned *curdata;

	if (!strstr(buf, "Adr")) {
		return;
	}

	transfer.curr += transfer.step;
	if (transfer.curr < transfer.to) {
		curdata = (unsigned *)(transfer.writebuf + (transfer.curr - transfer.from));
		pokecommand(sfd, transfer.curr, *curdata, transfer.type);
	} else {
		endtransfer();
	}
}

void onexit(void) {
	endtransfer();

	if (device != NULL) {
		free(device);
	}

	if (exploit != NULL) {
		free(exploit);
	}

	if (logfile) {
		fclose(logfile);
	}

	if (sfd != -1) {
		close(sfd);
	}
}

int main(int argc, char *argv[]) {
	int count = 0;
	int readed;
	int err;
	bool hasn = false;

	atexit(onexit);
	parseparams(argc, argv);
	err = openserial();
	if (err != 0) {
		printf("%s.\n", strerror(err));
		exit(err);
	}

	setvbuf(stdout, NULL, _IONBF, 0);

	pfd[1].fd = 0;
	pfd[1].events = POLLIN;
	pfd[0].fd = sfd;
	pfd[0].events = POLLIN;

	while(true) {
		poll(pfd, 2, -1);
		if (pfd[0].revents == POLLIN) {
			do {
				readed = read(sfd, &readbuf[count], 1);
				if (readed < 1) {
					break;
				}

				if (logfile) {
					fputc(readbuf[count], logfile);
				}

				if (readbuf[count++] == '\n') {
					hasn = true;
					break;
				}
			} while (count < sizeof(readbuf) - 1);

			if (count == 0) {
				goto stdinp;
			}

			readbuf[count] = 0;

			if (transfer.active) {
				if (hasn == false) {
					goto stdinp;
				}

				if (transfer.write) {
					datapokeprocess(readbuf, count);
				} else {
					datapeekprocess(readbuf, count);
				}
			}

			if (consoleoutput) {
				printf("%s", readbuf);
			}
			hasn = false;
			count = 0;

		}

stdinp:

		if (pfd[1].revents == POLLIN) {
			stdinprocess();
		}
	}

	printf("Impossible exit way!\n");
	return 0;
}

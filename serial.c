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
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/stat.h>
#include <poll.h>
#include <time.h>

#include "diskmem.h"

int sfd = -1;
char lastcommand[MAX_COMMAND_LEN];

struct SSPEED {
	unsigned baudrate;
	unsigned mask;
	char command[16];
};

struct SSPEED sspeed[] = {
	{50,		B50,		"B50"},
	{75,		B75,		"B75"},
	{110,		B110,		"B110"},
	{134,		B134,		"B134"},
	{150,		B150,		"B150"},
	{200,		B200,		"B200"},
	{300,		B300,		"B300"},
	{600,		B600,		"B600"},
	{1200,		B1200,		"B1200"},
	{1800,		B1800,		"B1800"},
	{2400,		B2400,		"B2400"},
	{4800,		B4800,		"B4800"},
	{9600,		B9600,		"B9600"},
	{19200,		B19200,		"B19200"},
	{38400,		B38400,		"B38400"},
	{57600,		B57600,		"B57600"},
	{115200,	B115200,	"B115200"},
	{230400,	B230400,	"B230400"},
	{460800,	B460800,	"B460800"},
	{500000,	B500000,	"B500000"},
	{576000,	B576000,	"B576000"},
	{921600,	B921600,	"B921600"},
	{1000000,	B1000000,	"B1000000"},
	{1152000,	B1152000,	"B1152000"},
	{1500000,	B1500000,	"B1500000"},
	{2000000,	B2000000,	"B2000000"},
	{2500000,	B2500000,	"B2500000"},
	{3000000,	B3000000,	"B3000000"},
	{3500000,	B3500000,	"B3500000"},
	{4000000,	B4000000,	"B4000000"},
	{0,		0,		""}};

int selectbaud(int baudrate) {
	int i;

	if (baudrate < sspeed[0].baudrate) {
		return -1;
	}

	for (i = 0; sspeed[i].baudrate; i++) {
		if ((baudrate >= sspeed[i].baudrate) && (baudrate < sspeed[i + 1].baudrate)) {
			return i;
		}
	}

	return i - 1;
}

bool initserial(int baudrate) {
	struct termios opts;
	int baudi;

	if (tcgetattr(sfd, &opts) < 0) {
		return false;
	}

	baudi = selectbaud(baudrate);
	if (baudi < 0 ) {
		return false;
	}
	cfsetispeed(&opts, sspeed[baudi].mask);
	cfsetospeed(&opts, sspeed[baudi].mask);

	opts.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
	opts.c_cflag |= (CLOCAL | CREAD | CS8);

	opts.c_iflag &= ~(ICRNL | IGNCR | INLCR | ISTRIP | IUCLC);
	opts.c_iflag |= (IXON | IXOFF);

	opts.c_oflag &= ~(OPOST | OCRNL | ONLCR | ONLRET | ONOCR);

	opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	opts.c_cc[VMIN] = 1;
	opts.c_cc[VTIME] = 0;

	if (tcsetattr(sfd, TCSANOW, &opts) < 0) {
		return false;
	}

	return true;
}

int openserial(void) {
	int err;
	struct stat s;

	err = stat(device, &s);
	if (err != 0) {
		err = errno;
		printf("Stat failed on \"%s\".\n", device);
		return err;
	}

	if (!S_ISCHR(s.st_mode)) {
		printf("\"%s\" is not a character device.\n", device);
		return EINVAL;
	}

	sfd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (sfd == -1) {
		err = errno;
		printf("Failed to open device \"%s\".\n", device);
		return err;
	}

	fcntl(sfd, F_SETFL, FNDELAY);

	if (!initserial(baudrate)) {
		err = errno;
		printf("Failed to initialize serial port.\n");
		return errno;
	}

	return 0;
}

void closeserial(void) {
	if (sfd != -1) {
		close(sfd);
	}
}

bool setspeed(int baudrate, int *realset) {
	struct termios opts;
	int baudi;

	if (tcgetattr(sfd, &opts) < 0) {
		return false;
	}

	baudi = selectbaud(baudrate);
	if (baudi < 0 ) {
		return false;
	}

	sendcommand("%s\n", sspeed[baudi].command);

	cfsetospeed(&opts, sspeed[baudi].mask);
	cfsetispeed(&opts, sspeed[baudi].mask);
	sleep(1);

	if (tcsetattr(sfd, TCSAFLUSH, &opts) < 0) {
		return false;
	}

	if (realset != NULL) {
		*realset = sspeed[baudi].baudrate;
	}

	return true;
}

bool sendcommand(char *format, ...) {
	char command[MAX_COMMAND_LEN];
	unsigned len;
	struct pollfd lpfd[1];

	va_list ap;
	va_start(ap, format);
	len = vsnprintf(command, MAX_COMMAND_LEN, format, ap);
	va_end(ap);

	if (len >= MAX_COMMAND_LEN) {
		return false;
	}

	lpfd[0].fd = sfd;
	lpfd[0].events = POLLOUT;
	poll(lpfd, 1, -1);

	if (write(sfd, command, len) != len) {
		return false;
	}

	strcpy(lastcommand, command);
	return true;
}

bool sendbyte(char byte) {
	struct pollfd lpfd[1];

	lpfd[0].fd = sfd;
	lpfd[0].events = POLLOUT;
	poll(lpfd, 1, -1);

	if (write(sfd, &byte, 1) != 1) {
		return false;
	}

	return true;
}

void uflood(unsigned sec) {
	struct pollfd lpfd[1];
	time_t tm;
	char buf[1025];
	int len;

	if (sec < 1) {
		printf("Warning: Period to short - reset to 1s.\n");
		sec = 1;
	} else if (sec > 60) {
		printf("Warning: Period to long - reset to 60s.\n");
		sec = 60;
	}

	lpfd[0].fd = sfd;
	lpfd[0].events = POLLOUT | POLLIN;
	tm = time(NULL) + sec;

	printf("%is long \"U\" flood start.\n", sec);
	while(time(NULL) < tm) {
		usleep(1000);
		poll(lpfd, 1, sec * 1000);
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
			sendcommand("U");
		}
	}
	printf("\"U\" flood end - no effect.\n");
}

bool isecho(char *str) {
	if (abs(strlen(str) - strlen(lastcommand)) > 1) {
		return false;
	}

	return strncmp(lastcommand, str, MAX_COMMAND_LEN) ? false : true;
}

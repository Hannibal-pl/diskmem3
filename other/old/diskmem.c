#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <stdbool.h>
#include <poll.h>

int consoleoutput = 1;
char readbuf[1025];
char stdinbuf[1025];
int bfrom;
int bcur;
int bto;
FILE *bfile = NULL;
int wfrom;
int wcur;
int wto;
FILE *wfile = NULL;

struct pollfd pfd[2];

void initserial(int fd) {
	struct termios opts;

	tcgetattr(fd, &opts);
	cfsetispeed(&opts, B38400);
	cfsetospeed(&opts, B38400);
	opts.c_cflag &= ~PARENB;
	opts.c_cflag &= ~CSTOPB;
	opts.c_cflag &= ~CSIZE;
	opts.c_cflag |= CS8;
	opts.c_cflag |= (CLOCAL | CREAD);
	opts.c_iflag |= (IXON | IXOFF);
	opts.c_oflag &= ~OPOST;
	opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	opts.c_iflag &= ~(ISTRIP | IGNCR | INLCR | ICRNL | ONOCR);
	opts.c_cc[VMIN] = 1;
	opts.c_cc[VTIME] = 0;
	tcsetattr(fd, TCSAFLUSH, &opts);
}

void getbparams(char *params) {
	char *to;

	bfrom = strtoul(params, NULL, 0);
	to = strchr(params, ';');
	if (to == NULL) {
		bto = bfrom + 1;
	} else {
		bto = strtoul(to + 1, NULL, 0);
	}

	if (bfrom >= bto) {
		bto = bfrom + 1;
	}

	bcur = bfrom;
}

void openbfile(void) {
	char nbuf[64];

	snprintf(nbuf, sizeof(nbuf), "b0x%08X-0x%08X.dmp", bfrom, bto);
	bfile = fopen(nbuf, "w");
}

void peekbyte(int fd, int byte) {
	char peekbuf[32];

	snprintf(peekbuf, sizeof(peekbuf), "+%04x,%04x\n", byte / 0x10000, byte % 0x10000);
	write(fd, peekbuf, strlen(peekbuf));
}

void peekword(int fd, int word) {
	char peekbuf[32];

	snprintf(peekbuf, sizeof(peekbuf), "-%04x,%04x\n", word / 0x10000, word % 0x10000);
	write(fd, peekbuf, strlen(peekbuf));
}

void getwparams(char *params) {
	char *to;

	wfrom = strtoul(params, NULL, 0) & 0xFFFFFFFE;
	to = strchr(params, ';');
	if (to == NULL) {
		wto = wfrom + 2;
	} else {
		wto = (strtoul(to + 1, NULL, 0) + 1) & 0xFFFFFFFE;
	}

	if (wfrom >= wto) {
		wto = wfrom + 2;
	}

	wcur = wfrom;
}

void openwfile(void) {
	char nbuf[64];

	snprintf(nbuf, sizeof(nbuf), "w0x%08X-0x%08X.dmp", wfrom, wto);
	wfile = fopen(nbuf, "w");
}

void setspeed(int fd, char *params) {
	struct termios opts;
	int speed = strtoul(params, NULL, 0);

	tcgetattr(fd, &opts);

	if (speed < 57600) {
		cfsetispeed(&opts, B38400);
		cfsetospeed(&opts, B38400);
		write(fd, "B38400\n", 7);
	} else if (speed < 115200) {
		cfsetispeed(&opts, B57600);
		cfsetospeed(&opts, B57600);
		write(fd, "B57600\n", 7);
	} else if (speed < 230400) {
		cfsetispeed(&opts, B115200);
		cfsetospeed(&opts, B115200);
		write(fd, "B115200\n", 8);
	} else {
		cfsetispeed(&opts, B230400);
		cfsetospeed(&opts, B230400);
		write(fd, "B230400\n", 8);
	}
	tcsetattr(fd, TCSAFLUSH, &opts);
}

int main(int argc, char *argv[]) {
	int fd, count, i;

	if (argc > 1) {
		fd = open(argv[1], O_RDWR | O_NOCTTY | O_NDELAY);
	} else {
		fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);
	}

	if (fd == -1) {
		printf("Cannot open serial\n");
		exit(-1);
	}
	fcntl(fd, F_SETFL, FNDELAY);
	initserial(fd);

	pfd[1].fd = 0;
	pfd[1].events = POLLIN;
	pfd[0].fd = fd;
	pfd[0].events = POLLIN;

	setvbuf(stdout, NULL, _IONBF, 0);

	while(true) {
		poll(pfd, 2, 0);
		if (pfd[0].revents == POLLIN) {
			count = read(fd, readbuf, sizeof(readbuf) - 1);
			if (count == 0) {
				continue;
			}
			readbuf[count] = 0;
			if (consoleoutput) {
				printf("%s", readbuf);
			}

			if ((bcur > bfrom) && (bcur <= bto + 1)) {
				char val;
				char *eq = strchr(readbuf, '=');
				while (eq) {
					val = (char)strtoul(eq + 2, NULL, 16);
					fwrite(&val, 1, 1, bfile);
					eq = strchr(eq + 2, '=');
				}
			}
			if ((bcur > bfrom) && (bcur < bto)) {
				if (strstr(readbuf, "T>")) {
					peekbyte(fd, bcur++);
				}
			}
			if (bcur == bto + 1) {
				fclose(bfile);
				bfile = NULL;
				bcur = bfrom - 1;
			}

			if ((wcur > wfrom) && (wcur <= wto + 2)) {
				short val;
				char *eq = strchr(readbuf, '=');
				while (eq) {
					val = (short)strtoul(eq + 2, NULL, 16);
					fwrite(&val, 2, 1, wfile);
					eq = strchr(eq + 2, '=');
				}
			}
			if ((wcur > wfrom) && (wcur < wto)) {
				if (strstr(readbuf, "T>")) {
					peekword(fd, wcur);
					wcur += 2;
				}
			}
			if (wcur == wto + 1) {
				fclose(wfile);
				wfile = NULL;
				wcur = wfrom - 1;
			}


		}
		if (pfd[1].revents == POLLIN) {
			count = read(0, stdinbuf, sizeof(stdinbuf) - 1);
			stdinbuf[count] = 0;
			for (i = 0; i < count; i++) {
				switch (stdinbuf[i]) {
					case 'b':
					case 'B':
						getbparams(&stdinbuf[i + 1]);
						openbfile();
						peekbyte(fd, bcur++);
						break;
					case 'c':
					case 'C':
						consoleoutput ^= 1;
						break;
					case 'e':
					case 'E':
						goto end;
					case 'h':
					case 'H':
						printf("\n");
						printf("b[from];[to]\t- read memory by byte to file.\n");
						printf("c\t\t- toggle console output.\n");
						printf("e\t\t- exit program.\n");
						printf("h\t\t- print this help.\n");
						printf("l\t\t- print disk info.\n");
						printf("s[speed]\t- set serial speed.\n");
						printf("w[from];[to]\t- read memory by word to file.\n");
						printf("z\t\t- init terminal.\n");
						break;
					case 'l':
					case 'L':
						write(fd, "\x0C", 1);
						break;
					case 's':
					case 'S':
						setspeed(fd, &stdinbuf[i + 1]);
						break;
					case 'w':
					case 'W':
						getwparams(&stdinbuf[i + 1]);
						openwfile();
						peekword(fd, wcur);
						wcur += 2;
						break;
					case 'z':
					case 'Z':
						write(fd, "\x1A", 1);
						break;
				}
			}
		}
	}

end:
	if (bfile) {
		fclose(bfile);
	}
	if (wfile) {
		fclose(wfile);
	}
	close(fd);
}
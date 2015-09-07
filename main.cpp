// warning! some functions in this code are Linux specific and may \ will require additional libraries to be linked to the compiler

#include <iostream>
//#include <fstream>		// file operations. fstream
#include <stdio.h>			// standard input / output functions
#include <string.h>			// string function definitions. bzero()
#include <sys/time.h>		// time
#include <time.h>
//#include <iomanip>		// std::setfill, std::setw
#include <errno.h>			// Error number definitions
#include <unistd.h>			// UNIX standard function definitions
#include <fcntl.h>			// File control definitions
#include <termios.h>		// POSIX terminal control definitions
#include <vector>			// Vectors container

using namespace std;

#define T_TIMER 1		// 2 & 3 needs to be compiled with -lrt to Link the Real Time shared library
#define TEST_LVL 0	// 1 - time, 2 - parser
#define pkgoffset 17	// startbits do not count

// ---------------------------------------------

struct pack_
{
	bool SBit;				// 2	
	int plen;				// 2
	int id;				// 2
	unsigned int time;		// 8
	int crc;				// 4
	int dlen;
	char data[1024];
};

// ---------------------------------------------

class file_
{
	FILE * pFile;
public:
	int fLen;										// n of bytes read
	char * buffer;
	// ------------
	file_();
	file_(char *);									// read file
	file_(char *, char *, int);							// write to file
	~file_() { delete[] buffer; }
	int printHEX(int, unsigned int);
};

class test_data
{
	private:
		char* data;
		int len;
	public:	
		int t_datarate();
		void t_dataloss();
		int t_latency();
		int t_speed();
		int t_con_spd();
		int t_dist();
		// -------------------------------------
		unsigned int  get_time(void);
		int get_time_diff(unsigned int);  				// elapsed time, 4bytes
};

class serial_
{
public:
	int fd;										// File descriptor for the port
	int read_port_flag;
	// ------------
	serial_();
	~serial_();										// close the serial port
	serial_(char*, int, bool);
	int send(char*);
	int send(char*, int, int, test_data*);
	int receive(char *, unsigned int);
	int receive_and_parse(vector<pack_> *);
};

// =============================================
// =============================================
// =============================================

// ---------------------------------------------
// Files
// ---------------------------------------------

file_::file_(char *fName)								//read from file
{
	pFile = fopen (fName,"rb");
	if (pFile==NULL)
	{
		cout << "Unable to open file " << fName << endl << errno << endl;
		return;
	}
	setvbuf(pFile, NULL, _IONBF, 255);				// No buffer is used. Each I/O operation is written as soon as possible.
	fseek(pFile, 0, SEEK_END);						// go to the end of file, i.e. get file size
	fLen = ftell(pFile);								// Get current position in stream
	fseek(pFile, 0, SEEK_SET);						// go to beginning of file
	cout << "Size: " << fLen << "\n";
	// buffer = (char*) malloc (fLen+1);					// create a buffer for entire file.		The type of returned pointer is always void*, which can be cast to the desired type of data pointer in order to be dereferenceable.
	buffer = new char[fLen];							// create a buffer for entire file size
	if (!buffer)
	{
		cout << "Error while allocating memory" << endl;
		fclose(pFile);
		return;
	}
	fread(buffer, fLen, 1, pFile);						// read fLen bytes to BUFFER from pFile  
	fclose (pFile);
}

file_::file_(char *fName, char * buf, int len)				// write to file
{
	pFile = fopen (fName,"wb");
	fwrite (buf, len, 1, pFile);							// write 1 element of lenght len from buf to pFile
	fclose (pFile);
}

/*
file_::file_(char *fName, char * buf, int len)				// append to file
{
	pFile = fopen (fName,"wb");
	fwrite (buf, len, 1, pFile);							// write 1 element of lenght len from buf to pFile
	fclose (pFile);
}
*/

int file_::printHEX(int pos, unsigned int len)
{
/*
	1	pos = 0, len = 0		print all
	2	pos = 10, len = 0		print all from 10'th element to the end
	3	pos = 0, len = 10		print first 10 elements
	4	pos = 10, len = 10		print 10 elements beginning at 10th
	5	pos = -10, len = 0		print 10 last elements
	6	pos = -10, len = 5		print 5 elements, beginning from 10th at the end
*/
	if (pos != fLen) {
		if (pos >= 0){
			if (pos >= fLen) {
				cout << "print error. pos out of scope. pos > fLen or equal\n";
				return -1;
			}
			if (len == 0) len = fLen;					// 1. print all (if pos = 0) && 2. print all from POS to the end
			else
				if (pos + len > fLen){
					cout << "warning. len is out of scope. len is automatically set from " << len << " to " << fLen << endl;
					len = fLen;
				}else
					len = pos + len;				// 3. print first 10 elements && 4. print first N elements from POS
		} else {									// for negative POS
			if (pos <= -fLen) {
				cout << "print error. pos out of scope. pos < fLen or equal\n";
				return -1;
			}
			if (len == 0){							// 5. print -POS last elements
				pos = fLen + pos;					// position from the end
				len = fLen;
			} else {								// 6. print N elements, beginning from -POS at the end
				pos = fLen + pos;
				len = fLen - len;
			}
		}
	} else {
		cout << "print error. pos = fLen\n";
		return -1;
	}

	int n=0;
	for (int i=pos; i<len; i++)
	{
		printf("%.2X ", (int)buffer[i]);					// print as hex, minimum number of digits 2
		if (i % 16 == 15)							// Display 16 bytes per line
			printf("\n");
			if (i % 4 == 3)							// separate each byte
				printf("   ");
		n++;
	}
	printf("\n");

	return n;
/*
	cout << std::setfill('_');
		for(size_t i = 0; i < fLen; ++i) {
			cout << hex << setw(2) << (int)buffer[i];	// hex - Use hexadecimal base. Sets the basefield format flag for the str stream to hex.
			cout << (((i + 1) % 16 == 0) ? "\n" : " ");	// if((i + 1) % 16 == 0) cout<<"\n"; else cout<<" ";
	}
	cout << endl;
*/
}

// ---------------------------------------------
// Serial
// ---------------------------------------------

serial_::serial_(char *devName, int spd = B9600, bool block = false)	// The values for speed are B115200, B230400, B9600, B19200, B38400, B57600, B1200, B2400, B4800, etc. The values for parity are 0 (meaning no parity), PARENB|PARODD (enable parity and use odd), PARENB (enable parity and use even), PARENB|PARODD|CMSPAR (mark parity), and PARENB|CMSPAR (space parity).
{
	fd = open(devName, O_RDWR | O_NOCTTY | O_NDELAY);	// name of the serial port,  O_RDWR - read and write access | O_CTTY - prevent other input (like keyboard) from affecting what we read | O_NDELAY - don't care if he other side is connected
	if (fd != -1)
	{
		//fcntl(fd, F_SETFL, 0);						// manipulate file descriptor. F_SETFL - set the file status flags to the value specified by arg
		struct termios tty_s;						// serial port settings
		bzero(&tty_s, sizeof(tty_s));					// The bzero() function sets the first n bytes of the area starting at s to zero (bytes containing aq\0aq). 
		
		cfsetispeed(&tty_s, spd);					// sets the input baud rate
		cfsetospeed(&tty_s, spd);					// sets the output baud rate
/*
	Setting  a bit		OR		|=		number |= 1 << x;
	Clearing a bit		AND		&= ~		number &= ~(1 << x)
	Toggling a bit		XOR		^= 		number ^= 1 << x
	Checking a bit 						bit = (number >> x) & 1
	Changing nth bit to x				number ^= (-x ^ number) & (1 << n);
*/
		tty_s.c_cflag |= (CLOCAL | CREAD);			// enable the receiver and set local mode.		CLOCAL - Ignore modem control lines.
		
		tty_s.c_cflag &= ~PARENB;					// no parity.		PARENB - Enable parity generation on output and parity checking for input.
		//tty_s.c_cflag |= par;							// set parity if specified
		tty_s.c_cflag &= ~CSTOPB;					// one stop bit.	CSTOPB - Set two stop bits, rather than one.
		
		tty_s.c_cflag &= ~CSIZE;						// Character size mask. Set character size as 8 bits
		tty_s.c_cflag |= CS8;						// Values are CS5, CS6, CS7, or CS8

		tty_s.c_lflag &= ~(ICANON | ECHO | ISIG);		// Raw input mode, sends the raw and unprocessed data  (send as it is)
		
		// Raw output mode, sends the raw and unprocessed data  (send as it is).
		// If it is in canonical mode and sending new line char then CR will be added as prefix and send as CR LF

		tty_s.c_oflag = ~OPOST;						// OPOST - Enable implementation-defined output processing.
		
		if(block != 0)
		{
			tty_s.c_cc[VMIN]  = block;
			tty_s.c_cc[VTIME] = 1;					// 0.5 seconds read timeout
		}
		
		if (tcsetattr (fd, TCSANOW, &tty_s) != 0)
			cout << "error setting term attributes. " << errno << endl;
		//tcsetattr(fd, TCSANOW, &tty_s);				// tcsetattr() sets the parameters associated with the terminal* from the termios structure*. TCSANOW - the change occurs immediately
	}else{										// Could not open the port.
		cout << "Unable to open port " << devName << endl;
		return;
	}
}

serial_::~serial_()
{
	if(close(fd) == -1) cout << "Error while closing port\n";
}

int serial_::send(char * buf, int len)						// returns n of bytes send
{
	int n = write(fd, buf, len);
	if (n < 0)
	{
		cout << "send() failed!\n";
		return -1;
	}
	return n;
}


int serial_::send(char * buf, int len, int dataSegm, test_data * t1)							// generate and send a package, returns n of bytes send
{
/*
	startbit		2
	p_lenght		2
	ID			2
	time			8
	CRC			4
	DATA
*/
	int n = 0, p = 0;					// n = bytes rec., p = packages rec.
	int dlen = 0;
	int dataOffset = pkgoffset;			// n of bytes taken by the header
	for (int i = 0; i < len; ++i)
	{
		if (i*dataSegm < len)
		{
			p++;
			if (len-i*dataSegm < dataSegm){				// fix to prevent overflow
				dlen = len - i*dataSegm;
			}else{
				dlen = dataSegm;
			}

			char t_string2[dataOffset];
			int plen = sprintf(t_string2, "\n\n%.2x%.2x%.9u4444", dataOffset+dlen, i, t1->get_time());
			
			n += write(fd, t_string2, plen);					// temp solution
			n += write(fd, &(buf[i*dataSegm]), dlen);			//
		}
		else
		{
			cout << "* loop terminated" << endl;
			break;
		}
	}
	cout << "Sent: " << p << " packages (" << n << " bytes)." << endl;
	if (n < 0)
	{
		cout << "send() failed!\n";
		// return -1;
	}
	return n;
}

int serial_::receive(char * buf, unsigned int len)
{
	int n = read (fd, buf, len);
	if (n < 0)
	{
		cout << "recieve() failed!\n";
		return -1;
	}
	return n;
}

int serial_::receive_and_parse(vector<pack_> * buf)
{
	int n = 0, n2 = 0;					// n bytes read, n2 total bytes read
	int pkgcnt = 0;
	int len = -1;						// bytes of package recieved
	char temp[10] = {0};
	char tpkg[1024] = {0};
	char cbuf = '\0';
	bool hBeg = false;

	pack_ p_;
	p_.SBit = false;
	p_.plen = -1;
	p_.id = -1;
	p_.time = 0;
	p_.crc = -1;

	memset(p_.data, 0, 1024);

	printf("* Waiting for incoming package...\n");

	while(true)
	{
		n = read(fd, &cbuf, 1);
		if(n == 0 && n2 > 0) break;			// check if the receive was started and stop if there is no more data to process
		if(n > 0) {
			if (hBeg) {						// startbit recieved, ready to recieve new package
				if(p_.plen == -1) {
					switch (len) {
						case 0:			// pkg offset, 1st byte
							{
								tpkg[len] = temp[0] = cbuf;
								len = 1;
							}
							break;
						case 1:			// pkg offset, 2nd byte
							{
								tpkg[len] = temp[1] = cbuf;
								temp[2] = '\0';				// null terminated for strtol(), this elemens is overwriten afterwards
								p_.plen = (int)strtol(temp, NULL, 16);
								printf("received: %i bytes (%.5i total)\n", p_.plen, n2);
							}
							break;
					}
				}else{
					len ++;
					if (len < p_.plen-1)						// store LEN of package bytes, count starts from 1
					{
						if (len < pkgoffset){					// save header and data separately
							tpkg[len] = cbuf;				// save header bytes
						}else{
							p_.data[len - pkgoffset] = cbuf;	// save data bytes
						}
					}
					else
					{
						p_.data[len - pkgoffset] = cbuf;		// push last byte manually (caused by [len < p_.plen-1] state)
						
						pkgcnt ++;

						/* parse pkg */
						p_.dlen = len - pkgoffset + 1;

						sprintf(temp, "%c%c", tpkg[2], tpkg[3]);	// join chars into temp c-string
						p_.id = (int)strtol(temp, NULL, 16);		// cast from hex to int

						sprintf(temp, "%c%c%c%c%c%c%c%c%c", tpkg[4], tpkg[5], tpkg[6], tpkg[7], tpkg[8], tpkg[9], tpkg[10], tpkg[11], tpkg[12]);
						p_.time = (unsigned int)strtol(temp, NULL, 10);

						sprintf(temp, "%c%c%c%c", tpkg[13], tpkg[14], tpkg[15], tpkg[16]);
						p_.crc = (int)strtol(temp, NULL, 10);
#if TEST_LVL == 2
						printf("* Processing package...\n");
						printf("headerlen: %i   datalen: %i\n", pkgoffset, p_.dlen);
						printf("raw header: %s\n", tpkg);
						printf("pID: %i\n", p_.id);
						printf("time: %i\n", p_.time);
						printf("crc: %i\n", p_.crc);
						printf("data: %s\n", p_.data);
						printf("* Preparing to receive anoter package...\n");
#endif
						/* push pkg */
						buf->push_back(p_);

						/* reset and prepare to receive next */
						hBeg = false;
						p_.SBit = false;
						p_.plen = -1;
						len = -1;
						memset(temp, 0, 10);
						memset(tpkg, 0, 1024);
						memset(p_.data, 0, 1024);
					}
				}
			}
			else
			{
				if (cbuf == 0xA)							// startbit?
				{
					if (cbuf == 0xA && p_.SBit == true)		// 2 of 2?
					{
						hBeg = true;					// ok, do it
						len = 0;
					}
					else	p_.SBit = true;					// first of 2? ok, try again next time
				}
				else	p_.SBit = false;					// reset
			}
			cbuf = '\0';
		}
		n2 += n;
	}

	printf("* received: %i packages (bytes: %i)\n", pkgcnt, n2);

	if (n2 <= 0)
	{
		cout << "recieve() failed!\n";
		return -1;
	}

	return n2;
}

// ---------------------------------------------
// Time
// ---------------------------------------------

unsigned int test_data::get_time(void)						// get current timestamp with uSec, return value is 4bytes long 9 digit number, first 3 digits - time, last 6 digits - uSec
{
#if T_TIMER == 1										// get UNIX time and uSec
	struct timeval ts;
	if(gettimeofday(&ts, NULL) != 0) cout << "Error while getting time. " << errno << endl;
	#if TEST_LVL == 1
		cout << "Sec1 : " << ts.tv_sec << "   uSec1: " << ts.tv_usec << "   Time1: " << (unsigned int)(((ts.tv_sec % 1000)*1000000)+ts.tv_usec) << endl;
	#endif
	return (((ts.tv_sec % 1000)*1000000)+ts.tv_usec);
#elif T_TIMER == 2										// System-wide realtime clock. Setting this clock requires appropriate privileges. 
	struct timespec ts;
	if(clock_gettime(CLOCK_REALTIME, &ts) != 0) cout << "Error while getting time from CLOCK_REALTIME. " << errno << endl;
	#if TEST_LVL == 1
		cout << "Sec2 : " << ts.tv_sec << "   uSec2: " << ts.tv_nsec / 1.0e3 << "   Time2: " << (unsigned int)(((ts.tv_sec % 1000)*1000000)+(ts.tv_nsec / 1.0e3)) << endl;
	#endif
	return (((ts.tv_sec % 1000)*1000000)+(ts.tv_nsec / 1.0e3));
#elif T_TIMER == 3										// Clock that cannot be set and represents monotonic time since some unspecified starting point. 
	struct timespec ts;
	if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) cout << "Error while getting time from CLOCK_MONOTONIC. " << errno << endl;
	#if TEST_LVL == 1
		cout << "Sec3 : " << ts.tv_sec << "   uSec3: " << ts.tv_nsec / 1.0e3 << "   Time3: " << (unsigned int)(((ts.tv_sec % 1000)*1000000)+(ts.tv_nsec / 1.0e3)) << endl;
	#endif
	return (((ts.tv_sec % 1000)*1000000)+(ts.tv_nsec / 1.0e3));
#endif
}

int test_data::get_time_diff(unsigned int  t)				// elapsed time
{
#if TEST_LVL == 0
		return get_time() - t;
#elif TEST_LVL == 1
		unsigned int ttmp = get_time();
		cout << "time_diff: " << ttmp << endl;
		return ttmp - t;
#endif
}

// ---------------------------------------------
// =============================================
// ---------------------------------------------

int main (int argc, char *argv[]) {
	char iFname[] = "image.png";
	// char iFname[] = "text.txt";
	char devName1[] = "/dev/pts/1";					// serial device name
	char devName2[] = "/dev/pts/2";

	test_data * t1 = new test_data ();
	file_ * f1 = new file_ (iFname);
	serial_ * s1 = new serial_(devName1);
	serial_ * s2 = new serial_(devName2);
    
#define MODE_ 1	// 0 - send, 1 - receive

#if MODE_ == 0
	file_ * f1 = new file_ (iFname);
	serial_ * s1 = new serial_(devName1);
	test_data * t1 = new test_data ();
	s1->send(f1->buffer, f1->fLen, 50, t1);

#elif MODE_ == 1

	std::vector<pack_> rbuf;
	rbuf.reserve(1024);
	serial_ * s2 = new serial_(devName2);
	test_data * t1 = new test_data ();
	if(s2->receive_and_parse(&rbuf) == -1) return -1;
	printf("\x1b[31;1m DONE \x1b[0m\n");

	char tfile[] = "output";
	if( remove( tfile ) != 0 ) printf("Error deleting file\n");	// delete if exists

	FILE * pFile = NULL;
	pFile = fopen (tfile, "ab");						// append
	int total_b = 0;

	for (std::vector<pack_>::iterator it = rbuf.begin() ; it != rbuf.end(); it++)
	{
#if TEST_LVL == 2
		printf("id: %.2i   time: %u   timediff: %.9u   crc: %i   _dlen:%i\ndata: %s\n", it->id, it->time, t1->get_time_diff(it->time), it->crc, it->dlen, it->data);
#else
		printf("id: %.2i   time: %u   timediff: %.9u   crc: %i   _dlen:%i\n", it->id, it->time, t1->get_time_diff(it->time), it->crc, it->dlen);
#endif
		total_b += fwrite (it->data, it->dlen, 1, pFile);		// write to file
	}
	fclose (pFile);
	printf("Written: %i bytes.\n", total_b);
#endif

	return 0;
}
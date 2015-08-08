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

#define T_TIMER 1
#define TEST_LVL 0

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

class packager
{
public:
	file_ * pFile;
};

class serial_
{
public:
	int fd;										// File descriptor for the port
	int read_port_flag;
	// ------------
	serial_();
	~serial_();										// close the serial port
	serial_(char*, int, int, int);
	//void set_blocking(int);
	int send(char*, int);
	int receive(char *, unsigned int);
	int receive(vector<char> *);
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

serial_::serial_(char *devName, int spd = B9600, int par = 0, int block = 0)	// The values for speed are B115200, B230400, B9600, B19200, B38400, B57600, B1200, B2400, B4800, etc. The values for parity are 0 (meaning no parity), PARENB|PARODD (enable parity and use odd), PARENB (enable parity and use even), PARENB|PARODD|CMSPAR (mark parity), and PARENB|CMSPAR (space parity).
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
	Clearing a bit		AND		&= ~	number &= ~(1 << x)
	Toggling a bit		XOR		^= 		number ^= 1 << x
	Checking a bit 						bit = (number >> x) & 1
	Changing nth bit to x				number ^= (-x ^ number) & (1 << n);
*/
		tty_s.c_cflag |= (CLOCAL | CREAD);			// enable the receiver and set local mode.		CLOCAL - Ignore modem control lines.
		
		tty_s.c_cflag &= ~PARENB;					// no parity.		PARENB - Enable parity generation on output and parity checking for input.
		tty_s.c_cflag |= par;							// set parity if specified
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

int serial_::receive(vector<char> * buf)
{
	int n = 0;
	char cbuf;
/*
	while(read(fd, cbuf, 1))
	{
		buf.push_back(cbuf);
		//cbuf = '';
	}
	
	if (n <= 0)
	{
		cout << "recieve() failed!\n";
		return -1;
	}
*/
	return n;
}

// ---------------------------------------------
// Time
// ---------------------------------------------

unsigned int test_data::get_time(void)					// get current timestamp with uSec, return value is 4bytes long 9 digit number, first 6 digits - time, last 3 digits - uSec
{
#if T_TIMER == 1									// get UNIX time and uSec
	struct timeval ts;
	if(gettimeofday(&ts, NULL) != 0) cout << "Error while getting time. " << errno << endl;
	#if TEST_LVL == 1
		cout << "Sec1 : " << ts.tv_sec << "   uSec1: " << ts.tv_usec/ 1.0e3 << endl;
	#endif
	return (((ts.tv_sec % 1000000)*1000)+(ts.tv_usec / 1.0e3));
#elif T_TIMER == 2									// System-wide realtime clock. Setting this clock requires appropriate privileges. 
	struct timespec ts;								// theprogram needs to be compiled with -lrt flag to Link the Real Time shared library
	if(clock_gettime(CLOCK_REALTIME, &ts) != 0) cout << "Error while getting time from CLOCK_REALTIME. " << errno << endl;
	#if TEST_LVL == 1
		cout << "Sec2 : " << ts.tv_sec << "   nSec2: " << ts.tv_nsec / 1.0e6 << endl;
	#endif
	return (((ts.tv_sec % 1000000)*1000)+(ts.tv_nsec / 1.0e6));
#elif T_TIMER == 3									// Clock that cannot be set and represents monotonic time since some unspecified starting point. 
	struct timespec ts;
	if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) cout << "Error while getting time from CLOCK_MONOTONIC. " << errno << endl;
	#if TEST_LVL == 1
		cout << "Sec3 : " << ts.tv_sec << "   uSec3: " << ts.tv_nsec / 1.0e6 << endl;
	#endif
	return (unsigned int)(((ts.tv_sec % 1000000)*1000)+(ts.tv_nsec / 1.0e6));
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

int main (int argc, char *argv[]){
	char iFname[] = "image.png";
	//char iFname[] = "text.txt";
	char devName1[] = "/dev/pts/1";					// serial device name
	char devName2[] = "/dev/pts/2";

	test_data * t1 = new test_data ();
	file_ * f1 = new file_ (iFname);
	serial_ * s1 = new serial_(devName1);
	serial_ * s2 = new serial_(devName2);
    
	std::vector<char> rbuf;
	rbuf.reserve(1024);

/*
	unsigned int ts1 = t1->get_time();
	usleep(123000);								// int usleep(useconds_t usec);	sleep for 0.123sec
	int ts2 = t1->get_time_diff(ts1);

	cout << "Seconds: " << ts1 << "\nDiff: " << ts2 << endl;

	cout << "Printed: " << f1->printHEX(0, 16) << " bytes\n";

	cout << "Serial handler: " << s1->fd << endl;
	cout << "Serial handler: " << s2->fd << endl;
*/

	//cout << "Send: " << s1->send(f1->buffer, f1->fLen) << " bytes\n";
	//cout << "Send: " << s1->send(f1->buffer, 32) << " bytes\n";

	for (int i = 0; i < 10; ++i)
	{
		unsigned int time_ = t1->get_time();
		//char * time_string[(((sizeof time_) * 8) + 2)/3 + 2];
		char time_string[50];
		int ns = sprintf(time_string, "\n\n\n\nTime:%u\n", time_);	// converts int to char and formats the string, adds header signature 0x0A ( \n x4 ).
														// On success, the total number of characters written is returned. This count DOES NOT INCLUDE the additional null-character automatically appended at the end of the string. On failure, a negative number is returned.
		cout << "Time: " << time_ << endl;
		cout << "Send: " << s1->send(time_string, ns) << " bytes\n";
		usleep(1000000);
	}

/*
	char buf [1024];
	while (1)
	{
		cout << "Received1: " << s2->receive(buf, 1024) << " bytes.\nBuf: " << buf << endl;
		cout << "Received: " << s2->receive(rbuf) << " bytes.\nBuf: " << buf << endl;
		memset(buf, 0, 1024);
		usleep(1000000);
	}
*/
	return 0;
}
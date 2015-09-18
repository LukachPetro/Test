#include<iostream>
#include<fstream>
#include<stdio.h>
#include<string>
#include<iomanip>
#include<windows.h>
#include <time.h>
#include <ctype.h>
#include <conio.h>
using namespace std;

#define PING_FRAME 1
#define FRAME_SIZE 256
#define HEADER_SIZE 31						
#define START_BYTE 0xAA
#define END_BYTE 0xCC
#define TERMINATOR 0x00
#define ITOA_SIZE 10
#define PCKT_SZE_OFFSET 3
#define PCKT_TIME_OFFSET 13
#define PCKT_CRC_OFFSET 23
#define CRC_SIZE 4
#define SPACE 0x20
#define REP 100
#define ATT REP/2
#define CAP_MIN 100
#define TIMEOUT 10

#define T_TIMER 1
#define TEST_LVL 0


#define ECHO 0x00
#define DRATERQ 0x04	// data rate request
#define DRATERS 0x05	// data rate response
#define TESTR	0x11	// test routine ID
#define LTNCYRQ	0x08	// latency request
#define LTNCYRS	0x09	// latency response
#define DTNCERQ 0x0A	// distance request
#define DTNCERS 0x10	// distance respoce

struct pkt_header{
			char pkt_ID;
			unsigned int pkt_size;
			unsigned int time;
			char load_crc[4];		
};

void Packer(char*, pkt_header, char*);
void Get_Header (pkt_header&,char*);
void DoSmth (char*, int);


class serial_
{
public:
	HANDLE hSerial;					// port handle
	DCB DCBSerialParams;			// port params structure
	DWORD dwSizeOfBytes;			// port op variable 
	
	char buff[FRAME_SIZE];

	serial_(string, int, int);
	~serial_();	

	void SendFrame(pkt_header, char *);
	void SendFrame(pkt_header, int, char*);
	int ReadFrame();
	int Readit(int );
};

serial_::serial_(string pName, int bdRate=9600, int sParity=0){
	for(int i = 0; i < FRAME_SIZE;i++) buff[i]=NULL;
	string prefix = "\\\\.\\";
	pName = prefix+pName;
	LPCSTR name;
	name = pName.c_str();
	hSerial = CreateFileA(name,GENERIC_READ | GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
	if(hSerial==INVALID_HANDLE_VALUE)
	{
		if(GetLastError()==ERROR_FILE_NOT_FOUND)
		{
			cout << "serial port does not exist.\n";
		}
		cout << "some other error occurred.\n";
	}
	else 
		cout << "connected\n";

	DCB dcbSerialParams = {0};
	dcbSerialParams.DCBlength=sizeof(dcbSerialParams);
	if (!GetCommState(hSerial, &dcbSerialParams))
	{
		cout << "getting state error\n";
	}
	dcbSerialParams.BaudRate=bdRate;
	dcbSerialParams.ByteSize=8;
	dcbSerialParams.StopBits=ONESTOPBIT;
	dcbSerialParams.Parity=sParity;
	if(!SetCommState(hSerial, &dcbSerialParams))
	{
		cout << "error setting serial port state\n";
	}
	//
	COMMTIMEOUTS timeout={0};
	timeout.ReadIntervalTimeout = 50;
	timeout.ReadTotalTimeoutConstant = 150;
	//timeout.ReadTotalTimeoutMultiplier = 50;
	//timeout.WriteTotalTimeoutConstant = 50;
	//timeout.WriteTotalTimeoutMultiplier = 10;
	if(!SetCommTimeouts(hSerial,&timeout))
	{
		cout << "error setting serial port timeouts\n";
	}
}
serial_::~serial_(){
	CloseHandle(hSerial);
}

void serial_::SendFrame(pkt_header my_header, char* sendObject){
	try {
			//pkt_header my_header;															// creating frame header
			my_header.pkt_size = 0;
			while (sendObject[my_header.pkt_size] != TERMINATOR) my_header.pkt_size++;		// counting bytes in message to send
			Packer(buff,my_header,sendObject);												// adding header
			WriteFile (hSerial,&buff,FRAME_SIZE, &dwSizeOfBytes ,NULL);						// COM transaction
		}
		catch (ios_base::failure& e) {
			cout << "error";
			cerr << e.what() << '\n';
			}
}
void serial_::SendFrame(pkt_header my_header,int nsize, char* sendObject)
{
	try {
			//pkt_header my_header;															// creating frame header
			my_header.pkt_size = 0;
			char* frame = new char[nsize];
			while (sendObject[my_header.pkt_size] != TERMINATOR) my_header.pkt_size++;		// counting bytes in message to send
			Packer(frame,my_header,sendObject);												// adding header
			WriteFile (hSerial,&frame,nsize, &dwSizeOfBytes ,NULL);						// COM transaction
		}
		catch (ios_base::failure& e) {
			cout << "error";
			cerr << e.what() << '\n';
			}
}
int serial_::ReadFrame(){
	try {
			ReadFile(hSerial, buff, FRAME_SIZE, &dwSizeOfBytes, 0);						// COM port transaction
			return (int)dwSizeOfBytes;
	}
	catch (ios_base::failure& e) {
		cout << "error";
		cerr << e.what() << '\n';
		return -1;
	}

};
int serial_::Readit(int readsize)
{
	try {
		char* readarr = new char[readsize];
		ReadFile(hSerial, readarr, readsize, &dwSizeOfBytes, 0);						// COM port transaction
		return (int)dwSizeOfBytes;
	}
	catch (ios_base::failure& e) {
		cout << "error";
		cerr << e.what() << '\n';
		return -1;
	}
};
unsigned int get_time();


int main()
{
	unsigned int currTime;
	string pName;
	int bdRate;
	cout << "Enter: port name to open\n";
	cin >> pName;
	serial_ oport(pName);
	cout << "Press space to start testing\n";
	while (true)
	{
		while(!kbhit())
		{									// here starts server request processing
			if (oport.ReadFrame() > 0)
			{
				pkt_header header_r = {NULL};
				Get_Header(header_r,oport.buff);
				if(header_r.pkt_ID == ECHO)										
				{
					oport.SendFrame(header_r,&oport.buff[HEADER_SIZE]);
					for(int i = 0; i < REP-1; i++)
					{
						while(oport.ReadFrame() == 0);
						oport.SendFrame(header_r,&oport.buff[HEADER_SIZE]);
					}

				}
				if(header_r.pkt_ID == DRATERQ)									// case of data rate request
				{
					header_r.pkt_ID = DRATERS;
					for (int i = 0; i < 4; i ++)
					{
						oport.SendFrame(header_r,"RATE");
					}
				}
				if (header_r.pkt_ID == LTNCYRQ)									// case of latency request
				{
					header_r.pkt_ID = LTNCYRS;
					for (int i = 0; i < REP;i ++)
						oport.SendFrame(header_r,"LATENCY");
				}

				if(header_r.pkt_ID == DTNCERQ)									// case of distance request
				{
					char ndex[ITOA_SIZE];
					header_r.pkt_ID = DTNCERS;
					while (true)
					{
						oport.SendFrame(header_r,"RSP");
						Sleep(REP);
					}

				}
			}
		}
		// initializator
		if (getch() == SPACE)
		{
			pkt_header header_s;
			header_s.pkt_ID = ECHO;
			int arr_p[REP], max_p = 0, min_p = REP, p_av = 0;		// array with ping values, max min and average ping values
			// ping testing
			for(int i = 0; i < REP;i++)
			{
				oport.SendFrame(header_s,"Hello world");
				header_s.time = get_time();
				while(true)
				{
					if (oport.ReadFrame() > 0)
					{
						currTime = get_time();
						arr_p[i] = currTime- header_s.time;
						p_av += arr_p[i];
						if (arr_p[i]>max_p)
							max_p = arr_p[i];
						if (arr_p[i]<min_p)
							min_p = arr_p[i];
						break;
					}
				}
			}	
			cout << "\nPING IS: " << p_av/REP << "ms";
			cout << "\nMax: " << max_p << "ms";
			cout << "\nMin: " << min_p << "ms" << endl; 

			// DATA RATE testing
			float TF = 0, TL = 0, dRate = 0;
			header_s.pkt_ID = DRATERQ;
			oport.SendFrame(header_s,"DATA RATE");
			
			for(int i = 0; i < 4; i++)
			{
				oport.ReadFrame();
				if (i == 0)
					TF = get_time();
				if (i == 3)
					TL = get_time();
			}
			dRate = FRAME_SIZE*3*8*1000/(TL-TF);
			cout << "\nDATA RATE IS: " << dRate << "bit/s || " << dRate/(1024.0*1024.0) << "Mbit/s\n";
			
			
			// Latency testing
			int tarr[REP] = {0}, Tarr[REP] = {0}, LaT[REP] = {0};
			int lt_max = 0, lt_min=REP, lt=0;
			header_s.pkt_ID = LTNCYRQ;
			oport.SendFrame(header_s,"LATENCY");
			for (int i = 0; i < REP; i++)
			{
				oport.ReadFrame();
				Tarr[i] = get_time();
				Get_Header(header_s,oport.buff);
				tarr[i] = header_s.time;
			}
			for (int i = 0; i < REP-1; i++)
			{
				LaT[i] = Tarr[i+1] - Tarr[i] - ( tarr[i+1] - tarr[i]);
				lt+=LaT[i];
				if(LaT[i]<lt_min)
					lt_min=LaT[i];
				if(LaT[i]>lt_max)
					lt_max=LaT[i];
			}
			cout << "\nLATENCY IS: " << lt/REP << "ms\n";
			cout << "Max: " << lt_max << "ms\n";
			cout << "Min: " << lt_min << "ms\n";


			// distance testing
			cout << "\nFor range testing press R:\n";
			if (getch()=='r')
			{
				int timeout;
				unsigned int previous = get_time();
				header_s.pkt_ID = DTNCERQ;
				oport.SendFrame(header_s,"DIST");
				for (int i = 0; ;i++)
				{
					cout << "\npacket #: " << i << endl;
					if(oport.ReadFrame()== 0)
					{
						timeout++;
						if (timeout > TIMEOUT)
						{
							cout << "\nLOST CONNECTION\n";
							break;
						}
					}
					else
					{
						system("cls");
						Get_Header(header_s,oport.buff);
						cout << "; delay is: " << get_time() - previous << "ms\n";
						previous = get_time();
					}
				}
			cout << "\nSTOP RANGE LIMIT\n";
			}
			cout << "\nTo start testing press \"SPACE\"\n";
		}
	}
	return 0;
}
void Packer(char *byte_arr,pkt_header header, char* datap)
{
	byte_arr[0] = START_BYTE;
	byte_arr[1] = START_BYTE;
	byte_arr[2] = header.pkt_ID;
	SYSTEMTIME mtime;
	
	int i;
	char buff[ITOA_SIZE];						// buffer for itoa out
	// packing info size value (uint)  
	itoa(header.pkt_size,buff,10);
	for( i = 0; i < ITOA_SIZE; i++)
		byte_arr[i+PCKT_SZE_OFFSET]=buff[i];

	// packing time value (uint)
	if(header.time == NULL)
	{
		header.time = get_time();
		itoa(header.time,buff,10);
		for(i = 0; i < ITOA_SIZE; i++)
			byte_arr[i+PCKT_TIME_OFFSET]=buff[i];
	}

	// packing frame crc (uint)
	for ( i = 0; i < CRC_SIZE; i ++)
		byte_arr [i+PCKT_CRC_OFFSET] = header.load_crc[i];
	
	// packing data into frame
	for ( i = 0; i < (FRAME_SIZE - HEADER_SIZE) && datap[i]!= 0x00; i ++)
		byte_arr [i+HEADER_SIZE] = datap[i];
}
void Get_Header (pkt_header &my_header,char* msg_bgn)
{
	//crc processing //
	my_header.pkt_ID = *(msg_bgn+2);
	my_header.pkt_size = atoi(msg_bgn+PCKT_SZE_OFFSET);
	//cout << "DELAY: " << get_time(&timestrp) - (unsigned)atoi(msg_bgn+PCKT_TIME_OFFSET)<< "ms" << endl; 
	my_header.time = (unsigned)atoi(msg_bgn+PCKT_TIME_OFFSET);
}
void DoSmth (char* readed, int size)
{
	cout << "RECEIVED " << size << " BYTES\n";
	cout << "Received message is: \"";
	for(int i = 0 ; i < size;i++)
		cout << readed[HEADER_SIZE+i];
	cout << "\"";
}
unsigned int get_time()
{
	SYSTEMTIME timestrp;
	GetSystemTime(&timestrp);
	return (unsigned int)(timestrp.wMinute*10e4 + timestrp.wSecond*10e2 + timestrp.wMilliseconds);
}

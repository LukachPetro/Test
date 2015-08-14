#include<iostream>
#include<fstream>
#include<stdio.h>
#include<string>
#include<iomanip>
#include<windows.h>
using namespace std;

#define FRAME_SIZE 1024
#define HEADER_SIZE 63							// not true  will correct
#define START_BYTE 0xAA
#define END_BYTE 0xCC
#define TERMINATOR 0x00


struct pkt_header{
			char pkt_ID;
			unsigned int pkt_size;
			char load_crc[4];
			char hdr_crc;
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

	serial_(LPCSTR, int, int);
	~serial_();	

	void SendFrame(char *);
	void ReadFrame();
};

serial_::serial_(LPCSTR sPortName, int bdRate=9600, int sParity=0){
	for(int i = 0; i < FRAME_SIZE;i++) buff[i]=NULL;
	hSerial = CreateFileA(sPortName,GENERIC_READ | GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
	if(hSerial==INVALID_HANDLE_VALUE)
	{
		if(GetLastError()==ERROR_FILE_NOT_FOUND)
		{
			cout << "serial port does not exist.\n";
		}
		cout << "some other error occurred.\n";
	}

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

}
serial_::~serial_(){
	CloseHandle(hSerial);
}
void serial_::SendFrame(char* sendObject){
	try {
			pkt_header my_header;															// creating frame header
			my_header.pkt_size = 0;
			while (sendObject[my_header.pkt_size] != TERMINATOR) my_header.pkt_size++;		// counting bytes in message to send
			Packer(buff,my_header,sendObject);												// adding header
			WriteFile (hSerial,&buff,FRAME_SIZE,&dwSizeOfBytes ,NULL);						// COM transaction
		}
		catch (ios_base::failure& e) {
			cout << "error";
			cerr << e.what() << '\n';
			}




	/*char data[] = "Test";
	DWORD dwSize = sizeof(data);
	WriteFile (hSerial,data,dwSize,&dwBytesWritten ,NULL);*/
}
void serial_::ReadFrame(){
	try {
			ReadFile(hSerial, buff, FRAME_SIZE, &dwSizeOfBytes, 0);						// COM port transaction
	}
	catch (ios_base::failure& e) {
		cout << "error";
		cerr << e.what() << '\n';
	}

};

int main()
{
	string pName;
	string prefix = "\\\\.\\";
	int bdRate;
	cout << "Enter: port name to open\n";
	cin >> pName;
	pName = prefix+pName;
	LPCSTR name;
	name = pName.c_str();
	serial_ oport(name);
	int m;
	cout << "Enter mode\nMode 1 is used to send message (initializator) other number receiver mode\n";
	cout << "Tip: first start receiver then initializator\n";
	cin >>m;
	if(m==1)
		oport.SendFrame("Hello world");
	else
	{
		oport.ReadFrame();					// reading frame

		/*				readed frame processing			*/
		pkt_header my_header;
		for (int i = 0; i < FRAME_SIZE;i++)
			{
				if (oport.buff[i] == (char)START_BYTE && oport.buff[i+1] == (char)START_BYTE)
				{
					Get_Header(my_header, &oport.buff[i]);
					switch (my_header.pkt_ID)
					{
						case (char)END_BYTE:
						{
							DoSmth(oport.buff,my_header.pkt_size);
						}break;
					}

				}
			}
		/*				readed frame processing			*/

	}
	cin.get();
	cin.get();
	return 0;
}
void Packer(char *byte_arr,pkt_header header, char* datap)
{
	byte_arr[0] = START_BYTE;
	byte_arr[1] = START_BYTE;
	byte_arr[2] = header.pkt_ID;

	char buff[20];
	itoa(header.pkt_size,buff,10);

	for(int i = 0; i < 20; i++)
		byte_arr[i+3]=buff[i];
	for (int i = 0; i < 32; i ++)
		byte_arr [i+23] = header.load_crc[i];
	for (int i = 0; i < 8; i ++)
		byte_arr [i+55] = header.load_crc[i];
	for (int i = 0; i < (FRAME_SIZE - HEADER_SIZE) && datap[i]!= 0x00; i ++)
		byte_arr [i+HEADER_SIZE] = datap[i];
}
void Get_Header (pkt_header &my_header,char* msg_bgn)
{
	my_header.pkt_ID = *(msg_bgn+2);
	my_header.pkt_size = atoi(msg_bgn+3);

	for (int i = 0; i < 4; i ++)
		my_header.load_crc[i]=*(msg_bgn+23+i);
		my_header.hdr_crc = *(msg_bgn+28);
}
void DoSmth (char* readed, int size)
{
	cout << "Received message is: \"";
	for(int i = 0 ; i < size;i++)
		cout << readed[HEADER_SIZE+i];
	cout << "\"";
}

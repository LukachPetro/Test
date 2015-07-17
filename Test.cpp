#include "stdafx.h"
#include<iostream>
#include<fstream>
#include<stdio.h>
using namespace std;


struct pkt_header{
			char pkt_start[2];
			char pkt_ID;
			unsigned int pkt_size;
			char load_crc[32];
			char hdr_crc[8];
};

char* Packer(char*, pkt_header, char*);
pkt_header Get_Header (char*);
char* DoSmth (char*, char*, int);


int main()
{
	char iFname[] = "in.jpg";
	char oFname[] = "out.bin";
	char nFname[] = "ready.jpg";

	try {
		ifstream iFile(iFname, ios::in | ios::binary);
		ofstream oFile(oFname, ios::out | ios::binary | ios::trunc);

		if (iFile){	//check if file exists
			iFile.seekg(0, iFile.end);	//move to end position, file size
			streampos size = iFile.tellg();	//Returns the position of the current character in the input stream.
			iFile.seekg(0, iFile.beg);	//move to beginning of the file

			cout << "File Size: " << size << endl;

			char * buffer = new char[size];	//create a buffer for entire file size

			iFile.read(buffer, size);	//read SIZE of bytes frome file to BUFFER
			iFile.close();


			/* Do stuff here */
			pkt_header my_header;
			
			my_header.pkt_start[0] = 0xAA;
			my_header.pkt_start[1] = 0xAA;

			int packet_bytes = 0;
			char *start_byte = &buffer[0]; 
			char *frame;
			while (start_byte < &buffer[size])
			{
				if (&buffer[size] - start_byte <= 1024)
				{
					my_header.pkt_size = &buffer[size] - start_byte;
					frame = new char[my_header.pkt_size + 63];
					frame = Packer(frame,my_header, start_byte);
					//writefile routine
					oFile.write(frame, (my_header.pkt_size+63) * sizeof(char));
					//writefile routine
					
					start_byte+=my_header.pkt_size;
				}
				else
				{
					my_header.pkt_size=1024;
					frame = new char[1024 + 63];
					frame = Packer(frame,my_header, start_byte);
					//write file routine
					oFile.write(frame, (my_header.pkt_size+63) * sizeof(char));
				
					// write file routine
					start_byte+=1024;
				}
				delete[] frame;
			}

			oFile.close();
		}
		else{
			cout << "No such file\n";
		}
	}
	catch (ios_base::failure& e) {
		cout << "error";
		cerr << e.what() << '\n';
	}


	//reader
		try {
		ifstream iFile(oFname, ios::in | ios::binary);
		ofstream oFile(nFname, ios::out | ios::binary | ios::trunc);

		if (iFile){	//check if file exists
			iFile.seekg(0, iFile.end);	//move to end position, file size
			streampos size = iFile.tellg();	//Returns the position of the current character in the input stream.
			iFile.seekg(0, iFile.beg);	//move to beginning of the file

			cout << "File Size: " << size << endl;

			char * buffer = new char[size];	//create a buffer for entire file size

			iFile.read(buffer, size);	//read SIZE of bytes frome file to BUFFER
			iFile.close();


			/* Do stuff here */
			pkt_header my_header;
			char *frame;
			for (int i = 0; i < size;i++)
			{
				if (buffer[i] == (char)0xAA && buffer[i+1] == (char)0xAA)
				{
					//run
					my_header = Get_Header(&buffer[i]);
					switch (my_header.pkt_ID)
					{
					case (char)0xCC:
						{
							frame = new char[my_header.pkt_size];
							frame = DoSmth(frame,(&buffer[i]+63),my_header.pkt_size);
							oFile.write(frame, (my_header.pkt_size) * sizeof(char));
							delete[] frame;
						}break;
					}

				}
			}

			oFile.close();
		}
		else{
			cout << "No such file\n";
		}
	}
	catch (ios_base::failure& e) {
		cout << "error";
		cerr << e.what() << '\n';
	}




	cin.get();
	return 0;
}
char *Packer(char* byte_arr,pkt_header header, char* datap)
{
	byte_arr[0] = header.pkt_start[0];
	byte_arr[1] = header.pkt_start[1];
	byte_arr[2] = header.pkt_ID;

	char buff[20];
	itoa(header.pkt_size,buff,10);

	for(int i = 0; i < 20; i++)
		byte_arr[i+3]=buff[i];
	for (int i = 0; i < 32; i ++)
		byte_arr [i+23] = header.load_crc[i];
	for (int i = 0; i < 8; i ++)
		byte_arr [i+55] = header.load_crc[i];
	for (int i = 0; i < header.pkt_size; i ++)
		byte_arr [i+63] = datap[i];
	return byte_arr;
}
pkt_header Get_Header (char* msg_bgn)
{
	pkt_header my_header;
	my_header.pkt_start[0] = *msg_bgn;
	my_header.pkt_start[1] = *(msg_bgn+1);
	my_header.pkt_ID = *(msg_bgn+2);
	my_header.pkt_size = atoi(msg_bgn+3);

	for (int i = 0; i < 32; i ++)
		my_header.load_crc[i]=*(msg_bgn+23+i);
	for (int i = 0; i < 8; i ++)
		my_header.hdr_crc[i] = *(msg_bgn+55+i);
	return my_header;
}
char* DoSmth (char* target, char* strt_pos, int size)
{
	for(int i = 0;i<size;i++)
		target[i]=*(strt_pos+i);
	return target;
}
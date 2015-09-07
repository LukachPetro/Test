// needs to be compiled with -lbluetooth to link the bluetooth library
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>	
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>


struct bt_dev_list
{
	char addr[18];	// was 19?
	char name[255];
	bdaddr_t bdaddr;			//  bdaddr_t is an array of 6 unsigned char elements
};


// 	http://mapidev.blogspot.com/2009/11/bluetooth-device-discovery-in-c.html
class bluetooth_
{
	int max_rsp;										// max devices to find
	int dev_id;
	int sock;
	int dur;											// scan duration 10.24 sec. Performs INQUIRY during (1.28 * Inquiry_Length) sec.
	int flags;											// don't use previous inquiry data
	int i;
	char addr[19];										// this is a middle to find out friendly name
	char name[248];										// this is to store friendly name
public:
	inquiry_info *iInfo;								// This struct will store the addresses of found devices
	int num_rsp;										// devices found
	// ------------
	bluetooth_(bdaddr_t *);
	~bluetooth_() { close(sock); }
	int scan(bt_dev_list*, int);
	int bt_scan_service();
	int connect();
	int disconenct();
};


// ---------------------------------------------
// Bluetooth
// ---------------------------------------------
bluetooth_::bluetooth_(bdaddr_t * id = NULL)
{
// OPEN A LOCAL BLUETOOTH ADAPTER
	dev_id = hci_get_route(id);							// Return the device id of the given bdaddr_t. Passing NULL will retrieve the resource number of the first available Bluetooth adapter
	// dev_id = hci_devid( "01:23:45:67:89:AB" );		// OR get ID by MAC string representation of the choosen adapter
	sock = hci_open_dev(dev_id);						// hci_open_dev opens a Bluetooth socket with the specified resource number. To be clear, the socket opened by hci_open_dev represents a connection to the microcontroller on the specified local Bluetooth adapter, and not a connection to a remote Bluetooth device. Performing low level Bluetooth operations involves sending commands directly to the microcontroller with this socket.
	if (dev_id < 0 || sock < 0)
	{
		cout << "BT: error while opening device or socket. id:" << dev_id << " sock: " << sock << endl;
		return;
	}
}

int bluetooth_::scan(bt_dev_list * arr, int l=5)
{
	max_rsp = 255;										// max of 255 devices to discover
	dur = l;											// 8 = 10.24 sec lasts. Performs INQUIRY during (1.28 * Inquiry_Length) sec.
	flags = IREQ_CACHE_FLUSH;							// don't use previous inquiry data
// INQURY PROCESS
	cout << "Scanning for available BT devices..." << endl;
	iInfo = new inquiry_info [max_rsp];
	num_rsp = hci_inquiry(dev_id, dur, max_rsp, NULL, &iInfo, flags);
	if( num_rsp < 0 )
	{
		cout << "BT: hci_inquiry error" << endl;
		return -1;
	}
	cout << "Devices found: " << num_rsp << endl;
// FRIENDLY NAME
	for (i = 0; i < num_rsp; i++) {
		arr[i].bdaddr = iInfo[i].bdaddr;
// int hci_read_remote_name(int sock, const bdaddr_t *ba, int len, char *name, int timeout)
// tries for at most TIMEOUT milliseconds to use the socket SOCK to query the user-friendly name of the device BA. On success, hci_read_remote_name returns 0 and copies at most the first LEN bytes of the device's user-friendly name into NAME. On failure, it returns -1 and sets errno accordingly. 
		if (hci_read_remote_name(sock, &(iInfo[i].bdaddr), sizeof(arr[i].name), arr[i].name, 0) == -1)	// get name based on address.
		{
			strcpy(arr[i].name, "[unknown]");
			cout << "hci_read_remote_name error: " << errno << endl;
		}else{
			ba2str(&(iInfo[i].bdaddr), arr[i].addr);	// convert bdaddr_t to a c-string. First argument is a constant pointer to bdaddr_t address. Second argument is a char string which will contains the returned address. Return value - lenght
		}
		//printf("name: %s   bdaddr: %s   pscan_rep_mode: %i   pscan_period_mode: %i   pscan_mode: %i   dev_class: %i %i %i   clock_offset: %i\n", arr[i].name, arr[i].addr, iInfo[i].pscan_rep_mode, iInfo[i].pscan_period_mode, iInfo[i].pscan_mode, iInfo[i].dev_class[0], iInfo[i].dev_class[1], iInfo[i].dev_class[2], iInfo[i].clock_offset);

		// memset(name, 0, sizeof(name));					// clear the name variable
		// if (hci_read_remote_name(sock, &(iInfo[i].bdaddr), sizeof(name), name, 0) < 0)	// get name based on address
		// 	strcpy(name, "[unknown]");
	}
	//delete[] iInfo;
	return num_rsp;
}


// ==========================================
//
// ------------------------------------------
//
// ==========================================


int bluetooth_::bt_scan_service()
{
	//inquiry_info *ii = NULL;
	int max_rsp = 255;				// max of 255 devices to discover
	int num_rsp;
	int dev_id, sock;
	int len = 8;					// 10.24 sec lasts. Performs INQUIRY during (1.28 * Inquiry_Length) sec.
	int flags = IREQ_CACHE_FLUSH;   // don't use previous inquiry data
	bdaddr_t my_bdaddr_any = { {0, 0, 0, 0, 0, 0} };	// instead of #define BDADDR_ANY  (&(bdaddr_t) {{0, 0, 0, 0, 0, 0}}) in bluetooth.h which causes "error: taking address of temporary [-fpermissive]" in C++
	int i, n;
	char bdaddr[19] = { 0 };
	char name[248] = { 0 };
// ==========================================
	uuid_t svc_uuid;
	int err;
	//bdaddr_t target;
	sdp_list_t *response_list = NULL, *search_list, *attrid_list;
	sdp_session_t *session = 0;
	uint32_t class_id = 0;
	//int num;
//
	//ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
	inquiry_info * ii = new inquiry_info [max_rsp];
// hard code your remote bdaddr here
	char serv_class_ID[]="0x1105";			// service identifier
	//char bdaddr[]="00:1A:7D:DA:71:0A";		// remote bdaddress



// ==========================================
// ==========================================

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev( dev_id );
	if (dev_id < 0 || sock < 0)
	{
		perror("E: can't open socket");
		return -1;
	}

	num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
	if (num_rsp == 0)
	{
		printf("no devices found\n");
		return -1;
	}

	if( num_rsp < 0 ) perror("hci_inquiry");

	for (i = 0; i < num_rsp; i++)
	{
		memset(name, 0, sizeof(name));
		if (hci_read_remote_name(sock, &ii[i].bdaddr, sizeof(name), name, 0) < 0)
			strcpy(name, "[unknown]");
		ba2str(&ii[i].bdaddr, bdaddr);
		printf("%i. %s  %s\n", i, name, bdaddr);
	}


	close( sock );
// cin.getline(buf,100);		// cin.getline is for C strings (character arrays)
// getline(cin,input);			// getline is for C++ strings

// scanf("%d", &number);		// allows to accept input from a keyboard
// char ch = getchar();

	printf("Choose a device to scan: ");
	scanf("%d", &n);

	if (n > max_rsp || n < 0)
	{
		printf("E: variable out of scope\n");
		return -1;
	}

	ba2str(&ii[n].bdaddr, bdaddr);				// now bdaddr stores the name of selected BT device

	// printf("Choose a service id to search for: 0x");
	
	// scanf("%c", serv_class_ID);
	// printf("Service: %s\n", serv_class_ID);

	printf("Scanning MAC: %s...\n", bdaddr);

	// free( ii );
	// delete[] ii;


// ==========================================
// ==========================================


	//str2ba( bdaddr, &target );

	if (!strncasecmp(serv_class_ID, "0x", 2))
	{
		// This is a UUID16, just convert to int 
		//int num;
		sscanf(serv_class_ID + 2, "%X", &class_id);//&num);		// Reads data from serv_class_ID and stores them according to parameter format into the locations given by the additional arguments.
		//class_id = num;
		printf("Service Class: 0x%X\n", class_id);
	}

	if (class_id)
	{
		if (class_id & 0xffff0000)
		{
			sdp_uuid32_create(&svc_uuid, class_id);
			printf("Creating a 32 bit uuid: %i\n", class_id);
		} else {
			uint16_t class16 = class_id & 0xffff;
			sdp_uuid16_create(&svc_uuid, class_id & 0xffff);
			printf("Creating a 16 bit uuid: %i\n", class16);
		}
	}
	else
		printf("E: no device class specified\n");

	// connect to the SDP server running on the remote machine
	// session = sdp_connect( BDADDR_ANY, &ii[i].bdaddr, SDP_RETRY_IF_BUSY );
	session = sdp_connect( &my_bdaddr_any, &ii[n].bdaddr, SDP_RETRY_IF_BUSY );

	if (session == (sdp_session_t *)NULL)
	{
		printf("E: No device or matching services found.\n");
		return -1;
	}
	// specify the UUID of the application we're searching for

	search_list = sdp_list_append( NULL, &svc_uuid );

	// specify that we want a list of all the matching applications' attributes
	uint32_t range = 0x0000ffff;
	attrid_list = sdp_list_append( NULL, &range );

	// get a list of service records that have UUID 0xabcd
	err = sdp_service_search_attr_req( session, search_list, SDP_ATTR_REQ_RANGE, attrid_list, &response_list);

	sdp_list_t *r = response_list;

	// go through each of the service records
	for (; r; r = r->next )
	{
		sdp_record_t *rec = (sdp_record_t*) r->data;
		sdp_record_print(rec);							// print smt
		printf("Service RecHandle: 0x%x\n", rec->handle);
		sdp_list_t *proto_list;

		// get a list of the protocol sequences
		if( sdp_get_access_protos( rec, &proto_list ) == 0 ) {
			sdp_list_t *p = proto_list;
			// go through each protocol sequence
			for( ; p ; p = p->next )
			{
				sdp_list_t *pds = (sdp_list_t*)p->data;
				// go through each protocol list of the protocol sequence
				for( ; pds ; pds = pds->next )
				{
					// check the protocol attributes
					sdp_data_t *d = (sdp_data_t*)pds->data;
					int proto = 0;
					for( ; d; d = d->next )
					{
						switch( d->dtd ) {
							case SDP_UUID16:
							case SDP_UUID32:
							case SDP_UUID128:
								proto = sdp_uuid_to_proto( &d->val.uuid );
								break;
							case SDP_UINT8:
								if( proto == RFCOMM_UUID ) {
									printf("rfcomm channel: \x1b[31;1m%d\x1b[0m\n",d->val.int8);
									//printf("rfcomm channel: %d\n",d->val.int8);
								}
							break;
						}
					}
				}
				sdp_list_free( (sdp_list_t*)p->data, 0 );
			}
			sdp_list_free( proto_list, 0 );
		}else printf("non\n");
		printf("found service record: 0x%x\n", rec->handle);
		sdp_record_free( rec );
	}
	sdp_close(session);

	delete[] ii;
	return 0;
}
#include <stdlib.h>
#include <unistd.h>

#include "BluetoothLocator.h"

#define EIR_FLAGS 0X01
#define EIR_NAME_SHORT 0x08
#define EIR_NAME_COMPLETE 0x09
#define EIR_MANUFACTURE_SPECIFIC 0xFF

void process_data(uint8_t* data, size_t data_len, le_advertising_info* info)
{
	printf("data: %p, data len: %d\n", data, data_len);
	if (data[0] == EIR_NAME_SHORT || data[0] == EIR_NAME_COMPLETE)
	{
		size_t name_len = data_len - 1;
		char* name = malloc(name_len + 1);
		memset(name, 0, name_len + 1);
		memcpy(name, &data[2], name_len);

		char addr[18];
		ba2str(&info->bdaddr, addr);

		printf("addr=%s name=%s\n", addr, name);

		free(name);
	}
	else if (data[0] == EIR_FLAGS)
	{
		printf("Flag type: len=%d\n", data_len);
		int i;
		for (i = 1; i < data_len; i++)
		{
			printf("\tFlag data: 0x%0X\n", data[i]);
		}
	}
	else if (data[0] == EIR_MANUFACTURE_SPECIFIC)
	{
		printf("Manufacture specific type: len=%d\n", data_len);

		// TODO int company_id = data[current_index + 2] 

		int i;
		for (i = 1; i < data_len; i++)
		{
			printf("\tData: 0x%0X\n", data[i]);
		}
	}
	else
	{
		printf("Unknown type: type=%X\n", data[0]);
	}
}

int main(void)
{
	//initscr();
	//timeout(0);

	BluetoothLocator* locator = BluetoothLocator_openDefaultDevice();

	if (locator == NULL)
	{
		printf("device is null");
		return 0;
	}

#define CHECK_FOR_ERROR if (BluetoothLocator_hasError(locator)) \
	{ \
		const char* text = BluetoothLocator_getError(locator); \
		printf("ERROR: %s", text); \
		return -1; \
	}

	CHECK_FOR_ERROR

	BluetoothLocator_startScan(locator);

	CHECK_FOR_ERROR

	printf("Scanning...\n");

	bool done = false;
	bool error = false;
	int device_handler = BluetoothLocator_getDeviceHandler(locator);
	while (!done && !error)
	{
		int len = 0;
		unsigned char buf[HCI_MAX_EVENT_SIZE];
		while ((len = read(device_handler, buf, sizeof(buf))) < 0)
		{
			if (errno == EINTR)
			{
				done = true;
				break;
			}

			if (errno == EAGAIN || errno == EINTR)
			{
				usleep(100);
				continue;
			}

			error = true;
		}

		if (!done && !error)
		{
			evt_le_meta_event *meta = (void*)buf[1 + HCI_EVENT_HDR_SIZE];

			len -= 1 + HCI_EVENT_HDR_SIZE;

			if (meta->subevent != EVT_LE_ADVERTISING_REPORT)
			{
				continue;
			}

			le_advertising_info *info = (void*)meta->data[1];

			if (info->length == 0)
			{
				continue;
			}

			printf("Event: %d\n", info->evt_type);
			printf("Length: %d\n", info->length);

			size_t current_index = 0;
			int data_error = 0;

			while (!data_error && current_index < info->length)
			{
				size_t data_len = info->data[current_index];

				if (data_len + 1 > info->length)
				{
					printf("EIR data length is longer than EIR packet length. %d + 1 > %d", data_len, info->length);
					data_error = 1;
				}
				else
				{
					process_data((void*)info->data[current_index + 1], data_len, info);
					current_index += data_len + 1;
				}
			}
		}
	}

	if (error)
	{
		printf("Error scanning.");
	}

	BluetoothLocator_stopScan(locator);

	CHECK_FOR_ERROR

	BluetoothLocator_closeDevice(locator);

	//endwin();
	return 0;
}
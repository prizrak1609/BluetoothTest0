#include <stdlib.h>
#include <unistd.h>

#include "BluetoothLocator.h"

int main(void)
{
	//initscr();
	//timeout(0);

	BluetoothLocator* locator = BluetoothLocator_openDefaultDevice();

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
				if (getch() == 'q')
				{
					done = true;
					break;
				}

				usleep(100);
				continue;
			}

			error = true;
		}

		if (!done && !error)
		{
			evt_le_meta_event *meta = (void *)(buf + (1 + HCI_EVENT_HDR_SIZE));

			len -= (1 + HCI_EVENT_HDR_SIZE);

			if (meta->subevent != EVT_LE_ADVERTISING_REPORT)
			{
				continue;
			}

			le_advertising_info *info = (le_advertising_info *)(meta->data + 1);

			if (info->length == 0)
			{
				continue;
			}

			printf("Event: %d\n", info->evt_type);
			printf("Length: %d\n", info->length);

			int current_index = 0;
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
					process_data(info->data + current_index + 1, data_len, info);
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
#include "BluetoothLocator.h"

// ncurses
#include <curses.h>

struct hci_state {
	int device_id;
	int device_handle;
	struct hci_filter original_filter;
	int state;
	int has_error;
	char error_message[1024];
};

typedef struct hci_state hci_state;

#define HCI_STATE_OPEN 2
#define HCI_STATE_SCANNING 3
#define HCI_STATE_FILTERING 4

#define EIR_FLAGS 0X01
#define EIR_NAME_SHORT 0x08
#define EIR_NAME_COMPLETE 0x09
#define EIR_MANUFACTURE_SPECIFIC 0xFF

bool BluetoothLocator_hasError(const BluetoothLocator *const instance)
{
	if (instance)
	{
		hci_state* current_hci_state = instance->currentState;
		if (current_hci_state)
		{
			return current_hci_state->has_error;
		}
	}
	return false;
}

const char* BluetoothLocator_getError(const BluetoothLocator* const instance)
{
	hci_state* current_hci_state = instance->currentState;
	if (current_hci_state)
	{
		return current_hci_state->error_message;
	}
	return NULL;
}

void process_data(uint8_t *data, size_t data_len, le_advertising_info *info)
{
	printf("data: %p, data len: %d\n", data, data_len);
	if (data[0] == EIR_NAME_SHORT || data[0] == EIR_NAME_COMPLETE)
	{
		size_t name_len = data_len - 1;
		char *name = malloc(name_len + 1);
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

BluetoothLocator* BluetoothLocator_new()
{
	return calloc(1, sizeof(BluetoothLocator));
}

void BluetoothLocator_free(BluetoothLocator *instance)
{
	if (instance)
	{
		if (instance->currentState)
		{
			cfree(instance->currentState);
		}
		cfree(instance);
	}
}

BluetoothLocator* BluetoothLocator_openDefaultDevice()
{
	BluetoothLocator* locator = BluetoothLocator_new();

	hci_state *current_hci_state = calloc(1, sizeof(hci_state));

	current_hci_state->device_id = hci_get_route(NULL);

	if ((current_hci_state->device_handle = hci_open_dev(current_hci_state->device_id)) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Could not open device: %s", strerror(errno));
		return;
	}

	// Set fd non-blocking
	int on = 1;
	if (ioctl(current_hci_state->device_handle, FIONBIO, (char *)&on) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Could set device to non-blocking: %s", strerror(errno));
		return;
	}

	current_hci_state->state = HCI_STATE_OPEN;

	locator->currentState = current_hci_state;

	return locator;
}

void BluetoothLocator_startScan(const BluetoothLocator *const instance)
{
	hci_state *current_hci_state = instance->currentState;

	if (hci_le_set_scan_parameters(current_hci_state->device_handle, 0x01, htobs(0x0010), htobs(0x0010), 0x00, 0x00, 1000) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Failed to set scan parameters: %s", strerror(errno));
		return;
	}

	if (hci_le_set_scan_enable(current_hci_state->device_handle, 0x01, 1, 1000) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Failed to enable scan: %s", strerror(errno));
		return;
	}

	current_hci_state->state = HCI_STATE_SCANNING;

	// Save the current HCI filter
	socklen_t olen = sizeof(current_hci_state->original_filter);
	if (getsockopt(current_hci_state->device_handle, SOL_HCI, HCI_FILTER, &current_hci_state->original_filter, &olen) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Could not get socket options: %s", strerror(errno));
		return;
	}

	// Create and set the new filter
	struct hci_filter new_filter;

	hci_filter_clear(&new_filter);
	hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);
	hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);

	if (setsockopt(current_hci_state->device_handle, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter)) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Could not set socket options: %s", strerror(errno));
		return;
	}

	current_hci_state->state = HCI_STATE_FILTERING;
}

void BluetoothLocator_stopScan(const BluetoothLocator *const instance)
{
	hci_state *current_hci_state = instance->currentState;

	if (current_hci_state->state == HCI_STATE_FILTERING)
	{
		current_hci_state->state = HCI_STATE_SCANNING;
		setsockopt(current_hci_state->device_handle, SOL_HCI, HCI_FILTER, &current_hci_state->original_filter, sizeof(current_hci_state->original_filter));
	}

	if (hci_le_set_scan_enable(current_hci_state->device_handle, 0x00, 1, 1000) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Disable scan failed: %s", strerror(errno));
	}

	current_hci_state->state = HCI_STATE_OPEN;
}

void BluetoothLocator_closeDevice(BluetoothLocator *instance)
{
	hci_state *current_hci_state = instance->currentState;

	if (current_hci_state->state == HCI_STATE_OPEN)
	{
		hci_close_dev(current_hci_state->device_handle);
	}

	BluetoothLocator_free(instance);
}

int BluetoothLocator_getDeviceHandler(const BluetoothLocator *const instance)
{
	return instance->currentState->device_handle;
}

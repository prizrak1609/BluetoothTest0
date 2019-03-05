#include "BluetoothLocator.h"

#include <unistd.h>

// ncurses
#include <curses.h>

struct hci_state {
	int device_id;
	int device_handle;
	struct hci_filter original_filter;
	int state;
	int has_error;
	char error_message[1024];
	void (*error_handler)(const BluetoothLocator* const);
	bool (*data_handler)(unsigned char buf[HCI_MAX_EVENT_SIZE], int len);
};

typedef struct hci_state hci_state;

#define HCI_STATE_OPEN 2
#define HCI_STATE_SCANNING 3
#define HCI_STATE_FILTERING 4

#define CALL_ERROR_HANDLER(instance) if (instance->currentState->error_handler) \
{ \
	instance->currentState->error_handler(instance); \
}

#define NULL_CHECK(instance, ...) if (!instance) \
{ \
	return _VA_LIST_; \
}

bool BluetoothLocator_hasError(const BluetoothLocator *const instance)
{
	NULL_CHECK(instance, false);
	hci_state* current_hci_state = instance->currentState;
	if (current_hci_state)
	{
		return current_hci_state->has_error;
	}
	return false;
}

const char* BluetoothLocator_getError(const BluetoothLocator* const instance)
{
	NULL_CHECK(instance, NULL);
	hci_state* current_hci_state = instance->currentState;
	if (current_hci_state)
	{
		return current_hci_state->error_message;
	}
	return NULL;
}

BluetoothLocator* BluetoothLocator_new()
{
	return calloc(1, sizeof(BluetoothLocator));
}

void BluetoothLocator_free(BluetoothLocator *const instance)
{
	NULL_CHECK(instance);
	if (instance->currentState)
	{
		cfree(instance->currentState);
	}
	cfree(instance);
}

BluetoothLocator* BluetoothLocator_openDefaultDevice()
{
	BluetoothLocator* locator = BluetoothLocator_new();

	hci_state *current_hci_state = calloc(1, sizeof(hci_state));

	locator->currentState = current_hci_state;

	current_hci_state->device_id = hci_get_route(NULL);

	if ((current_hci_state->device_handle = hci_open_dev(current_hci_state->device_id)) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Could not open device: %s", strerror(errno));
		return locator;
	}

	// Set fd non-blocking
	int on = 1;
	if (ioctl(current_hci_state->device_handle, FIONBIO, (char *)&on) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Could set device to non-blocking: %s", strerror(errno));
		return locator;
	}

	current_hci_state->state = HCI_STATE_OPEN;

	return locator;
}

void readData(const BluetoothLocator* const instance)
{
	NULL_CHECK(instance);

	bool done = false;
	while (!done)
	{
		int len = 0;
		unsigned char buf[HCI_MAX_EVENT_SIZE];
		while ((len = BluetoothLocator_readData(instance, buf, sizeof(buf))) < 0)
		{
			if (errno == EINTR)
			{
				done = true;
				break;
			}

			if (errno == EAGAIN || errno == EINTR)
			{
				usleep(500);
				continue;
			}
			break;
		}
		if (!done && instance->currentState->data_handler(buf, len))
		{
			continue;
		}
		break;
	}
}

void BluetoothLocator_startScan(const BluetoothLocator *const instance)
{
	NULL_CHECK(instance);

	hci_state *current_hci_state = instance->currentState;

	if (hci_le_set_scan_parameters(current_hci_state->device_handle, 0x01, htobs(0x0010), htobs(0x0010), 0x00, 0x00, 1000) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Failed to set scan parameters: %s", strerror(errno));
		CALL_ERROR_HANDLER(instance);
		return;
	}

	if (hci_le_set_scan_enable(current_hci_state->device_handle, 0x01, 1, 1000) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Failed to enable scan: %s", strerror(errno));
		CALL_ERROR_HANDLER(instance);
		return;
	}

	current_hci_state->state = HCI_STATE_SCANNING;

	// Save the current HCI filter
	socklen_t olen = sizeof(current_hci_state->original_filter);
	if (getsockopt(current_hci_state->device_handle, SOL_HCI, HCI_FILTER, &current_hci_state->original_filter, &olen) < 0)
	{
		current_hci_state->has_error = TRUE;
		snprintf(current_hci_state->error_message, sizeof(current_hci_state->error_message), "Could not get socket options: %s", strerror(errno));
		CALL_ERROR_HANDLER(instance);
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
		CALL_ERROR_HANDLER(instance);
		return;
	}

	current_hci_state->state = HCI_STATE_FILTERING;

	readData(instance);
}

void BluetoothLocator_stopScan(const BluetoothLocator *const instance)
{
	NULL_CHECK(instance);

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
		CALL_ERROR_HANDLER(instance);
		return;
	}

	current_hci_state->state = HCI_STATE_OPEN;
}

void BluetoothLocator_closeDevice(BluetoothLocator *const instance)
{
	NULL_CHECK(instance);

	hci_state *current_hci_state = instance->currentState;

	if (current_hci_state->state == HCI_STATE_OPEN)
	{
		hci_close_dev(current_hci_state->device_handle);
	}

	BluetoothLocator_free(instance);
}

ssize_t BluetoothLocator_readData(const BluetoothLocator *const instance, unsigned char* buf, size_t size)
{
	NULL_CHECK(instance);

	return read(instance->currentState->device_handle, buf, size);
}

void BluetoothLocator_setErrorHandler(BluetoothLocator *const instance, void (*error_handler)(const BluetoothLocator* const))
{
	NULL_CHECK(instance);

	instance->currentState->error_handler = error_handler;
}

void BluetoothLocator_setDataHandler(const BluetoothLocator *const instance, bool (*data_handler)(unsigned char buf[HCI_MAX_EVENT_SIZE], int len))
{
	NULL_CHECK(instance);

	instance->currentState->data_handler = data_handler;
}

int BluetoothLocator_getDeviceHandler(const BluetoothLocator *const instance)
{
	NULL_CHECK(instance);

	return instance->currentState->device_handle;
}

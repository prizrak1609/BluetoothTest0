#pragma once

#include <stdbool.h>
#include <malloc.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <sys/ioctl.h>
#include <errno.h>

struct BluetoothLocator
{
	struct hci_state *currentState;
};

typedef struct BluetoothLocator BluetoothLocator;

BluetoothLocator* BluetoothLocator_openDefaultDevice();

void BluetoothLocator_closeDevice(BluetoothLocator *const instance);

void BluetoothLocator_setErrorHandler(BluetoothLocator *const instance, void (*error_handler)(const BluetoothLocator *const));

void BluetoothLocator_setDataHandler(const BluetoothLocator *const instance, bool (*data_handler)(unsigned char buf[HCI_MAX_EVENT_SIZE], int len));

void BluetoothLocator_startScan(const BluetoothLocator *const instance);

void BluetoothLocator_stopScan(const BluetoothLocator *const instance);

bool BluetoothLocator_hasError(const BluetoothLocator *const instance);

const char* BluetoothLocator_getError(const BluetoothLocator *const instance);

int BluetoothLocator_getDeviceHandler(const BluetoothLocator *const instance);

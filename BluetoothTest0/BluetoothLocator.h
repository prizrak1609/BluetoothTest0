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

BluetoothLocator* BluetoothLocator_new();
void BluetoothLocator_free(BluetoothLocator *instance);
void BluetoothLocator_openDefaultDevice(BluetoothLocator *instance);
void BluetoothLocator_closeDevice(const BluetoothLocator *const instance);
void BluetoothLocator_startScan(const BluetoothLocator *const instance);
void BluetoothLocator_stopScan(const BluetoothLocator *const instance);
bool BluetoothLocator_checkForErrorAndPrint(const BluetoothLocator *const instance);
int BluetoothLocator_getDeviceHandler(const BluetoothLocator *const instance);
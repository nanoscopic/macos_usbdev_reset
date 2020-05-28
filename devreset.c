// Copyright (C) 2020 Jesús A. Álvarez
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <mach/mach.h>

#define kExcAccMaxWait 5

static IONotificationPortRef gNotificationPort;
static io_iterator_t gDevIter;
static mach_port_t gMachPort;

void DeviceConnected (void *refCon, io_iterator_t iterator);

int main (int argc, char const *argv[])
{
  if (argc < 3) {
    fprintf(stderr, "usage: %s productID vendorID\n", argv[0]);
    exit(1);
  }

  SInt32 productID, vendorID;
  productID = strtol(argv[1], NULL, 0);
  vendorID = strtol(argv[2], NULL, 0);

  if (productID <= 0 || productID > 0xffff || vendorID <= 0 || vendorID > 0xffff) {
    fprintf(stderr, "Invalid productID or vendorID\n");
    exit(1);
  }

  printf("Looking for productID=0x%04x vendorID=0x%04x\n", productID, vendorID);

  kern_return_t kerr;
  kerr = IOMasterPort(MACH_PORT_NULL, &gMachPort);
  if (kerr || !gMachPort) return 1;

  CFMutableDictionaryRef dict = IOServiceMatching(kIOUSBDeviceClassName);
  if (!dict) {
    fprintf(stderr, "could not create matching dictionary for device vendor=0x%04X,product=0x%04X\n", vendorID, productID);
    exit(1);
  }
  CFDictionarySetValue(dict, CFSTR(kUSBVendorID), CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vendorID));
  CFDictionarySetValue(dict, CFSTR(kUSBProductID), CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &productID));
  gNotificationPort = IONotificationPortCreate(gMachPort);
  kerr = IOServiceAddMatchingNotification(gNotificationPort, kIOFirstMatchNotification, dict, DeviceConnected, NULL, &gDevIter);
  if (kerr != kIOReturnSuccess) {
    fprintf(stderr, "IOServiceAddMatchingNotification\n");
    exit(1);
  }
  DeviceConnected(NULL, gDevIter);
  mach_port_deallocate(mach_task_self(), gMachPort);

  return 0;
}

IOReturn ConfigureDevice (IOUSBDeviceInterface245 **dev) {
  UInt8 nConf;
  IOUSBConfigurationDescriptorPtr confDescPtr;

  (*dev)->GetNumberOfConfigurations(dev, &nConf);
  if (nConf == 0) return kIOReturnError;

  if ((*dev)->GetConfigurationDescriptorPtr(dev, 0, &confDescPtr)) return kIOReturnError;
  if ((*dev)->SetConfiguration(dev, confDescPtr->bConfigurationValue)) return kIOReturnError;

  return kIOReturnSuccess;
}

void DeviceConnected (void *refCon, io_iterator_t iterator) {
  kern_return_t kerr;
  io_service_t device;
  IOCFPlugInInterface **iodev;
  IOUSBDeviceInterface245 **dev = NULL;
  UInt16 vendor, product, version;
  HRESULT result;
  int u;

  while ((device = IOIteratorNext(iterator))) {
    // get device plugin
    kerr = IOCreatePlugInInterfaceForService(device, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, (SInt32*)&u);
    IOObjectRelease(device);
    if (kerr != kIOReturnSuccess) {
      fprintf(stderr, "could not create plug-in interface: %08x\n", kerr);
      continue;
    }

    // get device interface
    result = (*iodev)->QueryInterface(iodev, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245), (LPVOID)&dev);
    IODestroyPlugInInterface(iodev);
    if (result || !dev) {
      fprintf(stderr, "could not get device interface: %08x\n", (int)result);
      continue;
    }

    // get device data
    (*dev)->GetDeviceVendor(dev, &vendor);
    (*dev)->GetDeviceProduct(dev, &product);
    (*dev)->GetDeviceReleaseNumber(dev, &version);
    fprintf(stdout, "Found device vendor=0x%04X, product=0x%04X, version=0x%04X\n", vendor, product, version);

    // open device
    u = 0;
    do {
      kerr = (*dev)->USBDeviceOpen(dev);
      if (kerr == kIOReturnExclusiveAccess) {
        fprintf(stdout, "waiting for access (%d)\n", kExcAccMaxWait-u);
        u++;
        sleep(1);
      }
    } while ((kerr == kIOReturnExclusiveAccess) && (u < kExcAccMaxWait));

    if (kerr != kIOReturnSuccess) {
      fprintf(stderr, "could not open device: %08x\n", kerr);
      (void) (*dev)->Release(dev);
      continue;
    }

    // configure device
    if (ConfigureDevice(dev) != kIOReturnSuccess) {
      fprintf(stderr, "could not configure device\n");
      (*dev)->USBDeviceClose(dev);
      (*dev)->Release(dev);
      continue;
    }

    // reenumerate to device
    kerr = (*dev)->USBDeviceReEnumerate(dev, 0);
    if (kerr != kIOReturnSuccess) {
      fprintf(stdout, "USBDeviceReEnumerate: error %d\n", kerr);
    }

    // close device
    (*dev)->USBDeviceClose(dev);
    (*dev)->Release(dev);
  }
}

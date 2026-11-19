/*
 * What is in the cartridge port, from plain reads of the window: no command
 * is sent, so this works on a device whose command path is broken.
 */
#ifndef DEVICE_H
#define DEVICE_H

#include "mdtest.h"

typedef enum {
  DEVICE_NONE,        /* open bus */
  DEVICE_OTHER,       /* something serves data, not the test header */
  DEVICE_OLD,         /* v0.0.3's Multidevice Test */
  DEVICE_TEST         /* the test firmware's header */
} DeviceKind;

typedef struct {
  DeviceKind kind;
  unsigned short layout;
  unsigned short state;
  char version[MDTEST_HDR_VERSION_WORDS * 2 + 1];
  char build[MDTEST_HDR_BUILD_WORDS * 2 + 1];
} Device;

/* Supervisor mode. */
void device_recognise(Device *d);

#endif

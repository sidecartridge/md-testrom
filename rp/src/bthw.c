/**
 * File: bthw.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The Bluetooth controller (bthw.h) over the SDK's BTstack:
 *              cyw43_arch_init() starts BTstack with the chip
 *              (btstack_cyw43_no_flash.c), this file powers the controller
 *              on, takes its identity from its version event and hands
 *              each device a scan hears to the test.
 */

#include "bthw.h"

#include <string.h>

#include "ad_parser.h"
#include "bluetooth_data_types.h"
#include "btstack_event.h"
#include "debug.h"
#include "devtests.h"
#include "gap.h"
#include "hci.h"
#include "pico/cyw43_arch.h"

#define MS_PER_INQUIRY_UNIT 1280U
// A low-energy scan, active (it asks for the scan response, where names
// often are), every 30 ms for 30 ms: listening all the time.
#define LE_SCAN_ACTIVE 1U
#define LE_SCAN_INTERVAL 0x30U
#define LE_SCAN_WINDOW 0x30U
// The version event's return parameters: status, HCI version, HCI
// revision (2), LMP version, manufacturer (2), LMP subversion (2).
#define VERSION_HCI 1U
#define VERSION_MANUFACTURER 5U
#define VERSION_LMP_SUBVERSION 7U

#if (defined(_DEBUG) && (_DEBUG != 0))
#define CHIP_FAILS devtestsChipFails
#else
#define CHIP_FAILS false
#endif

static btstack_packet_callback_registration_t registration;
static BtHwDeviceSeen seenCallback;
static BtHwInfo controller;
static volatile int state = BTHW_OFF;
static bool chipUp;
static bool inquiring;
static bool leScanning;

static void nameOf(const uint8_t *from, unsigned length, char *out) {
  if (length > BTHW_NAME_BYTES - 1U) length = BTHW_NAME_BYTES - 1U;
  memcpy(out, from, length);
  out[length] = '\0';
}

static void inquiryResult(const uint8_t *packet) {
  BtHwDevice device;
  memset(&device, 0, sizeof(device));
  gap_event_inquiry_result_get_bd_addr(packet, device.address);
  device.kind = BTHW_KIND_CLASSIC;
  if (gap_event_inquiry_result_get_rssi_available(packet)) {
    device.rssi = (int8_t)gap_event_inquiry_result_get_rssi(packet);
  }
  if (gap_event_inquiry_result_get_name_available(packet)) {
    nameOf(gap_event_inquiry_result_get_name(packet),
           gap_event_inquiry_result_get_name_len(packet), device.name);
  }
  if (seenCallback) seenCallback(&device);
}

static void advertisingReport(const uint8_t *packet) {
  BtHwDevice device;
  memset(&device, 0, sizeof(device));
  gap_event_advertising_report_get_address(packet, device.address);
  device.kind = BTHW_KIND_LE;
  device.rssi = (int8_t)gap_event_advertising_report_get_rssi(packet);
  ad_context_t context;
  for (ad_iterator_init(&context, gap_event_advertising_report_get_data_length(packet),
                        gap_event_advertising_report_get_data(packet));
       ad_iterator_has_more(&context); ad_iterator_next(&context)) {
    uint8_t type = ad_iterator_get_data_type(&context);
    if (type == BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME ||
        type == BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME) {
      nameOf(ad_iterator_get_data(&context), ad_iterator_get_data_len(&context),
             device.name);
    }
  }
  if (seenCallback) seenCallback(&device);
}

static void versionInformation(const uint8_t *packet) {
  if (hci_event_command_complete_get_command_opcode(packet) !=
      HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION) {
    return;
  }
  const uint8_t *answer = hci_event_command_complete_get_return_parameters(packet);
  if (answer[0] != ERROR_CODE_SUCCESS) return;
  controller.hciVersion = answer[VERSION_HCI];
  controller.manufacturer = little_endian_read_16(answer, VERSION_MANUFACTURER);
  controller.lmpSubversion = little_endian_read_16(answer, VERSION_LMP_SUBVERSION);
}

static void events(uint8_t type, uint16_t channel, uint8_t *packet,
                   uint16_t size) {
  (void)channel;
  (void)size;
  if (type != HCI_EVENT_PACKET) return;
  switch (hci_event_packet_get_type(packet)) {
    case BTSTACK_EVENT_STATE:
      if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
        gap_local_bd_addr(controller.address);
        state = BTHW_WORKING;
      } else if (btstack_event_state_get_state(packet) == HCI_STATE_OFF &&
                 state == BTHW_STARTING) {
        state = BTHW_FAILED;
      }
      break;
    case HCI_EVENT_COMMAND_COMPLETE:
      versionInformation(packet);
      break;
    case GAP_EVENT_INQUIRY_RESULT:
      inquiryResult(packet);
      break;
    case GAP_EVENT_INQUIRY_COMPLETE:
      inquiring = false;
      break;
    case GAP_EVENT_ADVERTISING_REPORT:
      advertisingReport(packet);
      break;
    default:
      break;
  }
}

int bthw_start(BtHwDeviceSeen seen) {
  seenCallback = seen;
  memset(&controller, 0, sizeof(controller));
  inquiring = false;
  leScanning = false;
  if (CHIP_FAILS) return -1;
  if (!chipUp) {
    // Starts BTstack too (btstack_cyw43_init()), from fresh.
    int answer = cyw43_arch_init();
    if (answer != 0) {
      DPRINTF("Bluetooth: the radio chip did not start: %d\n", answer);
      return answer < 0 ? answer : -answer;
    }
    chipUp = true;
  }
  registration.callback = &events;
  hci_add_event_handler(&registration);
  // With signal strength and the extended data, where names are.
  hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);
  state = BTHW_STARTING;
  if (hci_power_control(HCI_POWER_ON) != 0) state = BTHW_FAILED;
  return 0;
}

int bthw_state(void) {
  if (chipUp) cyw43_arch_poll();
  return state;
}

void bthw_info(BtHwInfo *info) { *info = controller; }

int bthw_scan(bool classic, uint32_t seconds) {
  if (state != BTHW_WORKING) return -1;
  if (classic) {
    uint32_t units = (seconds * 1000U + MS_PER_INQUIRY_UNIT - 1U) / MS_PER_INQUIRY_UNIT;
    inquiring = true;
    int answer = gap_inquiry_start((uint8_t)(units > 0U ? units : 1U));
    if (answer != 0) inquiring = false;
    return answer;
  }
  gap_set_scan_params(LE_SCAN_ACTIVE, LE_SCAN_INTERVAL, LE_SCAN_WINDOW, 0);
  gap_start_scan();
  leScanning = true;
  return 0;
}

bool bthw_scanning(void) {
  if (chipUp) cyw43_arch_poll();
  return inquiring || leScanning;
}

void bthw_scanStop(void) {
  if (inquiring) (void)gap_inquiry_stop();
  if (leScanning) gap_stop_scan();
  inquiring = false;
  leScanning = false;
  if (chipUp) cyw43_arch_poll();
}

void bthw_stop(void) {
  seenCallback = NULL;
  if (chipUp) {
    bthw_scanStop();
    cyw43_arch_deinit();  // the controller off too (btstack_cyw43_deinit())
    chipUp = false;
  }
  state = BTHW_OFF;
}

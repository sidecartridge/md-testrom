/**
 * File: btstack_config.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: BTstack for the Bluetooth test: the controller started and a
 *              scan, classic and low energy. No connection is made, nothing
 *              is paired and nothing is stored, so the pools are the
 *              smallest BTstack builds with (taken from md-mv16-emulator's,
 *              without its audio).
 */

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

#define ENABLE_LOG_ERROR
#define ENABLE_PRINTF_HEXDUMP  // the SDK builds its HCI dump, unused here

// Low energy as a central, the role that scans; the peripheral role only
// because the SDK's low-energy library builds its attribute server, which
// needs it (nothing advertises).
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_PERIPHERAL

// Buffers: the scan needs none of its own; one small connection's worth.
#define HCI_OUTGOING_PRE_BUFFER_SIZE 4
#define HCI_ACL_PAYLOAD_SIZE (255 + 4)
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4

#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_L2CAP_SERVICES 1
#define MAX_NR_L2CAP_CHANNELS 1
#define MAX_NR_SERVICE_RECORD_ITEMS 0
#define MAX_NR_AVDTP_STREAM_ENDPOINTS 0
#define MAX_NR_AVDTP_CONNECTIONS 0
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 0
#define MAX_NR_BNEP_SERVICES 0
#define MAX_NR_BNEP_CHANNELS 0
#define MAX_NR_WHITELIST_ENTRIES 0
#define MAX_NR_SM_LOOKUP_ENTRIES 0
#define MAX_NR_RFCOMM_MULTIPLEXERS 0
#define MAX_NR_RFCOMM_SERVICES 0
#define MAX_NR_RFCOMM_CHANNELS 0
#define MAX_NR_HFP_CONNECTIONS 0
#define MAX_NR_AVRCP_CONNECTIONS 0
#define MAX_NR_GATT_CLIENTS 0
#define MAX_NR_LE_DEVICE_DB_ENTRIES 0
// The low-energy library builds its attribute database; a scan uses none.
#define MAX_ATT_DB_SIZE 16

// The controller's buffers, kept small on the CYW43's shared bus.
#define MAX_NR_CONTROLLER_ACL_BUFFERS 3
#define MAX_NR_CONTROLLER_SCO_PACKETS 3
#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
#define HCI_HOST_ACL_PACKET_LEN (255 + 4)
#define HCI_HOST_ACL_PACKET_NUM 3
#define HCI_HOST_SCO_PACKET_LEN 120
#define HCI_HOST_SCO_PACKET_NUM 3

// Nothing kept: no link keys, and the device database (which the
// low-energy library wants at least one entry of) sits on the no-op TLV
// backend; BTstack never writes flash.
#define NVM_NUM_LINK_KEYS 0
#define NVM_NUM_DEVICE_DB_ENTRIES 1

#define HAVE_EMBEDDED_TIME_MS
#define HAVE_ASSERT

#endif  // BTSTACK_CONFIG_H

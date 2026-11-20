/**
 * File: btstack_cyw43_no_flash.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The SDK's BTstack start for the CYW43 (cyw43_arch_init()
 *              calls it), without its flash-backed key store: BTstack gets
 *              the no-op TLV backend and no link key database, so it never
 *              writes flash. From md-mv16-emulator.
 */

#include "btstack_memory.h"
#include "btstack_tlv.h"
#include "btstack_tlv_none.h"
#include "hci.h"
#include "pico/btstack_cyw43.h"
#include "pico/btstack_hci_transport_cyw43.h"
#include "pico/btstack_run_loop_async_context.h"

bool btstack_cyw43_init(async_context_t *context) {
  btstack_memory_init();
  btstack_run_loop_init(btstack_run_loop_async_context_get_instance(context));
  hci_init(hci_transport_cyw43_instance(), NULL);
  btstack_tlv_set_instance(btstack_tlv_none_init_instance(), NULL);
#ifdef ENABLE_CLASSIC
  hci_set_link_key_db(NULL);
#endif
  return true;
}

void btstack_cyw43_deinit(__unused async_context_t *context) {
  hci_power_control(HCI_POWER_OFF);
  hci_close();
  btstack_run_loop_async_context_deinit();
  btstack_run_loop_deinit();
  btstack_memory_deinit();
}

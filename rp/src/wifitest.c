/**
 * File: wifitest.c
 * Author: Diego Parrilla Santamaría
 * Date: November 2026
 * Copyright: 2026 - GOODDATA LABS SL
 * Description: The Wi-Fi test (wifitest.h). The chip and the join are the
 *              template's (network.c, with Booster's settings); this file
 *              steps them, tells the failures apart from the driver's link
 *              status, and asks the router (ARP), a name (DNS) and a web
 *              server (one HEAD request over a raw TCP connection) in turn.
 */

#include "wifitest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "devtests.h"
#include "gconfig.h"
#include "jobs.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/tcp.h"
#include "network.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#define US_PER_MS 1000U
#define MS_PER_SECOND 1000U
#define PROGRESS_DONE 100U
#define HTTP_PORT 80U

// How long each step may take. The scan hears every channel in about 3 s;
// joining is the template's 30 s, of which a network that is not there gets
// 15 (asked again every 2 s, as network.c does) and DHCP 15 once joined.
#define SCAN_MAX_MS 10000U
#define JOIN_MAX_MS 30000U
#define NONET_MAX_MS 15000U
#define REJOIN_MS 2000U
#define ADDRESS_MAX_MS 15000U
#define ROUTER_MAX_MS 3000U
#define ROUTER_ASK_MS 1000U
#define DNS_MAX_MS 10000U
#define WEB_MAX_MS 10000U

#define SEEN_MAX 16U        // networks kept from a scan, by name
#define NAME_BYTES 33U      // a network's name: 32 bytes and the end
#define STATUS_BYTES 48U    // the web server's first line, as much as kept
#define FIRMWARE_BYTES 128U // the chip's "ver" answer

#if (defined(_DEBUG) && (_DEBUG != 0)) || (defined(EMUMD) && EMUMD)
#define CHIP_FAILS devtestsChipFails
#else
#define CHIP_FAILS false
#endif

typedef enum {
  STEP_CHIP,
  STEP_SCAN_START,
  STEP_SCAN,
  STEP_JOIN_START,
  STEP_JOIN,
  STEP_ROUTER_START,
  STEP_ROUTER,
  STEP_DNS_START,
  STEP_DNS,
  STEP_WEB_START,
  STEP_WEB,
  STEP_END,
} Step;

typedef struct {
  char name[NAME_BYTES];
  int16_t rssi;
  uint16_t channel;
  uint8_t auth;
} Seen;

static Step step;
static uint16_t phase;
static uint16_t endState;
static uint32_t phaseMs;   // when the phase began
static uint32_t stepMs;    // when the current wait began
static uint32_t askedMs;   // when the router or the network was last asked
static uint32_t routerUs;  // when the router was first asked, in us
static bool chipUp;
static char countryKept[NAME_BYTES];  // Booster's country, as it was
static bool countryWasSet;

static Seen seen[SEEN_MAX];
static uint16_t seenCount;   // kept, at most SEEN_MAX
static uint16_t heardCount;  // every distinct name heard, and the hidden
static uint16_t hiddenCount;
static char configured[NAME_BYTES];

static uint32_t joinedMs;    // joined, waiting for an address (0: not yet)
static bool dnsDone;
static bool dnsFound;
static ip_addr_t serverIp;
static struct tcp_pcb *webPcb;
static bool webEnded;
static err_t webError;
static char webStatus[STATUS_BYTES];
static uint16_t webStatusLength;

static uint32_t since(uint32_t nowMs, uint32_t thenMs) { return nowMs - thenMs; }

static void enterPhase(uint16_t which, uint32_t nowMs) {
  phase = which;
  phaseMs = nowMs;
  stepMs = nowMs;
}

static void fail(const char *error) {
  jobs_resultLine("error=%s", error);
  endState = MDTEST_JOB_FAILED;
  step = STEP_END;
}

// A network's name as text: bytes that are not printable as '?'.
static void nameText(const uint8_t *from, unsigned length, char *out) {
  if (length > NAME_BYTES - 1U) length = NAME_BYTES - 1U;
  for (unsigned i = 0; i < length; i++) {
    out[i] = (from[i] >= ' ' && from[i] <= '~') ? (char)from[i] : '?';
  }
  out[length] = '\0';
}

static const char *authText(uint8_t auth) {
  // The scan's auth_mode bits: 1 WEP, 2 WPA, 4 WPA2 (WPA3 shows as WPA2).
  if (auth == 0U) return "open";
  if (auth & 4U) return "WPA2";
  if (auth & 2U) return "WPA";
  return "WEP";
}

static const char *settingValue(const char *key) {
  SettingsConfigEntry *entry = settings_find_entry(gconfig_getContext(), key);
  return entry != NULL ? entry->value : NULL;
}

// --- The chip -------------------------------------------------------------

#if !(defined(EMUMD) && EMUMD)
// The chip's firmware, from its "ver" variable: "version 7.95.61 (...)".
static void firmwareLine(void) {
  static uint8_t ver[FIRMWARE_BYTES];
  memset(ver, 0, sizeof(ver));
  memcpy(ver, "ver", 4);
  if (cyw43_ioctl(&cyw43_state, CYW43_IOCTL_GET_VAR, sizeof(ver) - 1U, ver,
                  CYW43_ITF_STA) != 0) {
    return;
  }
  ver[sizeof(ver) - 1U] = '\0';
  char *found = strstr((char *)ver, "version ");
  if (found == NULL) return;
  found += strlen("version ");
  char *end = found;
  while (*end > ' ' && *end <= '~') end++;
  *end = '\0';
  jobs_resultLine("chip_firmware=%s", found);
}
#endif

static void chipStep(uint32_t nowMs) {
  const char *country = settingValue(PARAM_WIFI_COUNTRY);
  countryWasSet = country != NULL;
  if (countryWasSet) snprintf(countryKept, sizeof(countryKept), "%s", country);
  uint32_t startUs = time_us_32();
  int answer = CHIP_FAILS ? -1 : network_wifiInit(WIFI_MODE_STA);
  uint32_t chipMs = (time_us_32() - startUs) / US_PER_MS;
  // network_wifiInit() writes the country back as it understood it; the
  // settings are Booster's, and leaving to Booster saves them.
  if (countryWasSet) {
    settings_put_string(gconfig_getContext(), PARAM_WIFI_COUNTRY, countryKept);
  }
  if (answer != 0) {
    jobs_resultLine("radio=no answer");
    fail("chip silent");
    return;
  }
  chipUp = true;
  uint8_t mac[NETWORK_MAC_SIZE] = {0};
  (void)cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
  jobs_resultLine("radio=ok");
  jobs_resultLine("chip_ms=%lu", (unsigned long)chipMs);
  jobs_resultLine("mac=%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);
  jobs_resultLine("country=%s", countryWasSet && countryKept[0] ? countryKept
                                                                 : "XX");
#if !(defined(EMUMD) && EMUMD)
  firmwareLine();
#endif
  DPRINTF("Wi-Fi: the chip started in %lu ms\n", (unsigned long)chipMs);
  enterPhase(MDTEST_WIFI_PHASE_SCAN, nowMs);
  step = STEP_SCAN_START;
}

// --- The scan ---------------------------------------------------------------

static int scanResult(void *env, const cyw43_ev_scan_result_t *res) {
  (void)env;
  if (res == NULL) return 0;
  if (res->ssid_len == 0U) {
    hiddenCount++;
    return 0;
  }
  char name[NAME_BYTES];
  nameText(res->ssid, res->ssid_len, name);
  for (unsigned i = 0; i < seenCount; i++) {
    if (strcmp(seen[i].name, name) == 0) {
      if (res->rssi > seen[i].rssi) {  // the same network, heard stronger
        seen[i].rssi = res->rssi;
        seen[i].channel = res->channel;
        seen[i].auth = res->auth_mode;
      }
      return 0;
    }
  }
  heardCount++;
  unsigned slot = seenCount;
  if (seenCount < SEEN_MAX) {
    seenCount++;
  } else {
    // Full: the weakest gives way to a stronger one.
    slot = 0;
    for (unsigned i = 1; i < SEEN_MAX; i++) {
      if (seen[i].rssi < seen[slot].rssi) slot = i;
    }
    if (seen[slot].rssi >= res->rssi && strcmp(name, configured) != 0) return 0;
  }
  memcpy(seen[slot].name, name, sizeof(name));
  seen[slot].rssi = res->rssi;
  seen[slot].channel = res->channel;
  seen[slot].auth = res->auth_mode;
  return 0;
}

static void scanStart(uint32_t nowMs) {
  const char *ssid = settingValue(PARAM_WIFI_SSID);
  snprintf(configured, sizeof(configured), "%s", ssid != NULL ? ssid : "");
  cyw43_wifi_scan_options_t options;
  memset(&options, 0, sizeof(options));
  int answer = cyw43_wifi_scan(&cyw43_state, &options, NULL, scanResult);
  if (answer != 0) {
    jobs_resultLine("scan_error=%d", answer);
    fail("scan failed");
    return;
  }
  stepMs = nowMs;
  step = STEP_SCAN;
}

static int strongerFirst(const void *one, const void *other) {
  return ((const Seen *)other)->rssi - ((const Seen *)one)->rssi;
}

static void scanLines(void) {
  qsort(seen, seenCount, sizeof(seen[0]), strongerFirst);
  jobs_resultLine("networks=%u", heardCount);
  jobs_resultLine("hidden=%u", hiddenCount);
  for (unsigned i = 0; i < seenCount && i < MDTEST_WIFI_NETWORKS_LISTED; i++) {
    jobs_resultLine("net=%d|%u|%s|%s", seen[i].rssi, seen[i].channel,
                    authText(seen[i].auth), seen[i].name);
  }
  if (configured[0] == '\0') {
    jobs_resultLine("wifi=none");
    return;
  }
  jobs_resultLine("wifi=configured");
  jobs_resultLine("ssid=%s", configured);
  for (unsigned i = 0; i < seenCount; i++) {
    if (strcmp(seen[i].name, configured) == 0) {
      jobs_resultLine("configured_seen=yes");
      jobs_resultLine("configured_rssi=%d", seen[i].rssi);
      jobs_resultLine("configured_channel=%u", seen[i].channel);
      return;
    }
  }
  jobs_resultLine("configured_seen=no");
}

static void scanStep(uint32_t nowMs) {
  cyw43_arch_poll();
  bool timedOut = since(nowMs, stepMs) > SCAN_MAX_MS;
  if (cyw43_wifi_scan_active(&cyw43_state) && !timedOut) return;
  scanLines();
  if (timedOut) jobs_resultLine("scan_complete=no");
  if (configured[0] == '\0') {
    fail("no network configured");
    return;
  }
  enterPhase(MDTEST_WIFI_PHASE_JOIN, nowMs);
  step = STEP_JOIN_START;
}

// --- Joining ----------------------------------------------------------------

static bool joinStart(void) {
  wifi_sta_conn_process_status_t started = network_wifiStaConnectStart();
  if (started == NETWORK_WIFI_STA_CONN_OK) return true;
  jobs_resultLine("join_error=%d", (int)started);
  fail(started == NETWORK_WIFI_STA_CONN_ERR_NO_SSID ? "no network configured"
                                                    : "join failed");
  return false;
}

static void addressLines(uint32_t nowMs) {
  struct netif *nif = &cyw43_state.netif[CYW43_ITF_STA];
  char text[IP4ADDR_STRLEN_MAX];
  jobs_resultLine("join=ok");
  jobs_resultLine("join_ms=%lu", (unsigned long)since(nowMs, phaseMs));
  jobs_resultLine("address=%s", ip4addr_ntoa_r(netif_ip4_addr(nif), text,
                                               sizeof(text)));
  jobs_resultLine("mask=%s", ip4addr_ntoa_r(netif_ip4_netmask(nif), text,
                                            sizeof(text)));
  jobs_resultLine("gateway=%s", ip4addr_ntoa_r(netif_ip4_gw(nif), text,
                                               sizeof(text)));
  const ip_addr_t *dns = dns_getserver(0);
  jobs_resultLine("dns=%s", ipaddr_ntoa_r(dns, text, sizeof(text)));
  const char *dhcp = settingValue(PARAM_WIFI_DHCP);
  const char *reason = NULL;
  if (dhcp != NULL && (dhcp[0] == 't' || dhcp[0] == 'T')) {
    jobs_resultLine("addressing=dhcp");
  } else if (network_getStaticConfigRejected(&reason)) {
    jobs_resultLine("addressing=static rejected");
    jobs_resultLine("static_reason=%s", reason != NULL ? reason : "");
  } else {
    jobs_resultLine("addressing=static");
  }
  int32_t rssi = 0;
  if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) == 0 && rssi != 0) {
    jobs_resultLine("rssi=%ld", (long)rssi);
  }
}

static void joinStep(uint32_t nowMs) {
  cyw43_arch_poll();
  int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
  switch (link) {
    case CYW43_LINK_UP:
      addressLines(nowMs);
      enterPhase(MDTEST_WIFI_PHASE_ROUTER, nowMs);
      step = STEP_ROUTER_START;
      return;
    case CYW43_LINK_NOIP:
      if (joinedMs == 0U) {
        joinedMs = nowMs == 0U ? 1U : nowMs;
        jobs_resultLine("joined_ms=%lu", (unsigned long)since(nowMs, phaseMs));
        phase = MDTEST_WIFI_PHASE_ADDRESS;
        stepMs = nowMs;
      }
      if (since(nowMs, joinedMs) > ADDRESS_MAX_MS) fail("no address");
      return;
    case CYW43_LINK_BADAUTH:
      fail("wrong password");
      return;
    case CYW43_LINK_FAIL:
      fail("join failed");
      return;
    case CYW43_LINK_NONET:
      if (since(nowMs, phaseMs) > NONET_MAX_MS) {
        fail("network not found");
        return;
      }
      // The driver does not try again by itself (network.c's poll).
      if (since(nowMs, askedMs) >= REJOIN_MS) {
        askedMs = nowMs;
        (void)joinStart();
      }
      return;
    default:
      break;
  }
  if (since(nowMs, phaseMs) > JOIN_MAX_MS) fail("join timed out");
}

// --- The router -------------------------------------------------------------

static bool routerKnown(struct netif *nif) {
  struct eth_addr *eth = NULL;
  const ip4_addr_t *entry = NULL;
  return etharp_find_addr(nif, netif_ip4_gw(nif), &eth, &entry) >= 0;
}

static void routerAsk(uint32_t nowMs) {
  struct netif *nif = &cyw43_state.netif[CYW43_ITF_STA];
  cyw43_arch_lwip_begin();
  etharp_cleanup_netif(nif);  // an old entry would answer for a dead network
  (void)etharp_request(nif, netif_ip4_gw(nif));
  cyw43_arch_lwip_end();
  askedMs = nowMs;
}

static void routerStart(uint32_t nowMs) {
  const ip4_addr_t *router = netif_ip4_gw(&cyw43_state.netif[CYW43_ITF_STA]);
  if (router == NULL || ip4_addr_isany(router)) {
    fail("no router");
    return;
  }
  routerAsk(nowMs);
  stepMs = nowMs;
  routerUs = time_us_32();
  step = STEP_ROUTER;
}

static void routerStep(uint32_t nowMs) {
  cyw43_arch_poll();
  struct netif *nif = &cyw43_state.netif[CYW43_ITF_STA];
  cyw43_arch_lwip_begin();
  bool known = routerKnown(nif);
  cyw43_arch_lwip_end();
  if (known) {
    jobs_resultLine("router=ok");
    // A router on the same network answers in well under a millisecond.
    jobs_resultLine("router_us=%lu", (unsigned long)(time_us_32() - routerUs));
    enterPhase(MDTEST_WIFI_PHASE_DNS, nowMs);
    step = STEP_DNS_START;
    return;
  }
  if (since(nowMs, stepMs) > ROUTER_MAX_MS) {
    fail("router silent");
    return;
  }
  if (since(nowMs, askedMs) >= ROUTER_ASK_MS) routerAsk(nowMs);
}

// --- The name ---------------------------------------------------------------

static void dnsFoundCb(const char *name, const ip_addr_t *addr, void *arg) {
  (void)name;
  (void)arg;
  dnsDone = true;
  dnsFound = addr != NULL;
  if (addr != NULL) ip_addr_copy(serverIp, *addr);
}

static void dnsResolved(uint32_t nowMs) {
  char text[IPADDR_STRLEN_MAX];
  jobs_resultLine("dns_lookup=ok");
  jobs_resultLine("dns_ms=%lu", (unsigned long)since(nowMs, stepMs));
  jobs_resultLine("server=%s", ipaddr_ntoa_r(&serverIp, text, sizeof(text)));
  enterPhase(MDTEST_WIFI_PHASE_WEB, nowMs);
  step = STEP_WEB_START;
}

static void dnsStart(uint32_t nowMs) {
  jobs_resultLine("host=%s", MDTEST_WIFI_HOST);
  dnsDone = false;
  dnsFound = false;
  stepMs = nowMs;
  cyw43_arch_lwip_begin();
  err_t err = dns_gethostbyname(MDTEST_WIFI_HOST, &serverIp, dnsFoundCb, NULL);
  cyw43_arch_lwip_end();
  if (err == ERR_OK) {
    dnsResolved(nowMs);
  } else if (err == ERR_INPROGRESS) {
    step = STEP_DNS;
  } else {
    jobs_resultLine("dns_error=%d", (int)err);
    fail(err == ERR_MEM ? "out of memory" : "name not found");
  }
}

static void dnsStep(uint32_t nowMs) {
  cyw43_arch_poll();
  if (dnsDone) {
    if (dnsFound) {
      dnsResolved(nowMs);
    } else {
      fail("name not found");
    }
    return;
  }
  if (since(nowMs, stepMs) > DNS_MAX_MS) fail("dns silent");
}

// --- The web server -----------------------------------------------------------

static void webForget(void) {
  if (webPcb == NULL) return;
  tcp_arg(webPcb, NULL);
  tcp_recv(webPcb, NULL);
  tcp_err(webPcb, NULL);
  tcp_sent(webPcb, NULL);
  if (tcp_close(webPcb) != ERR_OK) tcp_abort(webPcb);
  webPcb = NULL;
}

static void webErrCb(void *arg, err_t err) {
  (void)arg;
  webPcb = NULL;  // lwIP has freed it
  webError = err;
  webEnded = true;
}

static err_t webRecvCb(void *arg, struct tcp_pcb *pcb, struct pbuf *buf,
                       err_t err) {
  (void)arg;
  if (buf == NULL) {  // the server closed the connection
    webEnded = true;
    return ERR_OK;
  }
  uint16_t room = (uint16_t)(sizeof(webStatus) - 1U - webStatusLength);
  uint16_t take = buf->tot_len < room ? buf->tot_len : room;
  if (take > 0U) {
    pbuf_copy_partial(buf, webStatus + webStatusLength, take, 0);
    webStatusLength += take;
    webStatus[webStatusLength] = '\0';
  }
  tcp_recved(pcb, buf->tot_len);
  pbuf_free(buf);
  if (strchr(webStatus, '\n') != NULL || webStatusLength == sizeof(webStatus) - 1U) {
    webEnded = true;
  }
  (void)err;
  return ERR_OK;
}

static err_t webConnectedCb(void *arg, struct tcp_pcb *pcb, err_t err) {
  (void)arg;
  if (err != ERR_OK) {
    webError = err;
    webEnded = true;
    return err;
  }
  static const char request[] =
      "HEAD " MDTEST_WIFI_PATH " HTTP/1.1\r\n"
      "Host: " MDTEST_WIFI_HOST "\r\n"
      "User-Agent: MDTEST\r\n"
      "Connection: close\r\n\r\n";
  err_t sent = tcp_write(pcb, request, sizeof(request) - 1U, 0);
  if (sent == ERR_OK) sent = tcp_output(pcb);
  if (sent != ERR_OK) {
    webError = sent;
    webEnded = true;
  }
  return ERR_OK;
}

static void webStart(uint32_t nowMs) {
  webEnded = false;
  webError = ERR_OK;
  webStatus[0] = '\0';
  webStatusLength = 0;
  stepMs = nowMs;
  cyw43_arch_lwip_begin();
  webPcb = tcp_new_ip_type(IP_GET_TYPE(&serverIp));
  err_t err = ERR_MEM;
  if (webPcb != NULL) {
    tcp_err(webPcb, webErrCb);
    tcp_recv(webPcb, webRecvCb);
    err = tcp_connect(webPcb, &serverIp, HTTP_PORT, webConnectedCb);
    if (err != ERR_OK) webForget();
  }
  cyw43_arch_lwip_end();
  if (err != ERR_OK) {
    jobs_resultLine("web_error=%d", (int)err);
    fail(err == ERR_MEM ? "out of memory" : "web refused");
    return;
  }
  step = STEP_WEB;
}

static void webStep(uint32_t nowMs) {
  cyw43_arch_poll();
  bool timedOut = since(nowMs, stepMs) > WEB_MAX_MS;
  if (!webEnded && !timedOut) return;
  cyw43_arch_lwip_begin();
  webForget();
  cyw43_arch_lwip_end();
  // "HTTP/1.1 200 OK": the code after the first space.
  const char *code = strchr(webStatus, ' ');
  if (strncmp(webStatus, "HTTP/", strlen("HTTP/")) == 0 && code != NULL) {
    char number[4] = {0};
    memcpy(number, code + 1, 3);
    jobs_resultLine("web=ok");
    jobs_resultLine("web_ms=%lu", (unsigned long)since(nowMs, stepMs));
    jobs_resultLine("web_status=%s", number);
    endState = MDTEST_JOB_DONE;
    step = STEP_END;
    return;
  }
  if (webError != ERR_OK) jobs_resultLine("web_error=%d", (int)webError);
  if (timedOut) {
    fail("web silent");
  } else {
    fail(webError == ERR_RST ? "web refused" : "web closed");
  }
}

// --- The test -----------------------------------------------------------------

static void stopAll(void) {
  if (chipUp) {
    cyw43_arch_lwip_begin();
    webForget();
    cyw43_arch_lwip_end();
  }
  webPcb = NULL;
  network_deInit();
  chipUp = false;
}

void wifitest_begin(uint32_t nowMs) {
  step = STEP_CHIP;
  endState = MDTEST_JOB_FAILED;
  enterPhase(MDTEST_WIFI_PHASE_CHIP, nowMs);
  askedMs = nowMs;
  chipUp = false;
  seenCount = 0;
  heardCount = 0;
  hiddenCount = 0;
  configured[0] = '\0';
  joinedMs = 0;
  webPcb = NULL;
  jobs_resultLine("test=wifi");
}

uint16_t wifitest_step(uint32_t nowMs, uint16_t *progress,
                       uint16_t live[MDTEST_JOB_LIVE_WORDS]) {
  switch (step) {
    case STEP_CHIP:
      chipStep(nowMs);
      break;
    case STEP_SCAN_START:
      scanStart(nowMs);
      break;
    case STEP_SCAN:
      scanStep(nowMs);
      break;
    case STEP_JOIN_START:
      askedMs = nowMs;
      if (joinStart()) step = STEP_JOIN;
      break;
    case STEP_JOIN:
      joinStep(nowMs);
      break;
    case STEP_ROUTER_START:
      routerStart(nowMs);
      break;
    case STEP_ROUTER:
      routerStep(nowMs);
      break;
    case STEP_DNS_START:
      dnsStart(nowMs);
      break;
    case STEP_DNS:
      dnsStep(nowMs);
      break;
    case STEP_WEB_START:
      webStart(nowMs);
      break;
    case STEP_WEB:
      webStep(nowMs);
      break;
    case STEP_END:
    default:
      stopAll();
      *progress = PROGRESS_DONE;
      return endState;
  }
  *progress = (uint16_t)(phase * PROGRESS_DONE / MDTEST_WIFI_PHASES);
  live[0] = phase;
  live[1] = heardCount;
  live[2] = (uint16_t)(since(nowMs, phaseMs) / MS_PER_SECOND);
  return MDTEST_JOB_RUNNING;
}

void wifitest_abort(void) {
  stopAll();
  step = STEP_END;
}

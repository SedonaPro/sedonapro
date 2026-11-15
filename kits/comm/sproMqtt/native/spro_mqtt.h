// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 SedonaPro contributors
//
// Small MQTT 3.1.1 client for the Sedona VM: non-blocking sockets, no threads,
// driven by spro_mqtt_poll() from the VM's execute cycle.  The same code runs
// on Linux, in esp32sim and on ESP-IDF (lwIP sockets).
//
#ifndef SPRO_MQTT_H
#define SPRO_MQTT_H

#include <stdint.h>

// state
#define SPRO_MQTT_DISABLED    0
#define SPRO_MQTT_WAITING     1   // waiting before the next connect attempt
#define SPRO_MQTT_CONNECTING  2
#define SPRO_MQTT_CONNECTED   3

// last error
#define SPRO_MQTT_OK          0
#define SPRO_MQTT_E_CONFIG    1   // no host
#define SPRO_MQTT_E_DNS       2
#define SPRO_MQTT_E_CONNECT   3   // TCP connect failed (refused, unreachable)
#define SPRO_MQTT_E_TIMEOUT   4   // no CONNACK / no PINGRESP
#define SPRO_MQTT_E_PROTOCOL  5
#define SPRO_MQTT_E_AUTH      6   // CONNACK 4/5: bad user name or password, not authorized
#define SPRO_MQTT_E_REJECTED  7   // CONNACK 1-3
#define SPRO_MQTT_E_CLOSED    8   // broker closed the connection
#define SPRO_MQTT_E_NOMEM     9

// spro_mqtt_publish() result
#define SPRO_MQTT_PUB_OK      0
#define SPRO_MQTT_PUB_BUSY    1   // send buffer or QoS 1 window full, try again
#define SPRO_MQTT_PUB_OFFLINE 2
#define SPRO_MQTT_PUB_TOOBIG  3

// spro_mqtt_stat()
#define SPRO_MQTT_STAT_STATE    0
#define SPRO_MQTT_STAT_ERROR    1
#define SPRO_MQTT_STAT_TX       2   // PUBLISH sent
#define SPRO_MQTT_STAT_RX       3   // PUBLISH received
#define SPRO_MQTT_STAT_CONNECTS 4   // successful connects
#define SPRO_MQTT_STAT_INFLIGHT 5   // QoS 1 messages waiting for PUBACK
#define SPRO_MQTT_STAT_DROPPED  6   // received messages too big for the buffer

typedef struct spro_mqtt spro_mqtt_t;

spro_mqtt_t* spro_mqtt_new(int bufSize);
void spro_mqtt_free(spro_mqtt_t* m);

// configuration; takes effect on the next connect (spro_mqtt_restart() to apply now)
void spro_mqtt_server(spro_mqtt_t* m, const char* host, int port, int keepAliveSec);
void spro_mqtt_auth(spro_mqtt_t* m, const char* clientId, const char* user, const char* password);
// will: published by the broker if the connection is lost; online is published
// to the same topic after every connect (both retained, QoS 1); topic "" = none
void spro_mqtt_will(spro_mqtt_t* m, const char* topic, const char* offline, const char* online);
void spro_mqtt_enable(spro_mqtt_t* m, int enabled);
void spro_mqtt_restart(spro_mqtt_t* m);

// one step of the state machine: connect, send, receive, keep alive
void spro_mqtt_poll(spro_mqtt_t* m);

int spro_mqtt_publish(spro_mqtt_t* m, const char* topic, const uint8_t* payload, int len, int qos, int retain);

// subscriptions survive reconnects; returns a slot >= 0 or -1
int spro_mqtt_subscribe(spro_mqtt_t* m, const char* filter, int qos, int payloadMax);
void spro_mqtt_unsubscribe(spro_mqtt_t* m, int slot);
// copies the last message of the slot (truncated to cap, zero terminated if room);
// returns its sequence number (0 = nothing received yet), *len = bytes copied
int spro_mqtt_fetch(spro_mqtt_t* m, int slot, uint8_t* out, int cap, int* len);

int spro_mqtt_stat(spro_mqtt_t* m, int which);

// helpers (no client needed)
int spro_mqtt_topic_match(const char* filter, const char* topic);
void spro_mqtt_default_client_id(char* out, int cap);

#endif

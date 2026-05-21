/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#pragma once
#include <Arduino.h>

void  lora_init();
void  lora_update();

bool  lora_start_listen();
void  lora_stop_listen();
bool  lora_is_listening();

// Returns true once per received packet (clears flag), stores payload internally
bool  lora_poll_packet();
void  lora_get_last_packet(uint8_t* buf, int* len);

float lora_last_rssi();
float lora_last_snr();
int   lora_packet_count();

float lora_frequency();
float lora_bandwidth();
int   lora_spreading_factor();

bool  lora_set_frequency(float mhz);
bool  lora_set_bandwidth(float khz);
bool  lora_set_spreading_factor(int sf);

// Apply a Meshtastic channel preset by index (see modes.h MESH_CHANNELS[])
void  lora_apply_channel(int idx);

// Timestamp of last received packet (millis)
uint32_t lora_last_rx_ms();

// Re-queue the last packet so it can be read again by the active mode
void lora_requeue_packet();

// Read instantaneous RSSI at a given frequency (for scanning)
// Changes frequency temporarily; call lora_stop_listen() first if needed.
float lora_scan_rssi(float mhz);

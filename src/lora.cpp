#include "lora.h"
#include "modes.h"
#include "pins.h"
#include <SPI.h>
#include <RadioLib.h>

#if defined(HELTEC_T114)
static SPIClass loraSPI(NRF_SPIM2, PIN_LORA_MISO, PIN_LORA_SCK, PIN_LORA_MOSI);
static Module   mod(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, loraSPI);
#elif defined(HELTEC_V3)
static SPIClass loraSPI(FSPI);
static Module   mod(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, loraSPI);
#endif

static SX1262   radio(&mod);

static bool     listening   = false;
static volatile bool rxFlag = false;
static float    lastRSSI    = 0;
static float    lastSNR     = 0;
static int      packetCount = 0;
static float    curFreq     = 915.0f;
static float    curBW       = 125.0f;
static int      curSF       = 7;
static uint8_t  curCR       = 5;

static bool     packetReady = false;
static uint8_t  pktBuf[256];
static int      pktLen      = 0;
static uint32_t lastRxMs    = 0;

static void onReceive() {
    rxFlag = true;
}

void lora_init() {
#if defined(HELTEC_V3)
    // ESP32: begin() with pin args configures and initialises in one call
    loraSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, -1);
#else
    // nRF52840: pins set in constructor, begin() activates the peripheral
    loraSPI.begin();
#endif

    int state = radio.begin(curFreq, curBW, curSF, curCR,
                            RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                            10, 8, 1.8f, false);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("LoRa init failed: "); Serial.println(state);
        return;
    }
    radio.setDio2AsRfSwitch(true);
    radio.setDio1Action(onReceive);
    Serial.println("LoRa: SX1262 ready");
}

void lora_update() {
    if (!rxFlag) return;
    rxFlag = false;

    int state = radio.readData(pktBuf, sizeof(pktBuf));
    if (state == RADIOLIB_ERR_NONE) {
        pktLen      = radio.getPacketLength();
        lastRSSI    = radio.getRSSI();
        lastSNR     = radio.getSNR();
        lastRxMs    = millis();
        packetCount++;
        packetReady = true;

        Serial.print("LoRa RX: ");
        Serial.print(pktLen);
        Serial.print("B RSSI=");
        Serial.print(lastRSSI);
        Serial.print(" SNR=");
        Serial.println(lastSNR);
    }
    if (listening) radio.startReceive();
}

bool lora_poll_packet() {
    if (!packetReady) return false;
    packetReady = false;
    return true;
}

void lora_get_last_packet(uint8_t* buf, int* len) {
    *len = pktLen;
    memcpy(buf, pktBuf, pktLen);
}

bool lora_start_listen() {
    int state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) { listening = true; return true; }
    Serial.print("LoRa listen failed: "); Serial.println(state);
    return false;
}

void lora_stop_listen() {
    radio.standby();
    listening = false;
}

bool lora_is_listening()    { return listening; }
float lora_last_rssi()      { return lastRSSI; }
float lora_last_snr()       { return lastSNR; }
int   lora_packet_count()   { return packetCount; }
float lora_frequency()      { return curFreq; }
float lora_bandwidth()      { return curBW; }
int   lora_spreading_factor() { return curSF; }

bool lora_set_frequency(float mhz) {
    bool was = listening; if (was) radio.standby();
    int s = radio.setFrequency(mhz);
    if (s == RADIOLIB_ERR_NONE) curFreq = mhz;
    if (was) radio.startReceive();
    return s == RADIOLIB_ERR_NONE;
}

bool lora_set_bandwidth(float khz) {
    bool was = listening; if (was) radio.standby();
    int s = radio.setBandwidth(khz);
    if (s == RADIOLIB_ERR_NONE) curBW = khz;
    if (was) radio.startReceive();
    return s == RADIOLIB_ERR_NONE;
}

bool lora_set_spreading_factor(int sf) {
    if (sf < 5 || sf > 12) return false;
    bool was = listening; if (was) radio.standby();
    int s = radio.setSpreadingFactor(sf);
    if (s == RADIOLIB_ERR_NONE) curSF = sf;
    if (was) radio.startReceive();
    return s == RADIOLIB_ERR_NONE;
}

void lora_apply_channel(int idx) {
    if (idx < 0 || idx >= MESH_CHANNEL_COUNT) return;
    bool was = listening; if (was) radio.standby();
    const MeshChannel& ch = MESH_CHANNELS[idx];
    radio.setFrequency(ch.freqMHz);
    radio.setBandwidth(ch.bwKHz);
    radio.setSpreadingFactor(ch.sf);
    radio.setCodingRate(ch.cr);
    curFreq = ch.freqMHz;
    curBW   = ch.bwKHz;
    curSF   = ch.sf;
    curCR   = ch.cr;
    if (was) radio.startReceive();
}

uint32_t lora_last_rx_ms()  { return lastRxMs; }

void lora_requeue_packet() { packetReady = true; }

float lora_scan_rssi(float mhz) {
    bool was = listening;
    if (was) radio.standby();

    radio.setFrequency(mhz);
    radio.startReceive();
    delayMicroseconds(2500);
    float rssi = radio.getRSSI(false);  // instantaneous RSSI
    radio.standby();

    if (was) {
        // Restore original frequency and restart
        radio.setFrequency(curFreq);
        radio.setBandwidth(curBW);
        radio.setSpreadingFactor(curSF);
        radio.startReceive();
    }
    return rssi;
}

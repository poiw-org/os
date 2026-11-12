// Doora BLE bridge (ESP32-C3). A radio and nothing else: it advertises while the door is alive, takes
// one phone connection, and carries bytes between the phone and the door over UART. It holds no keys
// or secrets and decides nothing; the door creates every nonce and judges every request.
// docs/ble-protocol.md is the contract (sections 2, 3 and 6). Build with `make build ENV=bridge`.
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_task_wdt.h>
#include "../BridgeLink.h"
#include "../ChunkAssembler.h"

#ifndef BRIDGE_FIRMWARE_VERSION
#define BRIDGE_FIRMWARE_VERSION "dev"
#endif
#ifndef BRIDGE_RX
#define BRIDGE_RX 20  // from the door's BRIDGE_TX
#endif
#ifndef BRIDGE_TX
#define BRIDGE_TX 21  // to the door's BRIDGE_RX
#endif

namespace {
const char* const kServiceUuid = "7d0a0001-6f2b-4c1e-9a53-d00a00000001";
const char* const kCharUuid = "7d0a0002-6f2b-4c1e-9a53-d00a00000001";

constexpr uint32_t kDoorAliveMs = 15000;     // advertise only while the door pinged this recently
constexpr uint32_t kResultTimeoutMs = 10000;  // a connection without a RESULT is dropped
constexpr uint32_t kFlushMs = 250;           // lets the RESULT notification leave before disconnecting
constexpr uint16_t kWantedMtu = 247;
constexpr uint16_t kNotifyOverhead = 3;      // ATT header inside the MTU

HardwareSerial& uart = Serial1;
NimBLEServer* server = nullptr;
NimBLECharacteristic* characteristic = nullptr;
SemaphoreHandle_t lock;  // the BLE host task and loop() share the state below

Link::Decoder decoder;
ChunkAssembler assembler;

// Guarded by `lock`.
uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
uint8_t connNo = 0;           // the number the door knows this connection by; 0 = none
bool subscribed = false;
uint32_t connectedAtMs = 0;
uint32_t disconnectAtMs = 0;  // 0 = not scheduled
uint32_t lastPingMs = 0;
bool doorSeen = false;
uint8_t lastConnNo = 0;

struct Guard {
	Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
	~Guard() { xSemaphoreGive(lock); }
};

void sendFrame(uint8_t type, uint8_t conn, const uint8_t* payload = nullptr, size_t len = 0) {
	uint8_t out[Link::kMaxEncoded];
	size_t n = Link::encode(type, conn, payload, len, out, sizeof(out));
	if (n) uart.write(out, n);
}

void sendHello() {
	uint8_t payload[1 + 32];
	size_t v = strnlen(BRIDGE_FIRMWARE_VERSION, 32);
	payload[0] = Link::kVersion;
	memcpy(payload + 1, BRIDGE_FIRMWARE_VERSION, v);
	sendFrame(Link::kHello, 0, payload, 1 + v);
}

bool doorAlive(uint32_t ms) { return doorSeen && ms - lastPingMs < kDoorAliveMs; }

// Caller holds the lock.
void dropConnection() {
	if (connHandle != BLE_HS_CONN_HANDLE_NONE) server->disconnect(connHandle);
}

class ServerEvents : public NimBLEServerCallbacks {
	void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
		Guard g;
		connHandle = info.getConnHandle();
		if (++lastConnNo == 0) lastConnNo = 1;  // 1..255, wrapping, never 0
		connNo = lastConnNo;
		subscribed = false;
		disconnectAtMs = 0;
		connectedAtMs = millis();
		assembler.reset();
		Serial.printf("phone connected (conn %u)\n", connNo);
	}
	void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
		Guard g;
		if (connNo) sendFrame(Link::kGone, connNo);
		Serial.printf("phone gone (conn %u, reason %d)\n", connNo, reason);
		connHandle = BLE_HS_CONN_HANDLE_NONE;
		connNo = 0;
		subscribed = false;
		disconnectAtMs = 0;
		assembler.reset();
	}
};

class CharacteristicEvents : public NimBLECharacteristicCallbacks {
	void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
		Guard g;
		if (!connNo || subscribed || !(subValue & 1)) return;
		subscribed = true;
		sendFrame(Link::kConnected, connNo);
	}
	void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
		Guard g;
		if (!connNo || !subscribed) { dropConnection(); return; }  // the door has not asked for a request
		NimBLEAttValue v = c->getValue();
		switch (assembler.write(v.data(), v.size())) {
			case ChunkAssembler::Status::More: break;
			case ChunkAssembler::Status::Complete:
				sendFrame(Link::kRequest, connNo, assembler.body(), assembler.size());
				break;
			case ChunkAssembler::Status::Reject:
				Serial.println("bad write; disconnecting");
				dropConnection();
				break;
		}
	}
} characteristicEvents;
ServerEvents serverEvents;

// A frame from the door. Caller holds the lock.
void onDoorFrame(const Link::Frame& f, uint32_t ms) {
	switch (f.type) {
		case Link::kPing:
			if (!doorAlive(ms)) sendHello();  // the first ping after a gap
			lastPingMs = ms;
			doorSeen = true;
			sendFrame(Link::kPong, 0);
			break;
		case Link::kChallenge:
		case Link::kResult: {
			if (!connNo || f.conn != connNo || !subscribed) break;  // not about the live connection
			// A notification cannot be split: it has to fit the negotiated MTU.
			if (f.len + kNotifyOverhead > server->getPeerMTU(connHandle)) {
				Serial.println("MTU too small for the door's message; disconnecting");
				dropConnection();
				break;
			}
			characteristic->notify(f.payload, f.len, connHandle);
			if (f.type == Link::kResult) disconnectAtMs = ms + kFlushMs;
			break;
		}
		default:
			break;  // HELLO, PONG, CONNECTED, REQUEST, GONE only travel the other way
	}
}

void pollUart() {
	while (uart.available()) {
		uint32_t ms = millis();
		if (decoder.feed((uint8_t)uart.read())) {
			Guard g;
			onDoorFrame(decoder.frame(), ms);
		}
	}
}

void pollTimers() {
	uint32_t ms = millis();
	Guard g;
	if (connNo) {
		if (disconnectAtMs && (int32_t)(ms - disconnectAtMs) >= 0) { disconnectAtMs = 0; dropConnection(); }
		else if (ms - connectedAtMs >= kResultTimeoutMs && !disconnectAtMs) {
			Serial.println("no result in time; disconnecting");
			dropConnection();
		}
	}
	// Advertise only while the door can answer, so a phone never prompts for a dead door.
	NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
	bool want = !connNo && doorAlive(ms);
	if (want && !adv->isAdvertising()) adv->start();
	if (!want && adv->isAdvertising()) adv->stop();
}
}  // namespace

void setup() {
	Serial.begin(115200);  // the native USB serial: the console only, never the link
	lock = xSemaphoreCreateMutex();
	uart.setRxBufferSize(2048);
	uart.begin(115200, SERIAL_8N1, BRIDGE_RX, BRIDGE_TX);
	Serial.printf("\ndoora-bridge %s\n", BRIDGE_FIRMWARE_VERSION);

	NimBLEDevice::init("doora");
	NimBLEDevice::setMTU(kWantedMtu);
	server = NimBLEDevice::createServer();
	server->setCallbacks(&serverEvents);
	NimBLEService* service = server->createService(kServiceUuid);
	characteristic = service->createCharacteristic(kCharUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
	characteristic->setCallbacks(&characteristicEvents);
	service->start();

	// The service UUID goes in the advertising packet itself, since iOS background scans match only
	// UUIDs there; the name goes in the scan response.
	NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
	adv->addServiceUUID(kServiceUuid);
	NimBLEAdvertisementData scan;
	scan.setName("doora");
	adv->setScanResponseData(scan);

	esp_task_wdt_init(10, true);  // restart if loop() stalls
	esp_task_wdt_add(nullptr);
	sendHello();
}

void loop() {
	esp_task_wdt_reset();
	pollUart();
	pollTimers();
	delay(2);
}

#include "Bt.h"
#include <Arduino.h>
#include <esp32-hal-bt.h>
#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_spp_api.h>
#include <freertos/ringbuf.h>
#include <atomic>
#include "BtGate.h"
#include "Cloud.h"
#include "Log.h"

namespace {
constexpr size_t kRxBytes = 2048;   // holds one update chunk (Ota.cpp) with room to spare
constexpr size_t kTxBytes = 2048;
constexpr size_t kWriteMax = 512;   // one SPP write
constexpr int kMaxBonds = 6;        // the stack's bond table is small; strangers must not fill it

struct Event {
	enum Kind : uint8_t { Connected, Closed, Paired } kind;
	uint8_t address[6];
};

// Shared with the Bluetooth task.
QueueHandle_t gEvents = nullptr;
RingbufHandle_t gRx = nullptr, gTx = nullptr;
TaskHandle_t gPump = nullptr;
std::atomic<uint32_t> gHandle{0};
std::atomic<bool> gOpen{false};  // approved: data flows in both directions
std::atomic<bool> gCongested{false};
char gName[48];

// Owned by the loop task.
BtGate gGate;
char gRequestId[17] = {0};
bool gAsking = false;

void post(Event::Kind kind, const uint8_t* address) {
	Event e{kind, {0}};
	if (address) memcpy(e.address, address, 6);
	if (xQueueSend(gEvents, &e, 0) != pdTRUE && gHandle.load()) esp_spp_disconnect(gHandle.load());  // never lose track
}

void onGap(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
	switch (event) {
		case ESP_BT_GAP_CFM_REQ_EVT: esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true); break;  // no screen to compare on
		case ESP_BT_GAP_PIN_REQ_EVT: esp_bt_gap_pin_reply(param->pin_req.bda, false, 0, nullptr); break;  // no legacy pairing
		case ESP_BT_GAP_AUTH_CMPL_EVT:
			if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) post(Event::Paired, param->auth_cmpl.bda);
			break;
		default: break;
	}
}

void onSpp(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
	switch (event) {
		case ESP_SPP_INIT_EVT:
			esp_bt_dev_set_device_name(gName);
			esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
			esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE | ESP_SPP_SEC_ENCRYPT, ESP_SPP_ROLE_SLAVE, 0, "doora");
			break;
		case ESP_SPP_SRV_OPEN_EVT:
			if (param->srv_open.status != ESP_SPP_SUCCESS) break;
			if (gHandle.load()) {  // one console at a time
				esp_spp_disconnect(param->srv_open.handle);
				break;
			}
			gHandle.store(param->srv_open.handle);
			post(Event::Connected, param->srv_open.rem_bda);
			break;
		case ESP_SPP_CLOSE_EVT:
			if (param->close.handle != gHandle.load()) break;
			gOpen.store(false);
			gHandle.store(0);
			gCongested.store(false);
			post(Event::Closed, nullptr);
			break;
		case ESP_SPP_DATA_IND_EVT:
			if (gOpen.load()) xRingbufferSend(gRx, param->data_ind.data, param->data_ind.len, 0);
			break;
		case ESP_SPP_CONG_EVT: gCongested.store(param->cong.cong); break;
		case ESP_SPP_WRITE_EVT:
			gCongested.store(param->write.cong);
			xTaskNotifyGive(gPump);
			break;
		default: break;
	}
}

// Moves queued output onto the link. Anything queued while no approved link is open is thrown away,
// so one client's log lines never reach the next.
void pump(void*) {
	for (;;) {
		size_t n = 0;
		auto* item = static_cast<uint8_t*>(xRingbufferReceiveUpTo(gTx, &n, portMAX_DELAY, kWriteMax));
		if (!item) continue;
		while (gOpen.load() && gCongested.load()) vTaskDelay(pdMS_TO_TICKS(5));
		uint32_t handle = gHandle.load();
		if (gOpen.load() && handle) {
			ulTaskNotifyTake(pdTRUE, 0);
			if (esp_spp_write(handle, (int)n, item) == ESP_OK) ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
		}
		vRingbufferReturnItem(gTx, item);
	}
}

// A line for a client that has not been approved; it bypasses the queue, which only approved links read.
void notice(const char* text) {
	uint32_t handle = gHandle.load();
	if (handle) esp_spp_write(handle, (int)strlen(text), (uint8_t*)text);
}

void formatAddress(const uint8_t* a, char out[18]) {
	snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a[0], a[1], a[2], a[3], a[4], a[5]);
}

void finish(Cloud& cloud, BtOutcome outcome) {
	if (!gAsking) return;
	gAsking = false;
	cloud.endBluetoothRequest(outcome);
}

void openLink() {
	gOpen.store(true);
	LOG("Bluetooth: link open.");
}

void drop() {
	uint32_t handle = gHandle.load();
	gOpen.store(false);
	if (handle) esp_spp_disconnect(handle);
}

// Bonds of devices nobody approved are only removed once the table is nearly full, so someone who
// was merely too slow can retry without having to forget the door on their device.
void evictBonds(uint32_t ms) {
	int count = esp_bt_gap_get_bond_device_num();
	if (count < kMaxBonds) return;
	esp_bd_addr_t list[16];
	count = 16;
	if (esp_bt_gap_get_bond_device_list(&count, list) != ESP_OK) return;
	for (int i = 0; i < count; i++)
		if (!gGate.granted(list[i], ms)) esp_bt_gap_remove_bond_device(list[i]);
}

void refuse(Cloud& cloud, BtOutcome outcome, const char* why, uint32_t ms) {
	finish(cloud, outcome);
	LOG("Bluetooth: %s; dropping the connection.", why);
	notice("Not approved; disconnecting.\r\n");
	drop();
	gGate.closed();
	evictBonds(ms);
}

void onConnected(Cloud& cloud, const uint8_t* address, uint32_t ms) {
	char text[18];
	formatAddress(address, text);
	gGate.connected(address, ms);
	if (gGate.link() == BtGate::Link::Open) {
		LOG("Bluetooth: %s is still authorized.", text);
		openLink();
		return;
	}
	snprintf(gRequestId, sizeof(gRequestId), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
	gAsking = true;
	cloud.requestBluetooth(gRequestId, text, gGate.deadline());
	LOG("Bluetooth: %s wants to connect; waiting for an admin.", text);
	notice("Waiting for an admin to approve this connection (20 s).\r\n");
}

void onClosed(Cloud& cloud) {
	bool wasOpen = gGate.link() == BtGate::Link::Open;
	finish(cloud, BtOutcome::Gone);
	gGate.closed();
	while (Bt::read() >= 0) {}
	if (wasOpen) LOG("Bluetooth: link closed.");
}
}  // namespace

namespace Bt {

void begin(const char* name) {
	// Referencing the Arduino Bluetooth layer also links its btInUse(); without that, Arduino's
	// startup hands the controller's memory back to the heap and Bluetooth can never start.
	if (btStarted()) return;
	strlcpy(gName, name, sizeof(gName));
	uint32_t before = ESP.getFreeHeap();

	gEvents = xQueueCreate(8, sizeof(Event));
	gRx = xRingbufferCreate(kRxBytes, RINGBUF_TYPE_BYTEBUF);
	gTx = xRingbufferCreate(kTxBytes, RINGBUF_TYPE_BYTEBUF);
	bool ok = gEvents && gRx && gTx &&
	          xTaskCreatePinnedToCore(&pump, "bt-tx", 2560, nullptr, 1, &gPump, 0) == pdPASS;

	esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	cfg.mode = ESP_BT_MODE_CLASSIC_BT;
	ok = ok && esp_bt_controller_mem_release(ESP_BT_MODE_BLE) == ESP_OK && esp_bt_controller_init(&cfg) == ESP_OK &&
	     esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) == ESP_OK && esp_bluedroid_init() == ESP_OK &&
	     esp_bluedroid_enable() == ESP_OK && esp_bt_gap_register_callback(&onGap) == ESP_OK &&
	     esp_spp_register_callback(&onSpp) == ESP_OK;
	if (ok) {
		esp_bt_io_cap_t noScreen = ESP_BT_IO_CAP_NONE;
		ok = esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &noScreen, sizeof(noScreen)) == ESP_OK &&
		     esp_spp_init(ESP_SPP_MODE_CB) == ESP_OK;
	}
	if (!ok) {
		LOG("Bluetooth: could not start; the console stays on USB only.");
		return;
	}
	LOG("Bluetooth: listening as \"%s\" (uses %ld bytes of heap).", gName, (long)before - (long)ESP.getFreeHeap());
}

void service(Cloud& cloud) {
	if (!gEvents) return;
	uint32_t ms = millis();
	Event e;
	while (xQueueReceive(gEvents, &e, 0) == pdTRUE) {
		switch (e.kind) {
			case Event::Paired: gGate.forget(e.address); break;
			case Event::Connected: onConnected(cloud, e.address, ms); break;
			case Event::Closed: onClosed(cloud); break;
		}
	}
	if (gAsking) {
		BtDecision decision = cloud.takeBluetoothDecision();
		if (decision == BtDecision::Approve && gGate.approve(ms)) {
			finish(cloud, BtOutcome::Approved);
			LOG("Bluetooth: approved by an admin for 24 hours.");
			openLink();
		} else if (decision == BtDecision::Deny) {
			refuse(cloud, BtOutcome::Denied, "denied by an admin", ms);
		}
	}
	if (gGate.expired(ms)) refuse(cloud, BtOutcome::Expired, gAsking ? "no approval in time" : "the 24 hours are up", ms);
}

int available() {
	if (!gRx) return 0;
	UBaseType_t waiting = 0;
	vRingbufferGetInfo(gRx, nullptr, nullptr, nullptr, nullptr, &waiting);
	return (int)waiting;
}

int read() {
	if (!gRx) return -1;
	size_t n = 0;
	auto* item = static_cast<uint8_t*>(xRingbufferReceiveUpTo(gRx, &n, 0, 1));
	if (!item) return -1;
	int c = *item;
	vRingbufferReturnItem(gRx, item);
	return c;
}

void write(const uint8_t* data, size_t len) {
	if (gOpen.load()) xRingbufferSend(gTx, data, len, 0);
}

void printStatus() {
	static const char* const kLink[] = {"no device", "waiting for approval", "open"};
	LOG("bluetooth : \"%s\", %s", gName, gEvents ? kLink[(int)gGate.link()] : "off");
}

}  // namespace Bt

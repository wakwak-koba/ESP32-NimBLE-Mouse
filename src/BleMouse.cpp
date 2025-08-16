#include "BleMouse.h"

#include <NimBLEDevice.h>

#include "sdkconfig.h"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char *LOG_TAG = "NimBLEDevice";
#endif

static const uint8_t _hidReportDescriptor[] = {
    USAGE_PAGE(1), 0x01,  // USAGE_PAGE (Generic Desktop)
    USAGE(1), 0x02,       // USAGE (Mouse)
    COLLECTION(1), 0x01,  // COLLECTION (Application)
    USAGE(1), 0x01,       //   USAGE (Pointer)
    COLLECTION(1), 0x00,  //   COLLECTION (Physical)
    // ------------------------------------------------- Buttons (Left, Right, Middle, Back, Forward)
    USAGE_PAGE(1), 0x09,       //     USAGE_PAGE (Button)
    USAGE_MINIMUM(1), 0x01,    //     USAGE_MINIMUM (Button 1)
    USAGE_MAXIMUM(1), 0x05,    //     USAGE_MAXIMUM (Button 5)
    LOGICAL_MINIMUM(1), 0x00,  //     LOGICAL_MINIMUM (0)
    LOGICAL_MAXIMUM(1), 0x01,  //     LOGICAL_MAXIMUM (1)
    REPORT_SIZE(1), 0x01,      //     REPORT_SIZE (1)
    REPORT_COUNT(1), 0x05,     //     REPORT_COUNT (5)
    HIDINPUT(1), 0x02,         //     INPUT (Data, Variable, Absolute) ;5 button bits
    // ------------------------------------------------- Padding
    REPORT_SIZE(1), 0x03,   //     REPORT_SIZE (3)
    REPORT_COUNT(1), 0x01,  //     REPORT_COUNT (1)
    HIDINPUT(1), 0x03,      //     INPUT (Constant, Variable, Absolute) ;3 bit padding
    // ------------------------------------------------- X/Y position, Wheel
    USAGE_PAGE(1), 0x01,       //     USAGE_PAGE (Generic Desktop)
    USAGE(1), 0x30,            //     USAGE (X)
    USAGE(1), 0x31,            //     USAGE (Y)
    USAGE(1), 0x38,            //     USAGE (Wheel)
    LOGICAL_MINIMUM(1), 0x81,  //     LOGICAL_MINIMUM (-127)
    LOGICAL_MAXIMUM(1), 0x7f,  //     LOGICAL_MAXIMUM (127)
    REPORT_SIZE(1), 0x08,      //     REPORT_SIZE (8)
    REPORT_COUNT(1), 0x03,     //     REPORT_COUNT (3)
    HIDINPUT(1), 0x06,         //     INPUT (Data, Variable, Relative) ;3 bytes (X,Y,Wheel)
    // ------------------------------------------------- Horizontal wheel
    USAGE_PAGE(1), 0x0c,       //     USAGE PAGE (Consumer Devices)
    USAGE(2), 0x38, 0x02,      //     USAGE (AC Pan)
    LOGICAL_MINIMUM(1), 0x81,  //     LOGICAL_MINIMUM (-127)
    LOGICAL_MAXIMUM(1), 0x7f,  //     LOGICAL_MAXIMUM (127)
    REPORT_SIZE(1), 0x08,      //     REPORT_SIZE (8)
    REPORT_COUNT(1), 0x01,     //     REPORT_COUNT (1)
    HIDINPUT(1), 0x06,         //     INPUT (Data, Var, Rel)
    END_COLLECTION(0),         //   END_COLLECTION
    END_COLLECTION(0)          // END_COLLECTION
};

BleMouse::BleMouse(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel)
    : _buttons(0), hid(0) {
    this->deviceName         = deviceName;
    this->deviceManufacturer = deviceManufacturer;
    this->batteryLevel       = batteryLevel;
}

void BleMouse::begin(void) {
    NimBLEDevice::init(deviceName);

    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(this);

    hid        = new NimBLEHIDDevice(pServer);
    inputMouse = hid->inputReport(0);  // <-- input REPORTID from report map

    hid->manufacturer()->setValue(deviceManufacturer);

    hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    hid->hidInfo(0x00, 0x02);

    BLESecurity *pSecurity = new NimBLESecurity();

    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

    hid->reportMap((uint8_t *)_hidReportDescriptor, sizeof(_hidReportDescriptor));
    hid->startServices();

    NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(HID_MOUSE);
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    pAdvertising->start();
    hid->setBatteryLevel(batteryLevel);
}

void BleMouse::end(void) {
}

void BleMouse::click(uint8_t b) {
    _buttons = b;
    move(0, 0, 0, 0);
    _buttons = 0;
    move(0, 0, 0, 0);
}

void BleMouse::move(signed char x, signed char y, signed char wheel, signed char hWheel) {
    if (this->isConnected()) {
        uint8_t m[5];
        m[0] = _buttons;
        m[1] = x;
        m[2] = y;
        m[3] = wheel;
        m[4] = hWheel;
        this->inputMouse->setValue(m, 5);
        this->inputMouse->notify();
    }
}

void BleMouse::buttons(uint8_t b) {
    if (b != _buttons) {
        _buttons = b;
        move(0, 0, 0, 0);
    }
}

void BleMouse::press(uint8_t b) {
    buttons(_buttons | b);
}

void BleMouse::release(uint8_t b) {
    buttons(_buttons & ~b);
}

bool BleMouse::isPressed(uint8_t b) const {
    if ((b & _buttons) > 0)
        return true;
    return false;
}

bool BleMouse::isConnected(void) const {
    return connected;
}

void BleMouse::setBatteryLevel(uint8_t level) {
    this->batteryLevel = level;
    if (hid != 0)
        this->hid->setBatteryLevel(this->batteryLevel);
}

void BleMouse::onConnect(NimBLEServer *pServer) {
    connected = true;
    if (connectCallback) connectCallback();
}

void BleMouse::onDisconnect(NimBLEServer *pServer) {
    connected = false;
    if (disconnectCallback) disconnectCallback();
}

void BleMouse::onConnect(Callback cb) {
    connectCallback = cb;
}

void BleMouse::onDisconnect(Callback cb) {
    disconnectCallback = cb;
}
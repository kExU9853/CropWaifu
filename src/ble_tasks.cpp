#include <ble_tasks.h>

// BLE 全局变量
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
extern SemaphoreHandle_t bleUpdateSignal;
bool deviceConnected = false;
CropWaifuSensors cropWaifuSensors = CropWaifuSensors();

// 回调类：处理连接状态
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
  }
};

// BLE 初始化函数
canwaifu_status cropwaifu_ble_init() {
    Serial.println("[BLE.] Initializing BLE...");
  
    BLEDevice::init("CropWaifu"); // 初始化 BLE 设备名称
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_NOTIFY
                    );

    pCharacteristic->setValue("114514");
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
    Serial.println("[BLE.] Broadcasting BLE service...");
    return CANWAIFU_OK; // 返回成功状态
}

// 打包BLE通知数据，返回20字节数组
void pack_ble_notify_data(uint8_t* notifyData, const CropWaifuSensors& sensors) {
    memset(notifyData, 0, 20);
    // BoardID
    notifyData[0] = (uint8_t)BOARD_ID;
    // 温度（float转int16，*100）
    int16_t temp = (int16_t)(sensors.temperature * 100);
    notifyData[1] = temp & 0xFF;
    notifyData[2] = (temp >> 8) & 0xFF;
    // 光照强度
    uint16_t light = sensors.lightIntensity;
    notifyData[3] = light & 0xFF;
    notifyData[4] = (light >> 8) & 0xFF;
    // 风扇转速
    uint16_t fan = sensors.fanSpeedRPM;
    notifyData[5] = fan & 0xFF;
    notifyData[6] = (fan >> 8) & 0xFF;
    // 7-19字节保留为0
}

// BLE 任务函数
void ble_task(void* pvParameters) {
    Serial.println("[BLE.] BLE Task started");
    uint8_t notifyData[20] = {0};
    TickType_t lastWakeTime = xTaskGetTickCount();
    while (true) {
        if (deviceConnected) {
            // 1. 非阻塞检查信号，有信号就立即notify（可多次处理）
            while (xSemaphoreTake(bleUpdateSignal, 0) == pdTRUE) {
                pack_ble_notify_data(notifyData, cropWaifuSensors);
                pCharacteristic->setValue(notifyData, 20);
                pCharacteristic->notify();
                Serial.println("[BLE.] Notification sent (signal)");
            }
            // 2. 定时notify
            pack_ble_notify_data(notifyData, cropWaifuSensors);
            pCharacteristic->setValue(notifyData, 20);
            pCharacteristic->notify();
            Serial.println("[BLE.] Notification sent (periodic)");
        }
        vTaskDelayUntil(&lastWakeTime, 500 / portTICK_PERIOD_MS);
    }
}

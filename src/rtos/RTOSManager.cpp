#include "RTOSManager.h"
#include <Arduino.h>
#include <cstdlib>
#include <esp_idf_version.h>

RTOSManager::RTOSManager() {}

bool RTOSManager::begin() {
    initialized = true;
    Serial.println("RTOSManager: Initialisation OK");
    return initialized;
}

bool RTOSManager::createTask(const char* name, void (*taskFunc)(void*), uint16_t stackSize, void* params, UBaseType_t priority) {
    BaseType_t res = xTaskCreate(taskFunc, name, stackSize, params, priority, nullptr);
    if (res != pdPASS) {
        Serial.printf("RTOSManager: Échec création tâche %s\n", name);
        return false;
    }
    Serial.printf("RTOSManager: Tâche %s créée\n", name);
    return true;
}

void RTOSManager::startScheduler() {
    Serial.println("RTOSManager: Scheduler FreeRTOS démarré");
    // Scheduler déjà géré par ESP32
}

void RTOSManager::auditTasks() {
    Serial.println("RTOSManager: Audit des tâches en cours...");
    TaskStatus_t* pxTaskStatusArray;
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    pxTaskStatusArray = (TaskStatus_t*)malloc(uxArraySize * sizeof(TaskStatus_t));
    if (pxTaskStatusArray != nullptr) {
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, nullptr);
        for (UBaseType_t i = 0; i < uxArraySize; i++) {
            Serial.printf("Tâche: %s, Etat: %d, Priorité: %d\n", pxTaskStatusArray[i].pcTaskName, pxTaskStatusArray[i].eCurrentState, pxTaskStatusArray[i].uxCurrentPriority);
        }
        free(pxTaskStatusArray);
    }
}

void RTOSManager::logStatus() {
    Serial.printf("RTOSManager: init=%s, watchdog=%s, timeout=%lu ms\n", initialized ? "true" : "false", watchdogEnabled ? "true" : "false", watchdogTimeout);
}

void RTOSManager::enableWatchdog(uint32_t timeoutMs) {
#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t config = {
        .timeout_ms = timeoutMs,
        .idle_core_mask = static_cast<uint32_t>((1U << portNUM_PROCESSORS) - 1U),
        .trigger_panic = true,
    };
    esp_task_wdt_init(&config);
#else
    esp_task_wdt_init(timeoutMs / 1000, true);
#endif
    esp_task_wdt_add(nullptr);
    watchdogEnabled = true;
    watchdogTimeout = timeoutMs;
    Serial.printf("RTOSManager: Watchdog activé (%lu ms)\n", watchdogTimeout);
}

void RTOSManager::feedWatchdog() {
    if (watchdogEnabled) {
        esp_task_wdt_reset();
        Serial.println("RTOSManager: Watchdog feed");
    }
}

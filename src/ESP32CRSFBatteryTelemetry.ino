/**
 * ESP32CRSFBatteryTelemetry is working sample code, repository at: <https://github.com/ivan-vuela/esp32-crsf-diy-battery-telemetry>
 * Copyright (c) 2026 Ivan Bustamante Filoza
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <AlfredoCRSF.h>

SemaphoreHandle_t crsfMutex = NULL;

HardwareSerial crsfSerial(1);
AlfredoCRSF crsf;

void sendBattTelemetry(void *pvParameters);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // CRSF setup
  Serial.print("Initializing CRSF...");
  crsfSerial.begin(CRSF_BAUDRATE, SERIAL_8N1, D0, D1);
  if (!crsfSerial) {
    throw std::runtime_error("Failed to connect to CRSF receiver!");
  }
  crsf.begin(crsfSerial);
  Serial.println("CRSF initialized!");

  // Mutex for thread-safe access to CRSF object
  crsfMutex = xSemaphoreCreateMutex();

  // Task that handles battery telemetry in parallel
  xTaskCreate(sendBattTelemetry, "Battery Telemetry Task", 2048, NULL, 2, NULL);
}

void loop() {
  if (xSemaphoreTake(crsfMutex, portMAX_DELAY) == pdTRUE) {
    crsf.update();
    xSemaphoreGive(crsfMutex);
  }

  // 1-tick delay to avoid thread starvation
  vTaskDelay(1);
}

/**
 * Sends battery voltage telemetry over CRSF.
 * Intended for use as a FreeRTOS task.
 * @param pvParameters
 */
void sendBattTelemetry(void *pvParameters) {
  while (true) {
    int voltageSignal = analogRead(A2);
    float voltage = voltageSignal / 226.12;

    Serial.print("Voltage: ");
    Serial.print(voltageSignal);
    Serial.print(", ");
    Serial.println(voltage);

    if (xSemaphoreTake(crsfMutex, portMAX_DELAY) == pdTRUE) {
      if (crsf.isLinkUp()) {
        crsf_sensor_battery_t crsfBatt = { 0 };
        crsfBatt.voltage = htobe16((uint16_t)(voltage * 10.0));
        crsf.queuePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_BATTERY_SENSOR, &crsfBatt, sizeof(crsfBatt));;
      }
      xSemaphoreGive(crsfMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

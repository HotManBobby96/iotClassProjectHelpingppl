#include <DHT11.h>
#include "Zigbee.h"

// Create an instance of the DHT11 class.
#define DHT_PIN 2  // GPIO2 on the ESP32-C6
DHT11 dht11(DHT_PIN);  // No need for dht11.begin()

// Zigbee temperature sensor configuration
#define TEMP_SENSOR_ENDPOINT_NUMBER 10
ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);

// Optional Time cluster variables
struct tm timeinfo;
struct tm *localTime;
int32_t timezone;

void setup() {
    // Initialize serial communication for debugging and data readout.
    Serial.begin(9600);

    // Optional: Set Zigbee device name and model
    zbTempSensor.setManufacturerAndModel("Espressif", "ZigbeeTempSensor");

    // Set minimum and maximum temperature measurement range (e.g., 10-50°C)
    zbTempSensor.setMinMaxValue(10, 50);

    // Optional: Set tolerance for temperature measurement in °C
    zbTempSensor.setTolerance(1);

    // Add endpoint to Zigbee Core
    Zigbee.addEndpoint(&zbTempSensor);

    // Start Zigbee in End Device mode
    if (!Zigbee.begin()) {
        Serial.println("Zigbee failed to start!");
        Serial.println("Rebooting...");
        ESP.restart();
    } else {
        Serial.println("Zigbee started successfully!");
    }
    Serial.println("Connecting to network");
    while (!Zigbee.connected()) {
        Serial.print(".");
        delay(100);
    }
    Serial.println();

    // Optional: Time cluster configuration (time from coordinator)
    timeinfo = zbTempSensor.getTime();
    timezone = zbTempSensor.getTimezone();

    Serial.println("UTC time:");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

    time_t local = mktime(&timeinfo) + timezone;
    localTime = localtime(&local);

    Serial.println("Local time with timezone:");
    Serial.println(localTime, "%A, %B %d %Y %H:%M:%S");

    // Start temperature sensor reading task
    xTaskCreate(temp_sensor_value_update, "temp_sensor_update", 2048, NULL, 10, NULL);

    // Set reporting interval for temperature measurement
    zbTempSensor.setReporting(1, 0, 1);  // Report every 1 second or if temp changes ≥ 1°C
}

// Task to read temperature and send it via Zigbee
static void temp_sensor_value_update(void *arg) {
    for (;;) {
        int temperature = 0;
        int humidity = 0;

        // Read temperature and humidity from the DHT11 sensor
        int result = dht11.readTemperatureHumidity(temperature, humidity);

        if (result == 0) {
            // Update the Zigbee temperature sensor value
            zbTempSensor.setTemperature(temperature);

            // Print the temperature and humidity to Serial
            Serial.print("Temperature: ");
            Serial.print(temperature);
            Serial.print(" °C\tHumidity: ");
            Serial.print(humidity);
            Serial.println(" %");

        } else {
            // Print error message if the reading fails
            Serial.println(DHT11::getErrorString(result));
        }

        // Delay before next reading
        delay(1000);  // Update every second
    }
}

void loop() {
    // Checking for button press to reset Zigbee
    int button = BOOT_PIN;  // Assuming BOOT_PIN is connected to a button for reset
    if (digitalRead(button) == LOW) {  // Button pressed
        delay(100);
        int startTime = millis();
        while (digitalRead(button) == LOW) {
            delay(50);
            if ((millis() - startTime) > 3000) {
                // If button is pressed for more than 3 seconds, reset Zigbee
                Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
                delay(1000);
                Zigbee.factoryReset();
            }
        }
    }

    delay(100);
}

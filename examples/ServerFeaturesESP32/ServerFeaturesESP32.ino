#include <ezTime2.h>
#include <WiFi.h>

#if !defined(ESP32)
	#warning "ServerFeaturesESP32 is intended for ESP32-class boards."
#endif

static void printList(const String &listName) {
#ifdef EZTIME_SERVER_LIST_ENABLE
	String listData;
	Serial.print(F("\nLIST "));
	Serial.print(listName);
	Serial.println(F(":"));

	if (!ezt::listTimezones(listName, listData)) {
		Serial.print(F("  ERROR: "));
		Serial.println(errorString());
		return;
	}

	uint16_t count = ezt::listLength(listData);
	Serial.print(F("  Entries: "));
	Serial.println(count);

	for (uint16_t i = 0; i < count; i++) {
		String name;
		uint16_t childCount = 0;
		if (ezt::listItem(listData, i, name, childCount)) {
			Serial.print(F("  - "));
			Serial.print(name);
			Serial.print(F(" (subregions: "));
			Serial.print(childCount);
			Serial.println(F(")"));
		}
	}
#else
	(void)listName;
	Serial.println(F("LIST support is disabled in this build."));
#endif
}

void setup() {
	Serial.begin(115200);
	while (!Serial) { ; }

	WiFi.begin("your-ssid", "your-password");

	// Uncomment for protocol-level debug output.
	// setDebug(INFO);

	waitForSync();

	Serial.println();
	Serial.println(F("Server features demo"));
	Serial.println(F("--------------------"));
	Serial.print(F("UTC:             "));
	Serial.println(UTC.dateTime());

	String publicIP;
	if (ezt::getPublicIP(publicIP)) {
		Serial.print(F("Public IP:       "));
		Serial.println(publicIP);
	} else {
		Serial.print(F("Public IP:       "));
		Serial.println(errorString());
	}

	delay(3500);

	Timezone localTZ;
	localTZ.setGeoLookupMode(GEOIP_LOOKUP_WITH_EXT_FALLBACK);

	Serial.print(F("Local (GeoIP):   "));
	if (localTZ.setLocation()) {
		Serial.println(localTZ.dateTime());
	} else {
		Serial.println(errorString());
	}

	delay(3500);

	Serial.print(F("Local (EXT):     "));
	if (localTZ.setLocation(F("EXT_GEOIP"))) {
		Serial.println(localTZ.dateTime());
	} else {
		Serial.println(errorString());
	}

#ifdef EZTIME_SERVER_LIST_ENABLE
	delay(1500);
	printList(F("regions"));
	delay(1500);
	printList(F("america"));
#else
	Serial.println(F("\nLIST support not compiled in. Enable EZTIME_SERVER_LIST_ENABLE if needed."));
#endif
}

void loop() {
	events();
}

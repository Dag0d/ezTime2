#include <ezTime2.h>
#include <WiFi.h>

#ifndef EZTIME2_EXAMPLE_WIFI_TIMEOUT_MS
	#define EZTIME2_EXAMPLE_WIFI_TIMEOUT_MS 30000UL
#endif

#ifdef EZTIME_SERVER_LIST_ENABLE
static bool findListEntry(const String &listData, const String &targetName, uint16_t &childCount) {
	uint16_t count = ezt::listLength(listData);
	for (uint16_t i = 0; i < count; i++) {
		String name;
		if (ezt::listItem(listData, i, name, childCount) && name.equalsIgnoreCase(targetName)) {
			return true;
		}
	}
	return false;
}

static void printTimezoneList(const String &listName) {
	String listData;
	Serial.print(F("LIST "));
	Serial.print(listName);
	Serial.println(F(":"));

	if (!ezt::listTimezones(listName, listData)) {
		Serial.print(F("  ERROR: "));
		Serial.println(errorString());
		return;
	}

	uint16_t count = ezt::listLength(listData);
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
}

static void demoOlsonBuilder() {
	String regions;
	if (!ezt::listTimezones(F("regions"), regions)) {
		Serial.print(F("LIST regions failed: "));
		Serial.println(errorString());
		return;
	}

	delay(1500);

	uint16_t childCount = 0;
	if (findListEntry(regions, F("Europe"), childCount)) {
		String europe;
		if (ezt::listTimezones(F("europe"), europe)) {
			delay(1500);

			if (findListEntry(europe, F("Berlin"), childCount)) {
				Serial.print(F("Built Olson:     "));
				Serial.println(F("Europe/Berlin"));
			}
		}
	}

	String america;
	if (!ezt::listTimezones(F("america"), america)) {
		return;
	}

	delay(1500);

	if (findListEntry(america, F("Argentina"), childCount) && childCount > 0) {
		String argentina;
		if (ezt::listTimezones(F("argentina"), argentina)) {
			delay(1500);

			if (findListEntry(argentina, F("Buenos_Aires"), childCount)) {
				Serial.print(F("Built Olson:     "));
				Serial.println(F("America/Argentina/Buenos_Aires"));
			}
		}
	}
}
#endif

void setup() {
	Serial.begin(115200);
	while (!Serial) { ; }		// wait for Serial port to connect. Needed for native USB port only

	Serial.println();
	Serial.println(F("ezTime2 Timezones example"));
	Serial.println(F("========================"));
	Serial.println(F("Starting WiFi connection..."));

	WiFi.begin("your-ssid", "your-password");
	unsigned long wifiStart = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < EZTIME2_EXAMPLE_WIFI_TIMEOUT_MS) {
		delay(500);
		Serial.print(F("."));
	}
	Serial.println();

	if (WiFi.status() == WL_CONNECTED) {
		Serial.println(F("WiFi connected."));
		Serial.print(F("Local IP:         "));
		Serial.println(WiFi.localIP());
	} else {
		Serial.println(F("WiFi connection failed or timed out."));
		Serial.println(F("Check SSID/password before expecting server responses."));
		return;
	}

	// Uncomment the line below to see what it does behind the scenes
	// setDebug(INFO);

	Serial.println(F("Waiting for NTP sync..."));
	if (!waitForSync(30)) {
		Serial.print(F("waitForSync failed: "));
		Serial.println(errorString());
		return;
	}
	Serial.println(F("Time sync complete."));

	Serial.println();
	Serial.println("UTC:             " + UTC.dateTime());

	Timezone myTZ;
	myTZ.setGeoLookupMode(GEOIP_LOOKUP_WITH_EXT_FALLBACK);

	String publicIP;
	if (ezt::getPublicIP(publicIP)) {
		Serial.print(F("Public IP:       "));
		Serial.println(publicIP);
	} else {
		Serial.print(F("Public IP:       "));
		Serial.println(errorString());
	}

	delay(3500);

	// Provide official timezone names
	// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
	Serial.println(F("Requesting Pacific/Auckland..."));
	if (!myTZ.setLocation(F("Pacific/Auckland"))) {
		Serial.print(F("Pacific/Auckland failed: "));
		Serial.println(errorString());
	}
	Serial.print(F("New Zealand:     "));
	Serial.println(myTZ.dateTime());

	// Wait a little bit to not trigger DDoS protection on server
	// See https://github.com/Dag0d/ezTime2#timezonedcircuitfloweu
	delay(3500);

	// Or country codes for countries that do not span multiple timezones
	Serial.println(F("Requesting country code DE..."));
	if (!myTZ.setLocation(F("de"))) {
		Serial.print(F("DE lookup failed: "));
		Serial.println(errorString());
	}
	Serial.print(F("Germany:         "));
	Serial.println(myTZ.dateTime());

	// Same as above
	delay(3500);

	// GeoIP stays the default, but with EXT_GEOIP fallback enabled above.
	Serial.print(F("Local (GeoIP):   "));
	if (myTZ.setLocation()) {
		Serial.println(myTZ.dateTime());
	} else {
		Serial.println(errorString());
	}

	#ifdef EZTIME_SERVER_LIST_ENABLE
		delay(1500);
		printTimezoneList(F("regions"));
		delay(1500);
		printTimezoneList(F("europe"));
		delay(1500);
		printTimezoneList(F("america"));
		delay(1500);
		printTimezoneList(F("argentina"));
		delay(1500);
		demoOlsonBuilder();
	#endif

}

void loop() {
	events();
}

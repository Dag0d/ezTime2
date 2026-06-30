#include <ezTime2.h>
#include <WiFi.h>

#ifdef EZTIME_SERVER_LIST_ENABLE
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
#endif

void setup() {

	Serial.begin(115200);
	while (!Serial) { ; }		// wait for Serial port to connect. Needed for native USB port only
	WiFi.begin("your-ssid", "your-password");

	// Uncomment the line below to see what it does behind the scenes
	// setDebug(INFO);
	
	waitForSync();

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
	myTZ.setLocation(F("Pacific/Auckland"));
	Serial.print(F("New Zealand:     "));
	Serial.println(myTZ.dateTime());

	// Wait a little bit to not trigger DDoS protection on server
	// See https://github.com/Dag0d/ezTime2#timezonedcircuitfloweu
	delay(3500);

	// Or country codes for countries that do not span multiple timezones
	myTZ.setLocation(F("de"));
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
		printTimezoneList(F("america"));
	#endif

}

void loop() {
	events();
}

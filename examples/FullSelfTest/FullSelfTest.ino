#include <ezTime2.h>

#if defined(ESP8266)
	#include <ESP8266WiFi.h>
	#define EZTIME2_SELFTEST_HAS_WIFI 1
#elif defined(ARDUINO_SAMD_MKR1000)
	#include <WiFi101.h>
	#define EZTIME2_SELFTEST_HAS_WIFI 1
#elif defined(EZTIME_ETHERNET)
	#include <Ethernet.h>
	#define EZTIME2_SELFTEST_HAS_WIFI 0
#elif defined(EZTIME_WIFIESP)
	#include <WifiEsp.h>
	#define EZTIME2_SELFTEST_HAS_WIFI 0
#elif defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
	#include <WiFi.h>
	#define EZTIME2_SELFTEST_HAS_WIFI 1
#else
	#define EZTIME2_SELFTEST_HAS_WIFI 0
#endif

#ifndef EZTIME2_SELFTEST_WIFI_SSID
	#define EZTIME2_SELFTEST_WIFI_SSID "your-ssid"
#endif

#ifndef EZTIME2_SELFTEST_WIFI_PASSWORD
	#define EZTIME2_SELFTEST_WIFI_PASSWORD "your-password"
#endif

#ifndef EZTIME2_SELFTEST_SERVER_REQUEST_DELAY_MS
	#define EZTIME2_SELFTEST_SERVER_REQUEST_DELAY_MS 3200
#endif

#ifndef EZTIME2_SELFTEST_LIST_REQUEST_DELAY_MS
	#define EZTIME2_SELFTEST_LIST_REQUEST_DELAY_MS 1200
#endif

struct TestStats {
	uint16_t passed = 0;
	uint16_t failed = 0;
	uint16_t skipped = 0;
	String failures;
	String skips;
};

static TestStats stats;
static volatile uint8_t eventFlags = 0;

static void eventA() { eventFlags |= 0x01; }
static void eventB() { eventFlags |= 0x02; }

static String int64ToString(const int64_t value) {
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
	return String(buffer);
}

static void addFailure(const String &name, const String &detail) {
	stats.failed++;
	Serial.print(F("[FAIL] "));
	Serial.print(name);
	Serial.print(F(": "));
	Serial.println(detail);
	stats.failures += name + ": " + detail + "\n";
}

static void addPass(const String &name) {
	stats.passed++;
	Serial.print(F("[PASS] "));
	Serial.println(name);
}

static void addSkip(const String &name, const String &reason) {
	stats.skipped++;
	Serial.print(F("[SKIP] "));
	Serial.print(name);
	Serial.print(F(": "));
	Serial.println(reason);
	stats.skips += name + ": " + reason + "\n";
}

static void expectTrue(const String &name, const bool condition, const String &detail = "") {
	if (condition) addPass(name);
	else addFailure(name, detail.length() ? detail : F("expected true"));
}

static void expectFalse(const String &name, const bool condition, const String &detail = "") {
	if (!condition) addPass(name);
	else addFailure(name, detail.length() ? detail : F("expected false"));
}

static void expectEqString(const String &name, const String &actual, const String &expected) {
	if (actual == expected) {
		addPass(name);
	} else {
		addFailure(name, "expected \"" + expected + "\", got \"" + actual + "\"");
	}
}

static void expectEqInt(const String &name, const int64_t actual, const int64_t expected) {
	if (actual == expected) {
		addPass(name);
	} else {
		addFailure(name, "expected " + int64ToString(expected) + ", got " + int64ToString(actual));
	}
}

static void expectRange(const String &name, const int64_t actual, const int64_t minimum, const int64_t maximum) {
	if (actual >= minimum && actual <= maximum) {
		addPass(name);
	} else {
		addFailure(name, "expected range [" + int64ToString(minimum) + ", " + int64ToString(maximum) + "], got " + int64ToString(actual));
	}
}

static ezTime_t makeUtc(const uint16_t year, const uint8_t month, const uint8_t day, const uint8_t hour, const uint8_t minute, const uint8_t second) {
	return ezt::makeTime(hour, minute, second, day, month, year);
}

static void setUtcTime(const uint16_t year, const uint8_t month, const uint8_t day, const uint8_t hour, const uint8_t minute, const uint8_t second, const uint16_t ms = 0) {
	UTC.setPosix(F("UTC"));
	UTC.setTime(makeUtc(year, month, day, hour, minute, second), ms);
}

static bool networkConfigured() {
#if EZTIME2_SELFTEST_HAS_WIFI
	return String(EZTIME2_SELFTEST_WIFI_SSID) != F("your-ssid");
#else
	return false;
#endif
}

static bool connectNetwork() {
#if EZTIME2_SELFTEST_HAS_WIFI
	if (!networkConfigured()) {
		return false;
	}

	if (WiFi.status() == WL_CONNECTED) {
		return true;
	}

	WiFi.begin(EZTIME2_SELFTEST_WIFI_SSID, EZTIME2_SELFTEST_WIFI_PASSWORD);
	unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < 30000UL) {
		delay(250);
	}
	return WiFi.status() == WL_CONNECTED;
#else
	return false;
#endif
}

static void cooldownAfterServerRequest() {
	delay(EZTIME2_SELFTEST_SERVER_REQUEST_DELAY_MS);
}

static void cooldownAfterListRequest() {
	delay(EZTIME2_SELFTEST_LIST_REQUEST_DELAY_MS);
}

static uint16_t findChildCount(const String &listData, const String &targetName) {
#ifdef EZTIME_SERVER_LIST_ENABLE
	uint16_t count = ezt::listLength(listData);
	for (uint16_t i = 0; i < count; i++) {
		String name;
		uint16_t childCount = 0;
		if (ezt::listItem(listData, i, name, childCount) && name == targetName) {
			return childCount;
		}
	}
	return 0xFFFF;
#else
	(void)listData;
	(void)targetName;
	return 0xFFFF;
#endif
}

static void runCoreTests() {
	Serial.println(F("\n== Core tests =="));

#ifdef EZTIME_NETWORK_ENABLE
	ezt::setInterval(0);
#endif
	ezt::events();

	expectEqString(F("errorString(NO_ERROR)"), ezt::errorString(NO_ERROR), F("OK"));
	ezt::setDebug(NONE);
	ezt::setDebug(NONE, Serial);
	addPass(F("setDebug overloads"));
	expectEqString(F("monthStr(1)"), ezt::monthStr(1), F("January"));
	expectEqString(F("monthShortStr(9)"), ezt::monthShortStr(9), F("Sep"));
	expectEqString(F("dayStr(2)"), ezt::dayStr(2), F("Monday"));
	expectEqString(F("dayShortStr(1)"), ezt::dayShortStr(1), F("Sun"));
	expectEqString(F("zeropad"), ezt::zeropad(42, 5), F("00042"));
	expectEqString(F("urlEncode"), ezt::urlEncode(F("Hello world/+?")), F("Hello%20world%2F%2B%3F"));

	ezTime_t compiledExplicit = ezt::compileTime(F("Jan 02 2024"), F("03:04:05"));
	expectEqInt(F("compileTime explicit"), compiledExplicit, makeUtc(2024, 1, 2, 3, 4, 5));
	expectTrue(F("compileTime default > 2020"), ezt::compileTime() > (ezTime_t)1577836800LL);

	tmElements_t tm;
	ezTime_t leap = makeUtc(2024, 2, 29, 12, 34, 56);
	ezt::breakTime(leap, tm);
	expectEqInt(F("breakTime year"), tm.Year + 1970, 2024);
	expectEqInt(F("breakTime month"), tm.Month, 2);
	expectEqInt(F("breakTime day"), tm.Day, 29);
	expectEqInt(F("breakTime hour"), tm.Hour, 12);
	expectEqInt(F("breakTime minute"), tm.Minute, 34);
	expectEqInt(F("breakTime second"), tm.Second, 56);
	expectEqInt(F("makeTime roundtrip"), ezt::makeTime(tm), leap);

	ezTime_t ordinal = ezt::makeOrdinalTime(0, 0, 0, SECOND, TUESDAY, MARCH, 2024);
	ezt::breakTime(ordinal, tm);
	expectEqInt(F("makeOrdinalTime year"), tm.Year + 1970, 2024);
	expectEqInt(F("makeOrdinalTime month"), tm.Month, 3);
	expectEqInt(F("makeOrdinalTime day"), tm.Day, 12);

	setUtcTime(2024, 2, 29, 12, 0, 0, 789);
	expectEqInt(F("timeStatus after setTime"), ezt::timeStatus(), timeSet);
	expectEqInt(F("UTC.now"), UTC.now(), makeUtc(2024, 2, 29, 12, 0, 0));
	expectEqString(F("UTC.dateTime default"), UTC.dateTime(), F("Thursday, 29-Feb-2024 12:00:00 UTC"));
	expectEqInt(F("UTC.hour"), UTC.hour(), 12);
	expectEqInt(F("UTC.minute"), UTC.minute(), 0);
	expectEqInt(F("UTC.second"), UTC.second(), 0);
	expectEqInt(F("UTC.day"), UTC.day(), 29);
	expectEqInt(F("UTC.month"), UTC.month(), 2);
	expectEqInt(F("UTC.year"), UTC.year(), 2024);
	expectEqInt(F("UTC.weekday"), UTC.weekday(), 5); // Thursday
	expectEqInt(F("UTC.dayOfYear"), UTC.dayOfYear(), 59);
	expectEqInt(F("UTC.hourFormat12"), UTC.hourFormat12(), 12);
	expectTrue(F("UTC.isPM"), UTC.isPM());
	expectFalse(F("UTC.isAM"), UTC.isAM());
	expectEqString(F("UTC.militaryTZ"), UTC.militaryTZ(), F("Z"));
	expectEqString(F("UTC.dateTime format"), UTC.dateTime(F("Y-m-d H:i:s")), F("2024-02-29 12:00:00"));
	expectRange(F("UTC.ms(TIME_NOW)"), UTC.ms(TIME_NOW), 0, 999);
	(void)UTC.dateTime();
	expectRange(F("UTC.ms(LAST_READ)"), UTC.ms(LAST_READ), 0, 999);

	Timezone berlin;
	expectTrue(F("Berlin.setPosix"), berlin.setPosix(F("CET-1CEST,M3.5.0,M10.5.0/3")));
	expectEqString(F("Berlin.getPosix"), berlin.getPosix(), F("CET-1CEST,M3.5.0,M10.5.0/3"));

	ezTime_t winterUtc = makeUtc(2024, 1, 15, 12, 0, 0);
	ezTime_t summerUtc = makeUtc(2024, 7, 15, 12, 0, 0);
	expectEqInt(F("Berlin winter offset"), berlin.getOffset(winterUtc, UTC_TIME), -60);
	expectEqInt(F("Berlin summer offset"), berlin.getOffset(summerUtc, UTC_TIME), -120);
	expectEqString(F("Berlin winter tz name"), berlin.getTimezoneName(winterUtc, UTC_TIME), F("CET"));
	expectEqString(F("Berlin summer tz name"), berlin.getTimezoneName(summerUtc, UTC_TIME), F("CEST"));
	expectFalse(F("Berlin winter isDST"), berlin.isDST(winterUtc, UTC_TIME));
	expectTrue(F("Berlin summer isDST"), berlin.isDST(summerUtc, UTC_TIME));
	expectEqString(F("Berlin winter militaryTZ"), berlin.militaryTZ(winterUtc, UTC_TIME), F("A"));
	expectEqString(F("Berlin.dateTime winter"), berlin.dateTime(winterUtc, UTC_TIME, F("Y-m-d H:i T")), F("2024-01-15 13:00 CET"));
	expectEqString(F("Berlin.dateTime local overload"), berlin.dateTime(winterUtc, F("H:i")), F("12:00"));
	expectEqInt(F("Berlin.day"), berlin.day(winterUtc, UTC_TIME), 15);
	expectEqInt(F("Berlin.dayOfYear"), berlin.dayOfYear(winterUtc, UTC_TIME), 14);
	expectEqInt(F("Berlin.hour"), berlin.hour(winterUtc, UTC_TIME), 13);
	expectEqInt(F("Berlin.hourFormat12"), berlin.hourFormat12(winterUtc, UTC_TIME), 1);
	expectFalse(F("Berlin.isAM"), berlin.isAM(winterUtc, UTC_TIME));
	expectTrue(F("Berlin.isPM"), berlin.isPM(winterUtc, UTC_TIME));
	expectEqInt(F("Berlin.minute"), berlin.minute(winterUtc, UTC_TIME), 0);
	expectEqInt(F("Berlin.month"), berlin.month(winterUtc, UTC_TIME), 1);
	expectEqInt(F("Berlin.second"), berlin.second(winterUtc, UTC_TIME), 0);
	expectEqInt(F("Berlin.weekISO"), berlin.weekISO(winterUtc, UTC_TIME), 3);
	expectEqInt(F("Berlin.weekday"), berlin.weekday(winterUtc, UTC_TIME), 2);
	expectEqInt(F("Berlin.year"), berlin.year(winterUtc, UTC_TIME), 2024);
	expectEqInt(F("Berlin.yearISO"), berlin.yearISO(winterUtc, UTC_TIME), 2024);

	String tzname;
	bool isDst = false;
	int16_t offset = 0;
	ezTime_t berlinLocal = berlin.tzTime(winterUtc, UTC_TIME, tzname, isDst, offset);
	expectEqInt(F("Berlin.tzTime overload"), berlin.tzTime(winterUtc, UTC_TIME), makeUtc(2024, 1, 15, 13, 0, 0));
	expectEqInt(F("Berlin.tzTime convert"), berlinLocal, makeUtc(2024, 1, 15, 13, 0, 0));
	expectEqString(F("Berlin.tzTime tzname"), tzname, F("CET"));
	expectFalse(F("Berlin.tzTime isDst"), isDst);
	expectEqInt(F("Berlin.tzTime offset"), offset, -60);
	setUtcTime(2024, 1, 15, 12, 0, 0);
	expectRange(F("Berlin.now"), berlin.now(), makeUtc(2024, 1, 15, 13, 0, 0), makeUtc(2024, 1, 15, 13, 0, 1));
	berlin.setTime(makeUtc(2024, 1, 15, 18, 45, 30));
	expectEqString(F("Berlin.setTime(ezTime_t)"), berlin.dateTime(F("Y-m-d H:i:s T")), F("2024-01-15 18:45:30 CET"));
	berlin.setTime(6, 7, 8, 9, 10, 2024);
	expectEqString(F("Berlin.setTime(parts)"), berlin.dateTime(F("Y-m-d H:i:s T")), F("2024-10-09 06:07:08 CEST"));
	setUtcTime(2024, 1, 15, 12, 0, 0);

	berlin.setDefault();
	expectEqInt(F("defaultTZ hour wrapper"), ezt::hour(winterUtc, UTC_TIME), 13);
	expectEqInt(F("defaultTZ hourFormat12 wrapper"), ezt::hourFormat12(winterUtc, UTC_TIME), 1);
	expectFalse(F("defaultTZ isAM wrapper"), ezt::isAM(winterUtc, UTC_TIME));
	expectFalse(F("defaultTZ isDST wrapper"), ezt::isDST(winterUtc, UTC_TIME));
	expectTrue(F("defaultTZ isPM wrapper"), ezt::isPM(winterUtc, UTC_TIME));
	expectEqString(F("defaultTZ getTimezoneName wrapper"), ezt::getTimezoneName(winterUtc, UTC_TIME), F("CET"));
	expectEqInt(F("defaultTZ getOffset wrapper"), ezt::getOffset(winterUtc, UTC_TIME), -60);
	expectEqString(F("defaultTZ dateTime wrapper"), ezt::dateTime(winterUtc, UTC_TIME, F("H:i")), F("13:00"));
	expectEqInt(F("defaultTZ day wrapper"), ezt::day(winterUtc, UTC_TIME), 15);
	expectEqInt(F("defaultTZ dayOfYear wrapper"), ezt::dayOfYear(winterUtc, UTC_TIME), 14);
	expectEqInt(F("defaultTZ minute wrapper"), ezt::minute(winterUtc, UTC_TIME), 0);
	expectEqInt(F("defaultTZ month wrapper"), ezt::month(winterUtc, UTC_TIME), 1);
	expectEqInt(F("defaultTZ second wrapper"), ezt::second(winterUtc, UTC_TIME), 0);
	expectEqInt(F("defaultTZ weekISO wrapper"), ezt::weekISO(winterUtc, UTC_TIME), 3);
	expectEqInt(F("defaultTZ weekday wrapper"), ezt::weekday(winterUtc, UTC_TIME), 2);
	expectEqInt(F("defaultTZ year wrapper"), ezt::year(winterUtc, UTC_TIME), 2024);
	expectEqInt(F("defaultTZ yearISO wrapper"), ezt::yearISO(winterUtc, UTC_TIME), 2024);
	expectEqString(F("defaultTZ dateTime default wrapper"), ezt::dateTime(), F("Monday, 15-Jan-2024 13:00:00 CET"));
	expectEqInt(F("defaultTZ ms wrapper"), ezt::ms(TIME_NOW), UTC.ms(TIME_NOW));
	expectEqString(F("defaultTZ militaryTZ wrapper"), ezt::militaryTZ(winterUtc, UTC_TIME), F("A"));
	expectRange(F("defaultTZ now wrapper"), (int64_t)ezt::now(), (int64_t)makeUtc(2024, 1, 15, 13, 0, 0), (int64_t)makeUtc(2024, 1, 15, 13, 0, 1));
	UTC.setDefault();

	ezt::setTime(makeUtc(2024, 1, 2, 3, 4, 5));
	expectEqString(F("global setTime(wrapper)"), UTC.dateTime(F("Y-m-d H:i:s")), F("2024-01-02 03:04:05"));
	ezt::setTime(6, 7, 8, 9, 10, 2024);
	expectEqString(F("global setTime(parts wrapper)"), UTC.dateTime(F("Y-m-d H:i:s")), F("2024-10-09 06:07:08"));

	setUtcTime(2021, 1, 1, 12, 0, 0);
	expectEqInt(F("weekISO"), UTC.weekISO(), 53);
	expectEqInt(F("yearISO"), UTC.yearISO(), 2020);

	Timezone nepal;
	expectTrue(F("Nepal.setPosix"), nepal.setPosix(F("NPT-5:45")));
	expectEqString(F("Nepal.militaryTZ"), nepal.militaryTZ(makeUtc(2024, 1, 1, 0, 0, 0), UTC_TIME), F("?"));

	Timezone locked(true);
	expectFalse(F("Locked TZ rejects setPosix"), locked.setPosix(F("CET-1")));
	expectEqInt(F("error LOCKED_TO_UTC"), ezt::error(true), LOCKED_TO_UTC);
	expectEqInt(F("error reset"), ezt::error(), NO_ERROR);

	UTC.setPosix(F("UTC"));
	setUtcTime(2024, 1, 1, 0, 0, 0);
	(void)UTC.dateTime();
	expectFalse(F("secondChanged false immediately"), ezt::secondChanged());
	UTC.setTime(UTC.now() + 1);
	expectTrue(F("secondChanged true after +1s"), ezt::secondChanged());
	(void)UTC.dateTime();
	UTC.setTime(UTC.now() + 60);
	expectTrue(F("minuteChanged true after +60s"), ezt::minuteChanged());

	eventFlags = 0;
	setUtcTime(2024, 1, 1, 0, 0, 0);
	uint8_t eventHandle = UTC.setEvent(eventA, UTC.now() + 10, UTC_TIME);
	expectTrue(F("setEvent(time) returns handle"), eventHandle > 0);
	ezt::deleteEvent(eventHandle);
	UTC.setTime(UTC.now() + 20);
	ezt::events();
	expectFalse(F("deleteEvent(handle) works"), (eventFlags & 0x01) != 0);

	eventFlags = 0;
	setUtcTime(2024, 1, 1, 0, 0, 0);
	UTC.setEvent(eventA, UTC.now() + 10, UTC_TIME);
	ezt::deleteEvent(eventA);
	UTC.setTime(UTC.now() + 20);
	ezt::events();
	expectFalse(F("deleteEvent(function) works"), (eventFlags & 0x01) != 0);

	eventFlags = 0;
	setUtcTime(2024, 1, 1, 0, 0, 0);
	UTC.setEvent(eventA, 0, 1, 0, 1, 1, 2024);
	UTC.setTime(makeUtc(2024, 1, 1, 0, 1, 1));
	ezt::events();
	expectTrue(F("setEvent(date parts) fires"), (eventFlags & 0x01) != 0);

	eventFlags = 0;
	setUtcTime(2024, 1, 1, 0, 0, 0);
	ezt::setEvent(eventA, UTC.now() + 5, UTC_TIME);
	UTC.setTime(UTC.now() + 10);
	ezt::events();
	expectTrue(F("global setEvent(wrapper) fires"), (eventFlags & 0x01) != 0);

	eventFlags = 0;
	setUtcTime(2024, 1, 1, 0, 0, 0);
	for (uint8_t i = 0; i < MAX_EVENTS; i++) {
		UTC.setEvent(eventB, UTC.now() + i + 1, UTC_TIME);
	}
	expectEqInt(F("too many events"), UTC.setEvent(eventB, UTC.now() + 99, UTC_TIME), 0);
	expectEqInt(F("error TOO_MANY_EVENTS"), ezt::error(true), TOO_MANY_EVENTS);
	ezt::deleteEvent(eventB);

	// 2036 remains within 32-bit range and should always work.
	ezTime_t y2036 = makeUtc(2036, 2, 7, 6, 28, 16);
	ezt::breakTime(y2036, tm);
	expectEqInt(F("2036 year"), tm.Year + 1970, 2036);
	expectEqInt(F("2036 month"), tm.Month, 2);
	expectEqInt(F("2036 day"), tm.Day, 7);

	ezTime_t max32 = makeUtc(2038, 1, 19, 3, 14, 7);
	ezt::breakTime(max32, tm);
	expectEqInt(F("2038 boundary year"), tm.Year + 1970, 2038);
	expectEqInt(F("2038 boundary second"), tm.Second, 7);

#if EZTIME_TIME_BITS == 64
	ezTime_t y2040 = makeUtc(2040, 1, 1, 12, 0, 0);
	ezt::breakTime(y2040, tm);
	expectEqInt(F("2040 year in 64-bit mode"), tm.Year + 1970, 2040);
	expectEqInt(F("2040 roundtrip in 64-bit mode"), ezt::makeTime(tm), y2040);
#else
	addSkip(F("2040 year in 64-bit mode"), F("32-bit time mode"));
	addSkip(F("2040 roundtrip in 64-bit mode"), F("32-bit time mode"));
#endif
}

static void runNetworkTests() {
#ifndef EZTIME_NETWORK_ENABLE
	addSkip(F("Network tests"), F("EZTIME_NETWORK_ENABLE is disabled"));
	return;
#else
	Serial.println(F("\n== Network tests =="));

	if (!networkConfigured()) {
		addSkip(F("Network tests"), F("WiFi credentials not configured"));
		return;
	}

	if (!connectNetwork()) {
		addFailure(F("Network connect"), F("unable to connect within timeout"));
		return;
	}
	addPass(F("Network connect"));

	ezt::setServers(NTP_SERVER, NTP_SERVER_2, NTP_SERVER_3);
	addPass(F("setServers runtime API"));
	ezt::setServer(NTP_SERVER);
	ezt::setInterval(60);
	ezt::setDebug(NONE, Serial);
	ezt::setDebug(NONE);

	expectTrue(F("waitForSync"), ezt::waitForSync(30), ezt::errorString());
	expectEqInt(F("timeStatus after sync"), ezt::timeStatus(), timeSet);
	expectTrue(F("lastNtpUpdateTime > 0"), ezt::lastNtpUpdateTime() > 0);

	ezTime_t ntpTime = 0;
	unsigned long measuredAt = 0;
	expectTrue(F("queryNTP"), ezt::queryNTP(NTP_SERVER, ntpTime, measuredAt), ezt::errorString());
	if (ntpTime > 0) {
		expectTrue(F("queryNTP unix time sane"), ntpTime > (ezTime_t)1700000000LL);
		expectTrue(F("queryNTP measuredAt sane"), measuredAt <= millis());
	}

	ezt::error(true);
	ezt::updateNTP();
	expectEqInt(F("updateNTP leaves no error"), ezt::error(true), NO_ERROR);

	String publicIP;
	expectTrue(F("getPublicIP"), ezt::getPublicIP(publicIP), ezt::errorString());
	if (publicIP.length()) {
		expectTrue(F("getPublicIP non-empty"), publicIP.length() >= 7);
	}
	cooldownAfterServerRequest();

	Timezone netTZ;
	netTZ.setGeoLookupMode(GEOIP_LOOKUP_ONLY);
	expectEqInt(F("getGeoLookupMode GEOIP"), netTZ.getGeoLookupMode(), GEOIP_LOOKUP_ONLY);
	netTZ.setGeoLookupMode(EXT_GEOIP_LOOKUP_ONLY);
	expectEqInt(F("getGeoLookupMode EXT_GEOIP"), netTZ.getGeoLookupMode(), EXT_GEOIP_LOOKUP_ONLY);
	netTZ.setGeoLookupMode(GEOIP_LOOKUP_WITH_EXT_FALLBACK);
	expectEqInt(F("getGeoLookupMode fallback"), netTZ.getGeoLookupMode(), GEOIP_LOOKUP_WITH_EXT_FALLBACK);

	bool locationTimezoneOk = netTZ.setLocation(F("Pacific/Auckland"));
	expectTrue(F("setLocation timezone"), locationTimezoneOk, ezt::errorString());
	if (locationTimezoneOk && netTZ.getOlson().length()) {
		expectEqString(F("getOlson"), netTZ.getOlson(), F("Pacific/Auckland"));
		expectEqString(F("getOlsen"), netTZ.getOlsen(), F("Pacific/Auckland"));
	}
	cooldownAfterServerRequest();

	bool locationCountryOk = netTZ.setLocation(F("DE"));
	expectTrue(F("setLocation country"), locationCountryOk, ezt::errorString());
	cooldownAfterServerRequest();

	if (netTZ.setLocation()) {
		addPass(F("setLocation GEOIP"));
	} else {
		String geoipError = ezt::errorString();
		if (geoipError.indexOf(F("Country Spans Multiple Timezones")) >= 0) {
			addSkip(F("setLocation GEOIP"), geoipError);
		} else {
			addFailure(F("setLocation GEOIP"), geoipError);
		}
	}
	cooldownAfterServerRequest();

	expectTrue(F("setLocation EXT_GEOIP"), netTZ.setLocation(F("EXT_GEOIP")), ezt::errorString());
	cooldownAfterServerRequest();

#if defined(EZTIME_CACHE_EEPROM)
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
	addSkip(F("EEPROM cache write source"), F("library is built with EEPROM cache backend; ESP32 NVS backend not enabled"));
	addSkip(F("EEPROM cache read"), F("library is built with EEPROM cache backend; ESP32 NVS backend not enabled"));
	addSkip(F("EEPROM cache clear"), F("library is built with EEPROM cache backend; ESP32 NVS backend not enabled"));
#else
	Timezone cacheTZ;
	(void)cacheTZ.setCache(0);
	bool eepromWriteOk = cacheTZ.setLocation(F("Europe/Berlin"));
	expectTrue(F("EEPROM cache write source"), eepromWriteOk, ezt::errorString());
	cooldownAfterServerRequest();
	if (eepromWriteOk) {
		expectTrue(F("EEPROM cache read"), cacheTZ.setCache(0), F("expected freshly written cache to read back"));
		cacheTZ.clearCache();
		addPass(F("EEPROM cache clear"));
	} else {
		addSkip(F("EEPROM cache read"), F("cache source lookup failed"));
		addSkip(F("EEPROM cache clear"), F("cache source lookup failed"));
	}
#endif
#elif defined(EZTIME_CACHE_NVS)
	Timezone cacheTZ;
	(void)cacheTZ.setCache(F("eztime2"), F("selftest"));
	bool nvsWriteOk = cacheTZ.setLocation(F("Europe/Berlin"));
	expectTrue(F("NVS cache write source"), nvsWriteOk, ezt::errorString());
	cooldownAfterServerRequest();
	if (nvsWriteOk) {
		expectTrue(F("NVS cache read"), cacheTZ.setCache(F("eztime2"), F("selftest")), F("expected freshly written cache to read back"));
		cacheTZ.clearCache(true);
		addPass(F("NVS cache clear"));
	} else {
		addSkip(F("NVS cache read"), F("cache source lookup failed"));
		addSkip(F("NVS cache clear"), F("cache source lookup failed"));
	}
#else
	addSkip(F("Cache tests"), F("No cache backend enabled"));
#endif

#ifdef EZTIME_SERVER_LIST_ENABLE
	String regions;
	String america;
	bool regionsOk = ezt::listTimezones(F("regions"), regions);
	expectTrue(F("listTimezones regions"), regionsOk, ezt::errorString());
	cooldownAfterListRequest();
	if (regionsOk) {
		expectTrue(F("listLength regions"), ezt::listLength(regions) > 0);

		String firstName;
		uint16_t firstChildCount = 0;
		expectTrue(F("listItem regions[0]"), ezt::listItem(regions, 0, firstName, firstChildCount));
	} else {
		addSkip(F("listLength regions"), F("regions lookup failed"));
		addSkip(F("listItem regions[0]"), F("regions lookup failed"));
	}

	bool americaOk = ezt::listTimezones(F("america"), america);
	expectTrue(F("listTimezones america"), americaOk, ezt::errorString());
	cooldownAfterListRequest();
	if (americaOk) {
		uint16_t americaCount = ezt::listLength(america);
		expectTrue(F("listLength america"), americaCount > 0);

		uint16_t advertisedAmericaCount = findChildCount(regions, F("America"));
		expectTrue(F("regions contains America"), advertisedAmericaCount != 0xFFFF);
		if (advertisedAmericaCount != 0xFFFF && regionsOk) {
			if (advertisedAmericaCount == americaCount) {
				addPass(F("America child count matches list"));
			} else {
				addSkip(F("America child count matches list"), F("server list dataset not updated yet"));
			}
		}
	} else {
		addSkip(F("listLength america"), F("america lookup failed"));
		addSkip(F("regions contains America"), F("america lookup failed"));
		addSkip(F("America child count matches list"), F("america lookup failed"));
	}
#else
	addSkip(F("LIST tests"), F("EZTIME_SERVER_LIST_ENABLE is disabled"));
#endif

	ezt::setInterval(0);
#endif
}

static void printSummary() {
	Serial.println(F("\n== Summary =="));
	Serial.print(F("Passed : "));
	Serial.println(stats.passed);
	Serial.print(F("Failed : "));
	Serial.println(stats.failed);
	Serial.print(F("Skipped: "));
	Serial.println(stats.skipped);

	if (stats.failed) {
		Serial.println(F("\nFailed tests:"));
		Serial.print(stats.failures);
	}

	if (stats.skipped) {
		Serial.println(F("\nSkipped tests:"));
		Serial.print(stats.skips);
	}

	if (!stats.failed) {
		Serial.println(F("\nOverall result: PASS"));
	} else {
		Serial.println(F("\nOverall result: FAIL"));
	}
}

void setup() {
	Serial.begin(115200);
	while (!Serial) { ; }
	delay(250);

	Serial.println();
	Serial.println(F("ezTime2 Full Self-Test"));
	Serial.println(F("====================="));
	Serial.print(F("Time width: "));
	Serial.print(EZTIME_TIME_BITS);
	Serial.println(F("-bit"));

	runCoreTests();
	runNetworkTests();
	printSummary();
}

void loop() {
}

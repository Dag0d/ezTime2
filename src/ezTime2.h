/* Extensive API documentation is at https://github.com/Dag0d/ezTime2 */

#ifndef _EZTIME2_H_
#ifdef __cplusplus
#define _EZTIME2_H_

#if __has_include("ezTime2Config.h")
	#include "ezTime2Config.h"
#endif

// Sets the language for the names of Months and Days. See the src/lang directory for supported languages.
// Put overrides in ezTime2Config.h or pass them as global build flags.
#ifndef EZTIME_LANGUAGE
	#define EZTIME_LANGUAGE EN
#endif

// Compiles in NTP updating, timezoned fetching and caching.
// Network support is enabled by default. Put EZTIME_NETWORK_DISABLE into
// ezTime2Config.h next to your sketch, or pass it as a global build flag,
// if you want a smaller, no-network build.
#if defined(EZTIME_NETWORK_ENABLE) && defined(EZTIME_NETWORK_DISABLE)
	#error "Only one of EZTIME_NETWORK_ENABLE or EZTIME_NETWORK_DISABLE may be defined."
#endif

#if !defined(EZTIME_NETWORK_ENABLE) && !defined(EZTIME_NETWORK_DISABLE)
	#define EZTIME_NETWORK_ENABLE
#endif

// Arduino Ethernet shields
// Put this in ezTime2Config.h or pass it as a global build flag if needed.
// #define EZTIME_ETHERNET

// Arduino board with ESP8266 shield
// Put this in ezTime2Config.h or pass it as a global build flag if needed.
// #define EZTIME_WIFIESP

// Uncomment one of the below to only put only messages up to a certain level in the compiled code
// (You still need to turn them on with setDebug(someLevel) to see them).
// Put these in ezTime2Config.h or pass them as global build flags.
// #define EZTIME_MAX_DEBUGLEVEL_NONE
// #define EZTIME_MAX_DEBUGLEVEL_ERROR
// #define EZTIME_MAX_DEBUGLEVEL_INFO

// Cache mechanism, either EEPROM or NVS, not both. (See README)
// If you define one of these in ezTime2Config.h or as a global build flag, that manual choice wins.
// Otherwise ezTime2 defaults to NVS on ESP32 and EEPROM on other boards.
// #define EZTIME_CACHE_EEPROM
// #define EZTIME_CACHE_NVS

// Uncomment if you want to access ezTime functions only after "ezt."
// (to avoid naming conflicts in bigger projects, e.g.)
// Put this in ezTime2Config.h or pass it as a global build flag if needed.
// #define EZTIME_EZT_NAMESPACE

// Timestamp width selection.
// By default AVR builds stay on 32-bit for size/speed, all other targets use 64-bit.
// Put one of these in ezTime2Config.h or pass it as a global build flag to override the default:
// #define EZTIME_FORCE_32BIT_TIME
// #define EZTIME_FORCE_64BIT_TIME

// Optional server-side lookup features.
// EXT_GEOIP adds a public-IP based fallback path for timezone lookups.
// LIST support is intended for richer UIs and is only auto-enabled on stronger boards.
// Put these in ezTime2Config.h or pass them as global build flags if needed.
// #define EZTIME_EXT_GEOIP_FALLBACK
// #define EZTIME_SERVER_LIST_ENABLE
// #define EZTIME_SERVER_LIST_DISABLE

// Optional NTP server defaults. Leave NTP_SERVER_2 and NTP_SERVER_3 empty if unused.
// Put these in ezTime2Config.h or pass them as global build flags if needed.
// #define NTP_SERVER "pool.ntp.org"
// #define NTP_SERVER_2 ""
// #define NTP_SERVER_3 ""

// Warranty void if edited below this point...
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#if defined(EZTIME_FORCE_32BIT_TIME) && defined(EZTIME_FORCE_64BIT_TIME)
	#error "Only one of EZTIME_FORCE_32BIT_TIME or EZTIME_FORCE_64BIT_TIME may be defined."
#endif

#if !defined(EZTIME_FORCE_32BIT_TIME) && !defined(EZTIME_FORCE_64BIT_TIME)
	#if defined(__AVR__)
		#define EZTIME_FORCE_32BIT_TIME
	#else
		#define EZTIME_FORCE_64BIT_TIME
	#endif
#endif

#if defined(EZTIME_CACHE_EEPROM) && defined(EZTIME_CACHE_NVS)
	#error "Only one of EZTIME_CACHE_EEPROM or EZTIME_CACHE_NVS may be defined."
#endif

#if !defined(EZTIME_CACHE_EEPROM) && !defined(EZTIME_CACHE_NVS)
	#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
		#define EZTIME_CACHE_NVS
	#else
		#define EZTIME_CACHE_EEPROM
	#endif
#endif

#if !defined(EZTIME_SERVER_LIST_ENABLE) && !defined(EZTIME_SERVER_LIST_DISABLE)
	#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
		#define EZTIME_SERVER_LIST_ENABLE
	#endif
#endif

namespace eztime_detail {
	template <bool UseNative, typename Fallback>
	struct SelectTimeType {
		typedef Fallback type;
	};

	template <typename Fallback>
	struct SelectTimeType<true, Fallback> {
		typedef time_t type;
	};
}

#if defined(EZTIME_FORCE_64BIT_TIME)
	typedef typename eztime_detail::SelectTimeType<(((time_t)-1 < (time_t)0) && (sizeof(time_t) >= sizeof(int64_t))), int64_t>::type ezTime_t;
	#define EZTIME_TIME_BITS 64
	#define EZTIME_TIME_MAX INT64_MAX
#else
	typedef int32_t ezTime_t;
	#define EZTIME_TIME_BITS 32
	#define EZTIME_TIME_MAX INT32_MAX
#endif

extern "C++" {

typedef enum {
	NO_ERROR,
	LAST_ERROR,
	NO_NETWORK,
	TIMEOUT,
	CONNECT_FAILED,
	DATA_NOT_FOUND,
	LOCKED_TO_UTC,
	NO_CACHE_SET,
	CACHE_TOO_SMALL,
	TOO_MANY_EVENTS,
	INVALID_DATA,
	SERVER_ERROR
} ezError_t;

typedef enum {
	NONE,
	ERROR,
	INFO,
	DEBUG
} ezDebugLevel_t;

typedef enum {
	LOCAL_TIME,
	UTC_TIME
} ezLocalOrUTC_t;

typedef enum {
	GEOIP_LOOKUP_ONLY,
	EXT_GEOIP_LOOKUP_ONLY,
	GEOIP_LOOKUP_WITH_EXT_FALLBACK
} ezGeoLookupMode_t;

#define SUNDAY			1
#define MONDAY			2
#define TUESDAY			3
#define WEDNESDAY		4
#define THURSDAY		5
#define FRIDAY			6
#define SATURDAY		7

#define JANUARY			1
#define FEBRUARY		2
#define MARCH			3
#define APRIL			4
#define MAY				5
#define JUNE			6
#define JULY			7
#define AUGUST			8
#define SEPTEMBER		9
#define OCTOBER			10
#define NOVEMBER		11
#define DECEMBER		12

#define	FIRST			1
#define	SECOND			2
#define	THIRD			3
#define FOURTH			4
#define LAST			5

#define LEAP_YEAR(Y)	( ((1970 + (Y)) > 0) && !((1970 + (Y)) % 4) && (((1970 + (Y)) % 100) || !((1970 + (Y)) % 400)) )
#define SECS_PER_DAY	((ezTime_t)86400)

typedef struct  {
	uint8_t Second;
	uint8_t Minute;
	uint8_t Hour;
	uint8_t Wday;
	uint8_t Day;
	uint8_t Month;
	uint16_t Year;
} tmElements_t;

typedef enum {
	timeNotSet,
	timeNeedsSync,
	timeSet
} timeStatus_t;

typedef struct {
	ezTime_t time;
	void (*function)();
} ezEvent_t;

#define MAX_EVENTS				8

#define TIME_NOW				((ezTime_t)EZTIME_TIME_MAX)
#define LAST_READ				((ezTime_t)(EZTIME_TIME_MAX - 1))

#define EZTIME_NTP_EPOCH_OFFSET		((int64_t)2208988800LL)
#define EZTIME_NTP_ERA_SECONDS		((int64_t)4294967296LL)

#define NTP_PACKET_SIZE			48
#define NTP_LOCAL_PORT			4242
#ifndef NTP_SERVER
	#define NTP_SERVER			"pool.ntp.org"
#endif
#ifndef NTP_SERVER_2
	#define NTP_SERVER_2		""
#endif
#ifndef NTP_SERVER_3
	#define NTP_SERVER_3		""
#endif
#define NTP_TIMEOUT				1500
#define NTP_INTERVAL			1801
#define NTP_RETRY				20
#define NTP_STALE_AFTER			3602

#define TIMEZONED_REMOTE_HOST	"timezoned.circuitflow.eu"
#define TIMEZONED_REMOTE_PORT	2342
#define TIMEZONED_LOCAL_PORT	2342
#define TIMEZONED_TIMEOUT		2000

#define EEPROM_CACHE_LEN		50
#define MAX_CACHE_PAYLOAD		((EEPROM_CACHE_LEN - 3) / 3) * 4 + ( (EEPROM_CACHE_LEN - 3) % 3)
#define MAX_CACHE_AGE_MONTHS	6

#define ATOM 				"Y-m-d\\TH:i:sP"
#define COOKIE				"l, d-M-Y H:i:s T"
#define ISO8601				"Y-m-d\\TH:i:sO"
#define RFC822				"D, d M y H:i:s O"
#define RFC850				COOKIE
#define RFC1036				RFC822
#define RFC1123				RFC822
#define RFC2822				RFC822
#define RFC3339 			ATOM
#define RFC3339_EXT			"Y-m-d\\TH:i:s.vP"
#define RSS					RFC822
#define W3C					ATOM
#define ISO8601_YWD			"X-\\WW-N"
#define DEFAULT_TIMEFORMAT	COOKIE

namespace ezt {
	void breakTime(const ezTime_t time, tmElements_t &tm);
	ezTime_t compileTime(const String compile_date = __DATE__, const String compile_time = __TIME__);
	String dayShortStr(const uint8_t month);
	String dayStr(const uint8_t month);
	void deleteEvent(const uint8_t event_handle);
	void deleteEvent(void (*function)());
	ezError_t error(const bool reset = false);
	String errorString(const ezError_t err = LAST_ERROR);
	void events();
	ezTime_t makeOrdinalTime(const uint8_t hour, const uint8_t minute, const uint8_t second, uint8_t ordinal, const uint8_t wday, const uint8_t month, uint16_t year);
	ezTime_t makeTime(const uint8_t hour, const uint8_t minute, const uint8_t second, const uint8_t day, const uint8_t month, const uint16_t year);
	ezTime_t makeTime(tmElements_t &tm);
	bool minuteChanged();
	String monthShortStr(const uint8_t month);
	String monthStr(const uint8_t month);
	bool secondChanged();
	void setDebug(const ezDebugLevel_t level);
	void setDebug(const ezDebugLevel_t level, Print &device);
	timeStatus_t timeStatus();
	String urlEncode(const String str);
	String zeropad(const uint32_t number, const uint8_t length);

	#ifdef EZTIME_NETWORK_ENABLE
		bool getPublicIP(String &ip);
		bool queryNTP(const String server, ezTime_t &t, unsigned long &measured_at);
		void setInterval(const uint16_t seconds = 0);
		void setServer(const String ntp_server = NTP_SERVER);
		void setServers(const String ntp_server_1, const String ntp_server_2 = "", const String ntp_server_3 = "");
		void updateNTP();
		bool waitForSync(const uint16_t timeout = 0);
		ezTime_t lastNtpUpdateTime();
		#ifdef EZTIME_SERVER_LIST_ENABLE
			bool listTimezones(const String list_name, String &list_data);
			uint16_t listLength(const String &list_data);
			bool listItem(const String &list_data, const uint16_t item_index, String &name, uint16_t &child_count);
		#endif
	#endif
}

class Timezone {
	public:
		Timezone(const bool locked_to_UTC = false);
		String dateTime(const String format = DEFAULT_TIMEFORMAT);
		String dateTime(ezTime_t t, const String format = DEFAULT_TIMEFORMAT);
		String dateTime(ezTime_t t, const ezLocalOrUTC_t local_or_utc, const String format = DEFAULT_TIMEFORMAT);
		uint8_t day(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		uint16_t dayOfYear(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		int16_t getOffset(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		String getPosix();
		String getTimezoneName(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		uint8_t hour(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		uint8_t hourFormat12(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		bool isAM(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		bool isDST(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		bool isPM(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		String militaryTZ(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		uint8_t minute(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		uint8_t month(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		uint16_t ms(ezTime_t t = TIME_NOW);
		ezTime_t now();
		uint8_t second(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		void setDefault();
		uint8_t setEvent(void (*function)(), const uint8_t hr, const uint8_t min, const uint8_t sec, const uint8_t day, const uint8_t mnth, uint16_t yr);
		uint8_t setEvent(void (*function)(), ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		bool setPosix(const String posix);
		void setTime(const ezTime_t t, const uint16_t ms = 0);
		void setTime(const uint8_t hr, const uint8_t min, const uint8_t sec, const uint8_t day, const uint8_t mnth, uint16_t yr);
		ezTime_t tzTime(ezTime_t t = TIME_NOW, ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		ezTime_t tzTime(ezTime_t t, ezLocalOrUTC_t local_or_utc, String &tzname, bool &is_dst, int16_t &offset);
		uint8_t weekISO(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		uint8_t weekday(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		uint16_t year(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
		uint16_t yearISO(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	private:
		String _posix, _olson;
		bool _locked_to_UTC;

	#ifdef EZTIME_NETWORK_ENABLE
		private:
			ezGeoLookupMode_t _geo_lookup_mode;
		public:
			bool setLocation(const String location = "GeoIP");
			void setGeoLookupMode(const ezGeoLookupMode_t mode);
			ezGeoLookupMode_t getGeoLookupMode() const;
			String getOlson();
			String getOlsen();
		#ifdef EZTIME_CACHE_EEPROM
			public:
				bool setCache(const int16_t address);
			private:
				int16_t _eeprom_address;
		#endif
		#ifdef EZTIME_CACHE_NVS
			public:
				bool setCache(const String name, const String key);
			private:
				String _nvs_name, _nvs_key;
		#endif
 		#if defined(EZTIME_CACHE_EEPROM) || defined(EZTIME_CACHE_NVS)
 			public:
				void clearCache(const bool delete_section = false);
 			private:
 				bool setCache();
  				bool writeCache(String &str);
 				bool readCache(String &olson, String &posix, uint8_t &months_since_jan_2018);
 				uint8_t _cache_month;
		#endif
	#endif
};

extern Timezone UTC;
extern Timezone *defaultTZ;

namespace ezt {
	String dateTime(const String format = DEFAULT_TIMEFORMAT);
	String dateTime(ezTime_t t, const String format = DEFAULT_TIMEFORMAT);
	String dateTime(ezTime_t t, const ezLocalOrUTC_t local_or_utc, const String format = DEFAULT_TIMEFORMAT);
	uint8_t day(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint16_t dayOfYear(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	int16_t getOffset(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	String getTimezoneName(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint8_t hour(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint8_t hourFormat12(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	bool isAM(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	bool isDST(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	bool isPM(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	String militaryTZ(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint8_t minute(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint8_t month(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint16_t ms(ezTime_t t = TIME_NOW);
	ezTime_t now();
	uint8_t second(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint8_t setEvent(void (*function)(), const uint8_t hr, const uint8_t min, const uint8_t sec, const uint8_t day, const uint8_t mnth, uint16_t yr);
	uint8_t setEvent(void (*function)(), ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	void setTime(const uint8_t hr, const uint8_t min, const uint8_t sec, const uint8_t day, const uint8_t month, const uint16_t yr);
	void setTime(ezTime_t t);
	uint8_t weekISO(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint8_t weekday(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint16_t year(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
	uint16_t yearISO(ezTime_t t = TIME_NOW, const ezLocalOrUTC_t local_or_utc = LOCAL_TIME);
}

#ifndef EZTIME_EZT_NAMESPACE
	using namespace ezt;
#endif

/* Useful Constants */
#define SECS_PER_MIN  ((ezTime_t)60)
#define SECS_PER_HOUR ((ezTime_t)3600)
#define DAYS_PER_WEEK ((ezTime_t)7)
#define SECS_PER_WEEK (SECS_PER_DAY * DAYS_PER_WEEK)
#define SECS_PER_YEAR (SECS_PER_WEEK * (ezTime_t)52)
#define SECS_YR_2000  ((ezTime_t)946684800LL)

/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define dayOfWeek(_time_)  ((( _time_ / SECS_PER_DAY + 4)  % DAYS_PER_WEEK)+1)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)
#define elapsedSecsToday(_time_)  (_time_ % SECS_PER_DAY)

#define previousMidnight(_time_) (( _time_ / SECS_PER_DAY) * SECS_PER_DAY)
#define nextMidnight(_time_) ( previousMidnight(_time_)  + SECS_PER_DAY )
#define elapsedSecsThisWeek(_time_)  (elapsedSecsToday(_time_) +  ((dayOfWeek(_time_)-1) * SECS_PER_DAY) )
#define previousSunday(_time_)  (_time_ - elapsedSecsThisWeek(_time_))
#define nextSunday(_time_) ( previousSunday(_time_)+SECS_PER_WEEK)

#define minutesToTime_t(M) ((ezTime_t)(M) * SECS_PER_MIN)
#define hoursToTime_t(H)   ((ezTime_t)(H) * SECS_PER_HOUR)
#define daysToTime_t(D)    ((ezTime_t)(D) * SECS_PER_DAY)
#define weeksToTime_t(W)   ((ezTime_t)(W) * SECS_PER_WEEK)

} // extern "C++"
#endif // __cplusplus
#endif //_EZTIME2_H_

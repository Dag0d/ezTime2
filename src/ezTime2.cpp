#include <Arduino.h>

#if __has_include("ezTime2Config.h")
	#include "ezTime2Config.h"
#endif

#include <ezTime2.h>

#ifdef EZTIME_NETWORK_ENABLE
	#ifdef EZTIME_CACHE_NVS
		#include <Preferences.h>		// For timezone lookup cache
	#endif
	#ifdef EZTIME_CACHE_EEPROM
		#include <EEPROM.h>
	#endif	
	#if defined(ESP8266)
		#include <ESP8266WiFi.h>
		#include <WiFiUdp.h>
	#elif defined(ARDUINO_SAMD_MKR1000)
		#include <SPI.h>
		#include <WiFi101.h>
		#include <WiFiUdp.h>
	#elif defined(EZTIME_ETHERNET)
		#include <SPI.h>
		#include <Ethernet.h>
		#include <EthernetUdp.h>
	#elif defined(EZTIME_WIFIESP)
		#include <WifiEsp.h>
		#include <WifiEspUdp.h>
	#else
		#include <WiFi.h>
		#include <WiFiUdp.h>
	#endif

	#ifndef EZTIME_ETHERNET
		#ifndef EZTIME_WIFIESP
			typedef WiFiUDP EzTimeUdp;
		#else
			typedef WiFiEspUDP EzTimeUdp;
		#endif
	#else
		typedef EthernetUDP EzTimeUdp;
	#endif
#endif

#if defined(EZTIME_MAX_DEBUGLEVEL_NONE)
	#define	err(args...) 		""
	#define	errln(args...) 		""
	#define	info(args...) 		""
	#define	infoln(args...) 	""
	#define	debug(args...) 		""
	#define	debugln(args...) 	""
#elif defined(EZTIME_MAX_DEBUGLEVEL_ERROR)
	#define	err(args...) 		if (_debug_level >= ERROR) _debug_device->print(args)
	#define	errln(args...) 		if (_debug_level >= ERROR) _debug_device->println(args)
	#define	info(args...) 		""
	#define	infoln(args...) 	""
	#define	debug(args...) 		""
	#define	debugln(args...) 	""
#elif defined(EZTIME_MAX_DEBUGLEVEL_INFO)
	#define	err(args...) 		if (_debug_level >= ERROR) _debug_device->print(args)
	#define	errln(args...) 		if (_debug_level >= ERROR) _debug_device->println(args)
	#define	info(args...) 		if (_debug_level >= INFO) _debug_device->print(args)
	#define	infoln(args...) 	if (_debug_level >= INFO) _debug_device->println(args)
	#define	debug(args...) 		""
	#define	debugln(args...) 	""
#else		// nothing specified compiles everything in.
	#define	err(args...) 		if (_debug_level >= ERROR) _debug_device->print(args)
	#define	errln(args...) 		if (_debug_level >= ERROR) _debug_device->println(args)
	#define	info(args...) 		if (_debug_level >= INFO) _debug_device->print(args)
	#define	infoln(args...) 	if (_debug_level >= INFO) _debug_device->println(args)
	#define	debug(args...) 		if (_debug_level >= DEBUG) _debug_device->print(args)
	#define	debugln(args...) 	if (_debug_level >= DEBUG) _debug_device->println(args)
#endif


const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0

class Timezone;

// The private things go in an anonymous namespace
namespace {

	ezError_t _last_error = NO_ERROR;
	String _server_error = "";
	ezDebugLevel_t _debug_level = NONE;
	Print *_debug_device = (Print *)&Serial;
	ezEvent_t _events[MAX_EVENTS];
	ezTime_t _last_sync_time = 0;
	ezTime_t _last_read_t = 0;
	uint32_t _last_sync_millis = 0;
	uint16_t _last_read_ms;
	timeStatus_t _time_status;
	bool _initialised = false;

	typedef struct {
		bool has_dst;
		int16_t std_offset;
		int16_t dst_offset;
		uint8_t start_month;
		uint8_t start_week;
		uint8_t start_dow;
		uint8_t start_time_hr;
		uint8_t start_time_min;
		uint8_t end_month;
		uint8_t end_week;
		uint8_t end_dow;
		uint8_t end_time_hr;
		uint8_t end_time_min;
		uint8_t stdname_end;
		uint8_t dstname_begin;
		uint8_t dstname_end;
	} ezPosixRule_t;

	#ifdef EZTIME_NETWORK_ENABLE
		uint16_t _ntp_interval = NTP_INTERVAL;
		String _ntp_servers[3] = { NTP_SERVER, NTP_SERVER_2, NTP_SERVER_3 };
		uint32_t _server_request_counter = 0;

		typedef enum {
			SERVER_PHASE_IDLE,
			SERVER_PHASE_WAIT_RESPONSE,
			SERVER_PHASE_WAIT_LIST_CHALLENGE,
			SERVER_PHASE_WAIT_LIST_CHUNKS,
			SERVER_PHASE_DONE
		} ezServerPhase_t;

		typedef struct {
			ezAsyncStatus_t status;
			ezAsyncType_t type;
			ezServerPhase_t phase;
			EzTimeUdp udp;
			unsigned long started;
			String query;
			String request_id;
			String result;
			String info_type;
			String target;
			Timezone *timezone;
			bool allow_ext_geoip_fallback;
			String *chunks;
			bool *received;
			uint16_t total_chunks;
			uint16_t received_count;
		} ezServerAsyncState_t;

		typedef enum {
			NTP_PHASE_IDLE,
			NTP_PHASE_WAIT_RESPONSE
		} ezNtpPhase_t;

		typedef struct {
			bool active;
			ezNtpPhase_t phase;
			EzTimeUdp udp;
			unsigned long started;
			uint8_t server_index;
			byte buffer[NTP_PACKET_SIZE];
		} ezNtpAsyncState_t;

		ezServerAsyncState_t _server_async = { ASYNC_IDLE, ASYNC_NONE, SERVER_PHASE_IDLE, EzTimeUdp(), 0, "", "", "", "", "", NULL, false, NULL, NULL, 0, 0 };
		ezNtpAsyncState_t _ntp_async = { false, NTP_PHASE_IDLE, EzTimeUdp(), 0, 0, { 0 } };
	#endif

	void triggerError(const ezError_t err) {
		_last_error = err;
		if (_last_error) {
			err(F("ERROR: "));
			errln(ezt::errorString(err));
		}
	}

	String debugLevelString(const ezDebugLevel_t level) {
		switch (level) {
			case NONE: return 	F("NONE");
			case ERROR: return 	F("ERROR");
			case INFO: return 	F("INFO");
			default: return 	F("DEBUG");
		}
	}

	ezTime_t nowUTC(const bool update_last_read = true) {
		ezTime_t t;
		uint32_t m = millis();
		t = _last_sync_time + ((m - _last_sync_millis) / 1000);
		if (update_last_read) {
			_last_read_t = t;
			_last_read_ms = (m - _last_sync_millis) % 1000;
		}
		return t;
	}

	String int64ToString(const int64_t value) {
		char buffer[24];
		snprintf(buffer, sizeof(buffer), "%" PRId64, value);
		return String(buffer);
	}

	ezError_t classifyServerError(const String &server_error) {
		if (server_error.indexOf(F("Rate Limited")) >= 0 || server_error == F("Server Busy")) {
			return RATE_LIMITED;
		}
		if (server_error.indexOf(F("Invalid")) >= 0 || server_error.indexOf(F("Missing")) >= 0) {
			return INVALID_REQUEST;
		}
		return SERVER_ERROR;
	}

	#ifdef EZTIME_NETWORK_ENABLE

		void clearServerAsyncBuffers() {
			if (_server_async.chunks != NULL) {
				delete[] _server_async.chunks;
				_server_async.chunks = NULL;
			}
			if (_server_async.received != NULL) {
				delete[] _server_async.received;
				_server_async.received = NULL;
			}
			_server_async.total_chunks = 0;
			_server_async.received_count = 0;
		}

		void finishServerAsync(const bool success, const ezError_t err = NO_ERROR, const String &server_error = "") {
			_server_async.udp.stop();
			clearServerAsyncBuffers();
			_server_async.phase = SERVER_PHASE_DONE;
			if (success) {
				_server_async.status = ASYNC_SUCCESS;
				_last_error = NO_ERROR;
				_server_error = "";
			} else {
				if (server_error.length()) {
					_server_error = server_error;
				}
				triggerError(err ? err : SERVER_ERROR);
				_server_async.status = ASYNC_ERROR;
			}
		}

		bool networkReady() {
			#ifndef EZTIME_ETHERNET
				if (WiFi.status() != WL_CONNECTED) {
					triggerError(NO_NETWORK);
					return false;
				}
			#endif
			return true;
		}

		bool beginServerQuery(EzTimeUdp &udp, const String &query, unsigned long &started) {
			if (!networkReady()) {
				return false;
			}

			udp.flush();
			udp.begin(TIMEZONED_LOCAL_PORT);
			started = millis();
			udp.beginPacket(TIMEZONED_REMOTE_HOST, TIMEZONED_REMOTE_PORT);
			udp.write((const uint8_t*)query.c_str(), query.length());
			udp.endPacket();
			return true;
		}

		bool beginServerQueryNonBlocking(EzTimeUdp &udp, const String &query, unsigned long &started) {
			if (!networkReady()) {
				return false;
			}

			udp.flush();
			udp.begin(TIMEZONED_LOCAL_PORT);
			started = millis();
			if (!udp.beginPacket(TIMEZONED_REMOTE_HOST, TIMEZONED_REMOTE_PORT)) {
				triggerError(CONNECT_FAILED);
				return false;
			}
			udp.write((const uint8_t*)query.c_str(), query.length());
			if (!udp.endPacket()) {
				udp.stop();
				triggerError(CONNECT_FAILED);
				return false;
			}
			return true;
		}

		bool waitForServerPacket(EzTimeUdp &udp, String &packet, const unsigned long started, const uint16_t timeout = TIMEZONED_TIMEOUT) {
			while (!udp.parsePacket()) {
				delay(1);
				if (millis() - started > timeout) {
					udp.stop();
					triggerError(TIMEOUT);
					return false;
				}
			}

			packet = "";
			while (udp.available()) {
				packet += (char)udp.read();
			}
			return true;
		}

		String nextRequestId() {
			char buffer[13];
			uint32_t stamp = millis() ^ (++_server_request_counter * 2654435761UL);
			snprintf(buffer, sizeof(buffer), "%08" PRIx32 "%04" PRIx16, stamp, (uint16_t)(_server_request_counter & 0xFFFF));
			return String(buffer);
		}

		bool startsWithIgnoreCase(const String &value, const String &prefix) {
			if (value.length() < prefix.length()) {
				return false;
			}
			return value.substring(0, prefix.length()).equalsIgnoreCase(prefix);
		}

		String trimCopy(const String &value) {
			String out = value;
			out.trim();
			return out;
		}

		bool parseInfoResponse(const String &response, String &payload) {
			if (!startsWithIgnoreCase(response, F("INFO OK "))) {
				return false;
			}
			payload = response.substring(8);
			return true;
		}

		#ifdef EZTIME_SERVER_LIST_ENABLE

			typedef struct {
				String rid;
				uint16_t total;
				uint16_t index;
				uint16_t length;
				uint32_t crc;
				bool has_crc;
				String chunk;
			} ezChunkPacket_t;

			uint32_t crc32Update(uint32_t crc, const uint8_t data) {
				crc ^= data;
				for (uint8_t bit = 0; bit < 8; bit++) {
					crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
				}
				return crc;
			}

			uint32_t crc32String(const String &value) {
				uint32_t crc = 0xFFFFFFFFUL;
				for (uint16_t i = 0; i < value.length(); i++) {
					crc = crc32Update(crc, (uint8_t)value[i]);
				}
				return crc ^ 0xFFFFFFFFUL;
			}

			bool parseChunkPacket(const String &packet, ezChunkPacket_t &chunk_info) {
				if (packet.length() < 5 || packet[0] != '<' || packet[packet.length() - 1] != '>') {
					return false;
				}

				int separator = packet.indexOf('|');
				if (separator < 0) {
					return false;
				}

				String metadata = packet.substring(1, separator);
				chunk_info.chunk = packet.substring(separator + 1, packet.length() - 1);
				chunk_info.rid = "";
				chunk_info.total = 0;
				chunk_info.index = 0;
				chunk_info.length = chunk_info.chunk.length();
				chunk_info.crc = 0;
				chunk_info.has_crc = false;

				if (metadata.startsWith("RID=")) {
					int start = 0;
					while (start < metadata.length()) {
						int end = metadata.indexOf(';', start);
						if (end < 0) {
							end = metadata.length();
						}
						String part = metadata.substring(start, end);
						int equals = part.indexOf('=');
						if (equals > 0) {
							String key = part.substring(0, equals);
							String value = part.substring(equals + 1);
							if (key == "RID") chunk_info.rid = value;
							else if (key == "TOT") chunk_info.total = value.toInt();
							else if (key == "IDX") chunk_info.index = value.toInt();
							else if (key == "LEN") chunk_info.length = value.toInt();
							else if (key == "CRC") {
								chunk_info.crc = strtoul(value.c_str(), NULL, 16);
								chunk_info.has_crc = true;
							}
						}
						start = end + 1;
					}
				} else {
					int colon = metadata.indexOf(':');
					if (colon < 0) {
						return false;
					}
					chunk_info.total = metadata.substring(0, colon).toInt();
					chunk_info.index = metadata.substring(colon + 1).toInt();
				}

				return chunk_info.total > 0 && chunk_info.index > 0;
			}

			bool validateChunkPacket(const ezChunkPacket_t &chunk_info, const String &expected_rid) {
				if (expected_rid.length() && chunk_info.rid.length() && chunk_info.rid != expected_rid) {
					return false;
				}
				if (!chunk_info.total || !chunk_info.index || chunk_info.index > chunk_info.total) {
					return false;
				}
				if (chunk_info.length != chunk_info.chunk.length()) {
					return false;
				}
				if (chunk_info.has_crc && crc32String(chunk_info.chunk) != chunk_info.crc) {
					return false;
				}
				return true;
			}

			bool receiveChunkedResponse(EzTimeUdp &udp, const String &expected_rid, String &response, const unsigned long started) {
				String first_packet;
				if (!waitForServerPacket(udp, first_packet, started)) {
					return false;
				}

				ezChunkPacket_t first_chunk;
				if (!parseChunkPacket(first_packet, first_chunk) || !validateChunkPacket(first_chunk, expected_rid)) {
					triggerError(INVALID_DATA);
					udp.stop();
					return false;
				}

				response = "";
				const uint16_t total_chunks = first_chunk.total;
				String *chunks = new String[total_chunks];
				if (chunks == NULL) {
					triggerError(CONNECT_FAILED);
					udp.stop();
					return false;
				}

				bool *received = new bool[total_chunks];
				if (received == NULL) {
					delete[] chunks;
					triggerError(CONNECT_FAILED);
					udp.stop();
					return false;
				}

				for (uint16_t i = 0; i < total_chunks; i++) {
					received[i] = false;
				}

				chunks[first_chunk.index - 1] = first_chunk.chunk;
				received[first_chunk.index - 1] = true;
				uint16_t received_count = 1;

				while (received_count < total_chunks) {
					String packet;
					if (!waitForServerPacket(udp, packet, millis(), TIMEZONED_TIMEOUT)) {
						delete[] received;
						delete[] chunks;
						return false;
					}

					ezChunkPacket_t chunk_info;
					if (!parseChunkPacket(packet, chunk_info) || !validateChunkPacket(chunk_info, expected_rid) || chunk_info.total != total_chunks) {
						delete[] received;
						delete[] chunks;
						triggerError(INVALID_DATA);
						udp.stop();
						return false;
					}

					if (!received[chunk_info.index - 1]) {
						chunks[chunk_info.index - 1] = chunk_info.chunk;
						received[chunk_info.index - 1] = true;
						received_count++;
					}
				}

				for (uint16_t i = 0; i < total_chunks; i++) {
					response += chunks[i];
				}

				delete[] received;
				delete[] chunks;
				udp.stop();
				return true;
			}

		#endif

	#endif

	bool parsePosixRule(const String &posix, ezPosixRule_t &rule) {
			memset(&rule, 0, sizeof(rule));
			rule.start_time_hr = 2;
			rule.end_time_hr = 2;
			rule.stdname_end = posix.length() ? posix.length() - 1 : 0;
			rule.dstname_begin = posix.length();
			rule.dstname_end = posix.length();

			int8_t offset_hr = 0;
			uint8_t offset_min = 0;
			int8_t dst_shift_hr = 1;
			uint8_t dst_shift_min = 0;

			enum posix_state_e {STD_NAME, OFFSET_HR, OFFSET_MIN, DST_NAME, DST_SHIFT_HR, DST_SHIFT_MIN, START_MONTH, START_WEEK, START_DOW, START_TIME_HR, START_TIME_MIN, END_MONTH, END_WEEK, END_DOW, END_TIME_HR, END_TIME_MIN};
			posix_state_e state = STD_NAME;
			bool ignore_nums = false;
			uint8_t strpos = 0;

			while (strpos < posix.length()) {
				char c = (char)posix[strpos];

				if (c && state == STD_NAME) {
					if (c == '<') ignore_nums = true;
					if (c == '>') ignore_nums = false;
					if (!ignore_nums && (isDigit(c) || c == '-' || c == '+')) {
						state = OFFSET_HR;
						rule.stdname_end = strpos - 1;
					}
				}
				if (c && state == OFFSET_HR) {
					if (c == '+') {
					} else if (c == ':') {
						state = OFFSET_MIN;
						c = 0;
					} else if (c != '-' && !isDigit(c)) {
						state = DST_NAME;
						rule.dstname_begin = strpos;
					} else if (!offset_hr) {
						offset_hr = atoi(posix.c_str() + strpos);
					}
				}
				if (c && state == OFFSET_MIN) {
					if (!isDigit(c)) {
						state = DST_NAME;
						rule.dstname_begin = strpos;
						ignore_nums = false;
					} else if (!offset_min) {
						offset_min = atoi(posix.c_str() + strpos);
					}
				}
				if (c && state == DST_NAME) {
					if (c == '<') ignore_nums = true;
					if (c == '>') ignore_nums = false;
					if (c == ',') {
						state = START_MONTH;
						rule.dstname_end = strpos - 1;
						c = 0;
					} else if (!ignore_nums && (c == '-' || isDigit(c))) {
						state = DST_SHIFT_HR;
						rule.dstname_end = strpos - 1;
					}
				}
				if (c && state == DST_SHIFT_HR) {
					if (c == ':') {
						state = DST_SHIFT_MIN;
						c = 0;
					} else if (c == ',') {
						state = START_MONTH;
						c = 0;
					} else if (dst_shift_hr == 1) {
						dst_shift_hr = atoi(posix.c_str() + strpos);
					}
				}
				if (c && state == DST_SHIFT_MIN) {
					if (c == ',') {
						state = START_MONTH;
						c = 0;
					} else if (!dst_shift_min) {
						dst_shift_min = atoi(posix.c_str() + strpos);
					}
				}
				if (c && state == START_MONTH) {
					if (c == '.') {
						state = START_WEEK;
						c = 0;
					} else if (c != 'M' && !rule.start_month) {
						rule.start_month = atoi(posix.c_str() + strpos);
					}
				}
				if (c && state == START_WEEK) {
					if (c == '.') {
						state = START_DOW;
						c = 0;
					} else rule.start_week = c - '0';
				}
				if (c && state == START_DOW) {
					if (c == '/') {
						state = START_TIME_HR;
						c = 0;
					} else if (c == ',') {
						state = END_MONTH;
						c = 0;
					} else rule.start_dow = c - '0';
				}
				if (c && state == START_TIME_HR) {
					if (c == ':') {
						state = START_TIME_MIN;
						c = 0;
					} else if (c == ',') {
						state = END_MONTH;
						c = 0;
					} else if (rule.start_time_hr == 2) {
						rule.start_time_hr = atoi(posix.c_str() + strpos);
					}
				}
				if (c && state == START_TIME_MIN) {
					if (c == ',') {
						state = END_MONTH;
						c = 0;
					} else if (!rule.start_time_min) {
						rule.start_time_min = atoi(posix.c_str() + strpos);
					}
				}
				if (c && state == END_MONTH) {
					if (c == '.') {
						state = END_WEEK;
						c = 0;
					} else if (c != 'M' && !rule.end_month) {
						rule.end_month = atoi(posix.c_str() + strpos);
					}
				}
				if (c && state == END_WEEK) {
					if (c == '.') {
						state = END_DOW;
						c = 0;
					} else rule.end_week = c - '0';
				}
				if (c && state == END_DOW) {
					if (c == '/') {
						state = END_TIME_HR;
						c = 0;
					} else rule.end_dow = c - '0';
				}
				if (c && state == END_TIME_HR) {
					if (c == ':') {
						state = END_TIME_MIN;
						c = 0;
					} else if (rule.end_time_hr == 2) {
						rule.end_time_hr = atoi(posix.c_str() + strpos);
					}
				}
				if (c && state == END_TIME_MIN) {
					if (!rule.end_time_min) {
						rule.end_time_min = atoi(posix.c_str() + strpos);
					}
				}
				strpos++;
			}

			rule.std_offset = (offset_hr < 0) ? offset_hr * 60 - offset_min : offset_hr * 60 + offset_min;
			if (!rule.start_month) {
				rule.has_dst = false;
				rule.dst_offset = rule.std_offset;
				return true;
			}

			rule.has_dst = true;
			rule.dst_offset = rule.std_offset - dst_shift_hr * 60 - dst_shift_min;
			return true;
		}

	void computeDstTransitions(const ezPosixRule_t &rule, const uint16_t year, ezTime_t &dst_start_utc, ezTime_t &dst_end_utc) {
			dst_start_utc = ezt::makeOrdinalTime(rule.start_time_hr, rule.start_time_min, 0, rule.start_week, rule.start_dow + 1, rule.start_month, year);
			dst_end_utc = ezt::makeOrdinalTime(rule.end_time_hr, rule.end_time_min, 0, rule.end_week, rule.end_dow + 1, rule.end_month, year);
			dst_start_utc += rule.std_offset * 60LL;
			dst_end_utc += rule.dst_offset * 60LL;
		}

	ezTime_t ntpReferenceUnixTime() {
		if (_time_status == timeSet && _last_sync_time > 0) {
			return nowUTC(false);
		}
		ezTime_t compiled = ezt::compileTime();
		if (compiled > 0) {
			return compiled;
		}
		return (ezTime_t)1735689600LL; // 2025-01-01T00:00:00Z fallback
	}

	bool ntpSecondsToUnixTime(const uint32_t ntp_seconds, ezTime_t &unix_time_out) {
		int64_t unix_time = (int64_t)ntp_seconds - EZTIME_NTP_EPOCH_OFFSET;

		#if EZTIME_TIME_BITS == 64
			const int64_t reference = (int64_t)ntpReferenceUnixTime();
			const int64_t half_era = EZTIME_NTP_ERA_SECONDS / 2;
			while ((reference - unix_time) > half_era) unix_time += EZTIME_NTP_ERA_SECONDS;
			while ((unix_time - reference) > half_era) unix_time -= EZTIME_NTP_ERA_SECONDS;
		#else
			if (unix_time > INT32_MAX || unix_time < INT32_MIN) {
				return false;
			}
		#endif

		unix_time_out = (ezTime_t)unix_time;
		return true;
	}

}



namespace ezt {

	////////// Error handing

	String errorString(const ezError_t err /* = LAST_ERROR */) {
		switch (err) {
			case NO_ERROR: return				F("OK");
			case LAST_ERROR: return 			errorString(_last_error);
			case NO_NETWORK: return				F("No network");
			case TIMEOUT: return 				F("Timeout");
			case CONNECT_FAILED: return 		F("Connect Failed");
			case DATA_NOT_FOUND: return			F("Data not found");
			case LOCKED_TO_UTC: return			F("Locked to UTC");
			case NO_CACHE_SET: return			F("No cache set");
			case CACHE_TOO_SMALL: return		F("Cache too small");
			case TOO_MANY_EVENTS: return		F("Too many events");
			case INVALID_DATA: return			F("Invalid data received from NTP server");
			case SERVER_ERROR: return			_server_error;
			case INVALID_REQUEST: return		F("Invalid request");
			case RATE_LIMITED: return			F("Rate limited");
			case PROTOCOL_ERROR: return			F("Protocol error");
			case CHALLENGE_FAILED: return		F("Challenge failed");
			case CHUNK_ERROR: return			F("Invalid chunk data");
			case CRC_ERROR: return				F("Chunk CRC failed");
			case ASYNC_BUSY: return				F("Another async request is already running");
			default: return						F("Unkown error");
		}
	}

	ezError_t error(const bool reset /* = false */) {
		ezError_t tmp = _last_error;
		if (reset) _last_error = NO_ERROR;
		return tmp;
	}

	void setDebug(const ezDebugLevel_t level) {
		setDebug(level, *_debug_device);
	}

	void setDebug(const ezDebugLevel_t level, Print &device) { 
		_debug_level = level;
		_debug_device = &device;
		info(F("\r\nezTime debug level set to "));
		infoln(debugLevelString(level));
	}


	// The include below includes the dayStr, dayShortStr, monthStr and monthShortStr from the appropriate language file
	// in the /src/lang subdirectory.


	#ifdef EZTIME_LANGUAGE
		#define XSTR(x) #x
		#define STR(x) XSTR(x)
		#include STR(lang/EZTIME_LANGUAGE)
	#else
		#include "lang/EN"
	#endif

	//

	timeStatus_t timeStatus() { return _time_status; }

	void events() {
		if (!_initialised) {
			for (uint8_t n = 0; n < MAX_EVENTS; n++) _events[n] = { 0, NULL };
			#ifdef EZTIME_NETWORK_ENABLE
				if (_ntp_interval) updateNTP();	// Start the cycle of updateNTP running and then setting an event for its next run
			#endif
			_initialised = true;
		}
		#ifdef EZTIME_NETWORK_ENABLE
			pollAsync();
		#endif
		// See if any events are due
		for (uint8_t n = 0; n < MAX_EVENTS; n++) {
			if (_events[n].function && nowUTC(false) >= _events[n].time) {
				debug(F("Running event (#")); debug(n + 1); debug(F(") set for ")); debugln(UTC.dateTime(_events[n].time));
				void (*tmp)() = _events[n].function;
				_events[n] = { 0, NULL };		// reset the event
				(tmp)();						// execute the function
			}
		}
		yield();
	}

	void deleteEvent(const uint8_t event_handle) { 
		if (event_handle && event_handle <= MAX_EVENTS) {
			debug(F("Deleted event (#")); debug(event_handle); debug(F("), set for ")); debugln(UTC.dateTime(_events[event_handle - 1].time));	
			_events[event_handle - 1] = { 0, NULL };
		}
	}

	void deleteEvent(void (*function)()) { 
		for (uint8_t n = 0; n< MAX_EVENTS; n++) {
			if (_events[n].function == function) {
				debug(F("Deleted event (#")); debug(n + 1); debug(F("), set for ")); debugln(UTC.dateTime(_events[n].time));
				_events[n] = { 0, NULL };
			}
		}
	}

	void breakTime(const ezTime_t timeInput, tmElements_t &tm){
		// break the given ezTime_t into time components
		// this is a more compact version of the C library localtime function
		// note that year is offset from 1970 !!!

		uint16_t year = 0;
		uint8_t month = 0;
		uint8_t monthLength = 0;
		uint64_t time = timeInput < 0 ? 0 : (uint64_t)timeInput;
		uint64_t days = 0;

		tm.Second = time % 60;
		time /= 60; // now it is minutes
		tm.Minute = time % 60;
		time /= 60; // now it is hours
		tm.Hour = time % 24;
		time /= 24; // now it is days
		tm.Wday = ((time + 4) % 7) + 1;  // Sunday is day 1

		while ((days + (LEAP_YEAR(year) ? 366 : 365)) <= time) {
			days += LEAP_YEAR(year) ? 366 : 365;
			year++;
		}
		tm.Year = year; // year is offset from 1970

		time -= days; // now it is days in this year, starting at 0

		for (month = 0; month < 12; month++) {
			if (month == 1) { // february
				if (LEAP_YEAR(year)) {
					monthLength = 29;
				} else {
					monthLength = 28;
				}
			} else {
				monthLength = monthDays[month];
			}

			if (time >= monthLength) {
				time -= monthLength;
			} else {
				break;
			}
		}

		tm.Month = month + 1;  // jan is month 1
		tm.Day = time + 1;     // day of month
	}

	ezTime_t makeTime(const uint8_t hour, const uint8_t minute, const uint8_t second, const uint8_t day, const uint8_t month, const uint16_t year) {
		tmElements_t tm;
		tm.Hour = hour;
		tm.Minute = minute;
		tm.Second = second;
		tm.Day = day;
		tm.Month = month;
		if (year >= 1970) {
			tm.Year = year - 1970;
		} else {
			tm.Year = year;
		}
		return makeTime(tm);
	}

	ezTime_t makeTime(tmElements_t &tm){
		// assemble time elements into ezTime_t
		// note year argument is offset from 1970

		uint16_t i;
		uint64_t seconds = (uint64_t)tm.Year * (uint64_t)SECS_PER_DAY * 365ULL;

		for (i = 0; i < tm.Year; i++) {
			if (LEAP_YEAR(i)) {
				seconds += SECS_PER_DAY;   // add extra days for leap years
			}
		}

		// add days for this year, months start from 1
		for (i = 1; i < tm.Month; i++) {
			if ((i == 2) && LEAP_YEAR(tm.Year)) {
				seconds += SECS_PER_DAY * 29ULL;
			} else {
				seconds += SECS_PER_DAY * (uint64_t)monthDays[i - 1];  // monthDay array starts from 0
			}
		}

		seconds += (uint64_t)(tm.Day - 1) * (uint64_t)SECS_PER_DAY;
		seconds += (uint64_t)tm.Hour * 3600ULL;
		seconds += (uint64_t)tm.Minute * 60ULL;
		seconds += tm.Second;

		return (ezTime_t)seconds;
	}

	// makeOrdinalTime allows you to resolve "second thursday in September in 2018" into a number of seconds since 1970
	// (Very useful for the timezone calculations that ezTime does internally) 
	// If ordinal is 0 or 5 it is taken to mean "the last $wday in $month"
	ezTime_t makeOrdinalTime(const uint8_t hour, const uint8_t minute, uint8_t const second, uint8_t ordinal, const uint8_t wday, const uint8_t month, uint16_t year) {
		if (year < 1970) year = 1970 + year;		// interpret small values as years since 1970
		uint8_t m = month;   
		uint8_t w = ordinal;
		if (w == 5) {	
			ordinal = 0;
			w = 0;
		}
		if (w == 0) {			// is this a "Last week" rule?
			if (++m > 12) {		// yes, for "Last", go to the next month
				m = 1;
				++year;
			}
			w = 1;               // and treat as first week of next month, subtract 7 days later
		}
		ezTime_t t = makeTime(hour, minute, second, 1, m, year);
		// add offset from the first of the month to weekday, and offset for the given week
		t += ( (wday - UTC.weekday(t) + 7) % 7 + (w - 1) * 7 ) * SECS_PER_DAY;
		// back up a week if this is a "Last" rule
		if (ordinal == 0) t -= 7 * SECS_PER_DAY;
		return t;
	}

	String zeropad(const uint32_t number, const uint8_t length) {
		String out;
		out.reserve(length);
		out = String(number);
		while (out.length() < length) out = "0" + out;
		return out;
	}

	String urlEncode(const String str) {
		const char hex[] = "0123456789ABCDEF";
		String out;
		out.reserve(str.length() * 3);
		for (uint16_t i = 0; i < str.length(); i++) {
			const uint8_t c = (uint8_t)str[i];
			if ((c >= 'A' && c <= 'Z') ||
				(c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~') {
				out += (char)c;
			} else {
				out += '%';
				out += hex[(c >> 4) & 0x0F];
				out += hex[c & 0x0F];
			}
		}
		return out;
	}

	ezTime_t compileTime(const String compile_date /* = __DATE__ */, const String compile_time /* = __TIME__ */) {
	
		uint8_t hrs = compile_time.substring(0,2).toInt();
		uint8_t min = compile_time.substring(3,5).toInt();
		uint8_t sec = compile_time.substring(6).toInt();
		uint8_t day = compile_date.substring(4,6).toInt();
		int16_t year = compile_date.substring(7).toInt();
		String iterate_month;
		for (uint8_t month = 1; month < 13; month++) {
			iterate_month = monthStr(month);
			if ( iterate_month.substring(0,3) == compile_date.substring(0,3) ) {
				return makeTime(hrs, min, sec, day, month, year);
			}
		}
		return 0;
	}

	bool secondChanged() {
		ezTime_t t = nowUTC(false);
		if (_last_read_t != t) return true;
		return false;
	}

	bool minuteChanged() {
		ezTime_t t = nowUTC(false);
		if (_last_read_t / 60 != t / 60) return true;
		return false;
	}


	#ifdef EZTIME_NETWORK_ENABLE

		bool queryNTPWithFallbacks(ezTime_t &t, unsigned long &measured_at) {
			ezError_t last_error = NO_ERROR;
			String last_server_error = "";
			bool tried_any = false;

			for (uint8_t i = 0; i < 3; i++) {
				String server = _ntp_servers[i];
				server.trim();
				if (!server.length()) continue;

				tried_any = true;
				if (queryNTP(server, t, measured_at)) {
					if (i > 0) {
						info(F("NTP fallback succeeded with "));
						infoln(server);
					}
					return true;
				}

				last_error = _last_error;
				last_server_error = _server_error;

				for (uint8_t next = i + 1; next < 3; next++) {
					String next_server = _ntp_servers[next];
					next_server.trim();
					if (next_server.length()) {
						info(F("NTP query failed, trying fallback "));
						info(next + 1);
						info(F(": "));
						infoln(next_server);
						break;
					}
				}
			}

			if (!tried_any) {
				triggerError(INVALID_DATA);
				return false;
			}

			_last_error = last_error;
			_server_error = last_server_error;
			return false;
		}

		bool beginNtpAsyncRequest(const uint8_t server_index) {
			String server = _ntp_servers[server_index];
			server.trim();
			if (!server.length()) {
				triggerError(INVALID_DATA);
				return false;
			}

			#ifndef EZTIME_ETHERNET
				if (WiFi.status() != WL_CONNECTED) { triggerError(NO_NETWORK); return false; }
			#endif

			memset(_ntp_async.buffer, 0, NTP_PACKET_SIZE);
			_ntp_async.buffer[0] = 0b11100011;
			_ntp_async.buffer[1] = 0;
			_ntp_async.buffer[2] = 9;
			_ntp_async.buffer[3] = 0xEC;
			_ntp_async.buffer[12] = 'X';
			_ntp_async.buffer[13] = 'E';
			_ntp_async.buffer[14] = 'Z';
			_ntp_async.buffer[15] = 'T';

			_ntp_async.udp.flush();
			_ntp_async.udp.begin(NTP_LOCAL_PORT);
			_ntp_async.started = millis();
			if (!_ntp_async.udp.beginPacket(server.c_str(), 123)) {
				_ntp_async.udp.stop();
				triggerError(CONNECT_FAILED);
				return false;
			}
			_ntp_async.udp.write(_ntp_async.buffer, NTP_PACKET_SIZE);
			if (!_ntp_async.udp.endPacket()) {
				_ntp_async.udp.stop();
				triggerError(CONNECT_FAILED);
				return false;
			}

			info(F("Querying "));
			info(server);
			info(F(" ... "));

			_ntp_async.active = true;
			_ntp_async.phase = NTP_PHASE_WAIT_RESPONSE;
			_ntp_async.server_index = server_index;
			return true;
		}

		void updateNTP() {
			deleteEvent(updateNTP);	// Delete any events pointing here, in case called manually
			if (_ntp_async.active) {
				return;
			}
			for (uint8_t i = 0; i < 3; i++) {
				String server = _ntp_servers[i];
				server.trim();
				if (!server.length()) {
					continue;
				}
				if (beginNtpAsyncRequest(i)) {
					return;
				}
			}
			if (nowUTC(false) > _last_sync_time + _ntp_interval + NTP_STALE_AFTER) {
				_time_status = timeNeedsSync;
			}
			UTC.setEvent(updateNTP, nowUTC(false) + NTP_RETRY);
		}

		// This is a nice self-contained NTP routine if you need one: feel free to use it.
		// It gives you the seconds since 1970 (unix epoch) and the millis() on your system when 
		// that happened (by deducting fractional seconds and estimated network latency).
		bool queryNTP(const String server, ezTime_t &t, unsigned long &measured_at) {
			info(F("Querying "));
			info(server);
			info(F(" ... "));

			#ifndef EZTIME_ETHERNET
				if (WiFi.status() != WL_CONNECTED) { triggerError(NO_NETWORK); return false; }
				#ifndef EZTIME_WIFIESP
					WiFiUDP udp;
				#else
					WiFiEspUDP udp;
				#endif
			#else
				EthernetUDP udp;
			#endif
	
			// Send NTP packet
			byte buffer[NTP_PACKET_SIZE];
			memset(buffer, 0, NTP_PACKET_SIZE);
			buffer[0] = 0b11100011;		// LI, Version, Mode
			buffer[1] = 0;   			// Stratum, or type of clock
			buffer[2] = 9;				// Polling Interval (9 = 2^9 secs = ~9 mins, close to our 10 min default)
			buffer[3] = 0xEC;			// Peer Clock Precision
										// 8 bytes of zero for Root Delay & Root Dispersion
			buffer[12]  = 'X';			// "kiss code", see RFC5905
			buffer[13]  = 'E';			// (codes starting with 'X' are not interpreted)
			buffer[14]  = 'Z';
			buffer[15]  = 'T';	
	
			udp.flush();
			udp.begin(NTP_LOCAL_PORT);
			unsigned long started = millis();
			udp.beginPacket(server.c_str(), 123); //NTP requests are to port 123
			udp.write(buffer, NTP_PACKET_SIZE);
			udp.endPacket();

			// Wait for packet or return false with timed out
			while (!udp.parsePacket()) {
				delay (1);
				if (millis() - started > NTP_TIMEOUT) {
					udp.stop();	
					triggerError(TIMEOUT); 
					return false;
				}
			}
			udp.read(buffer, NTP_PACKET_SIZE);
			udp.stop();													// On AVR there's only very limited sockets, we want to free them when done.
	
			//print out received packet for debug
			int i;
			debug(F("Received data:"));
			for (i = 0; i < NTP_PACKET_SIZE; i++) {
				if ((i % 4) == 0) {
					debugln();
					debug(String(i) + ": ");
				}
				debug(buffer[i], HEX);
				debug(F(", "));
			}
			debugln();

			//prepare timestamps
			uint32_t highWord, lowWord;	
			highWord = ( buffer[16] << 8 | buffer[17] ) & 0x0000FFFF;
			lowWord = ( buffer[18] << 8 | buffer[19] ) & 0x0000FFFF;
			uint32_t reftsSec = highWord << 16 | lowWord;				// reference timestamp seconds

			highWord = ( buffer[32] << 8 | buffer[33] ) & 0x0000FFFF;
			lowWord = ( buffer[34] << 8 | buffer[35] ) & 0x0000FFFF;
			uint32_t rcvtsSec = highWord << 16 | lowWord;				// receive timestamp seconds

			highWord = ( buffer[40] << 8 | buffer[41] ) & 0x0000FFFF;
			lowWord = ( buffer[42] << 8 | buffer[43] ) & 0x0000FFFF;
			uint32_t secsSince1900 = highWord << 16 | lowWord;			// transmit timestamp seconds

			highWord = ( buffer[44] << 8 | buffer[45] ) & 0x0000FFFF;
			lowWord = ( buffer[46] << 8 | buffer[47] ) & 0x0000FFFF;
			uint32_t fraction = highWord << 16 | lowWord;				// transmit timestamp fractions	

			//check if received data makes sense
			//buffer[1] = stratum - should be 1..15 for valid reply
			//also checking that all timestamps are non-zero and receive timestamp seconds are <= transmit timestamp seconds
			if ((buffer[1] < 1) or (buffer[1] > 15) or (reftsSec == 0) or (rcvtsSec == 0) or (rcvtsSec > secsSince1900)) {
				// we got invalid packet
				triggerError(INVALID_DATA); 
				return false;
			}

			// Set the t and measured_at variables that were passed by reference
			uint32_t done = millis();
			info(F("success (round trip ")); info(done - started); infoln(F(" ms)"));
			if (!ntpSecondsToUnixTime(secsSince1900, t)) {
				triggerError(INVALID_DATA);
				return false;
			}
			uint16_t ms = fraction / 4294967UL;					// Turn 32 bit fraction into ms by dividing by 2^32 / 1000 
			measured_at = done - ((done - started) / 2) - ms;	// Assume symmetric network latency and return when we think the whole second was.
				
			return true;
		}

		void setInterval(const uint16_t seconds /* = 0 */) { 
			deleteEvent(updateNTP);
			_ntp_interval = seconds;
			if (seconds) UTC.setEvent(updateNTP, nowUTC(false) + _ntp_interval);
		}

		void setServer(const String ntp_server /* = NTP_SERVER */) { _ntp_servers[0] = ntp_server; }

		void setServers(const String ntp_server_1, const String ntp_server_2 /* = "" */, const String ntp_server_3 /* = "" */) {
			_ntp_servers[0] = ntp_server_1;
			_ntp_servers[1] = ntp_server_2;
			_ntp_servers[2] = ntp_server_3;
		}

		bool waitForSync(const uint16_t timeout /* = 0 */) {

			unsigned long start = millis();
		
			#if !defined(EZTIME_ETHERNET)
				if (WiFi.status() != WL_CONNECTED) {
					info(F("Waiting for WiFi ... "));
					while (WiFi.status() != WL_CONNECTED) {
						if ( timeout && (millis() - start) / 1000 > timeout ) { triggerError(TIMEOUT); return false;};
						events();
						delay(25);
					}
					infoln(F("connected"));
				}
			#endif

			if (_time_status != timeSet) {
				infoln(F("Waiting for time sync"));
				while (_time_status != timeSet) {
					if ( timeout && (millis() - start) / 1000 > timeout ) { triggerError(TIMEOUT); return false;};
					delay(250);
					events();
				}
				infoln(F("Time is in sync"));
			}
			return true;
		}
		
		ezTime_t lastNtpUpdateTime() { return _last_sync_time; }
	
	#endif // EZTIME_NETWORK_ENABLE

}


//
// Timezone class
//

Timezone::Timezone(const bool locked_to_UTC /* = false */) {
	_locked_to_UTC = locked_to_UTC;
	_posix = "UTC";
	#ifdef EZTIME_NETWORK_ENABLE
		#ifdef EZTIME_EXT_GEOIP_FALLBACK
			_geo_lookup_mode = GEOIP_LOOKUP_WITH_EXT_FALLBACK;
		#else
			_geo_lookup_mode = GEOIP_LOOKUP_ONLY;
		#endif
		#ifdef EZTIME_CACHE_EEPROM
			_cache_month = 0;
			_eeprom_address = -1;
		#endif
		#ifdef EZTIME_CACHE_NVS
			_cache_month = 0;
			_nvs_name = "";
			_nvs_key = "";
		#endif
		_olson = "";
	#endif
}

bool Timezone::setPosix(const String posix) {
	if (_locked_to_UTC) { triggerError(LOCKED_TO_UTC); return false; }
	_posix = posix;
	#ifdef EZTIME_NETWORK_ENABLE
		_olson = "";
	#endif
	return true;
}

ezTime_t Timezone::now() { return tzTime(); }

ezTime_t Timezone::tzTime(ezTime_t t /* = TIME_NOW */, ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	if (_locked_to_UTC) return nowUTC();	// just saving some time and memory
	String tzname;
	bool is_dst;
	int16_t offset;
	return tzTime(t, local_or_utc, tzname, is_dst, offset);
}

ezTime_t Timezone::tzTime(ezTime_t t, ezLocalOrUTC_t local_or_utc, String &tzname, bool &is_dst, int16_t &offset) {

	if (t == TIME_NOW) {
		t = nowUTC(); 
		local_or_utc = UTC_TIME;
	} else if (t == LAST_READ) {
		t = _last_read_t;
		local_or_utc = UTC_TIME;
	}
	
	ezPosixRule_t rule;
	parsePosixRule(_posix, rule);

	tzname = _posix.substring(0, rule.stdname_end + 1);
	if (!rule.has_dst) {
		if (tzname == "UTC" && rule.std_offset) tzname = "???";
		is_dst = false;
		offset = rule.std_offset;
	} else {
		tmElements_t tm;
		ezt::breakTime(t, tm);	
		ezTime_t dst_start;
		ezTime_t dst_end;
		computeDstTransitions(rule, tm.Year + 1970, dst_start, dst_end);

		if (local_or_utc == UTC_TIME) {
			// already UTC
		} else {
			dst_start -= rule.std_offset * 60LL;
			dst_end -= rule.dst_offset * 60LL;
		}
		
		if (dst_end > dst_start) {
			is_dst = (t >= dst_start && t < dst_end);
		} else {
			is_dst = !(t >= dst_end && t < dst_start);
		}

		if (is_dst) {
			offset = rule.dst_offset;
			tzname = _posix.substring(rule.dstname_begin, rule.dstname_end + 1);
		} else {
			offset = rule.std_offset;
		}
	}

	if (local_or_utc == LOCAL_TIME) {
		return t + offset * 60LL;
	} else {
		return t - offset * 60LL;
	}
}

String Timezone::getPosix() { return _posix; }

bool Timezone::hasDST() {
	ezPosixRule_t rule;
	parsePosixRule(_posix, rule);
	return rule.has_dst;
}

bool Timezone::nextDSTChange(ezTime_t &transition, ezTime_t from /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = UTC_TIME */) {
	ezPosixRule_t rule;
	parsePosixRule(_posix, rule);
	if (!rule.has_dst) {
		triggerError(DATA_NOT_FOUND);
		return false;
	}

	if (from == TIME_NOW) {
		from = nowUTC();
	}

	tmElements_t tm;
	ezt::breakTime(from, tm);
	for (uint8_t delta = 0; delta < 3; delta++) {
		ezTime_t dst_start_utc;
		ezTime_t dst_end_utc;
		computeDstTransitions(rule, tm.Year + 1970 + delta, dst_start_utc, dst_end_utc);

		ezTime_t first = dst_start_utc;
		ezTime_t second = dst_end_utc;
		if (second < first) {
			ezTime_t tmp = first;
			first = second;
			second = tmp;
		}
		if (first > from) {
			transition = (local_or_utc == LOCAL_TIME) ? tzTime(first, UTC_TIME) : first;
			return true;
		}
		if (second > from) {
			transition = (local_or_utc == LOCAL_TIME) ? tzTime(second, UTC_TIME) : second;
			return true;
		}
	}

	triggerError(DATA_NOT_FOUND);
	return false;
}

#ifdef EZTIME_NETWORK_ENABLE

	void Timezone::setGeoLookupMode(const ezGeoLookupMode_t mode) {
		_geo_lookup_mode = mode;
	}

	ezGeoLookupMode_t Timezone::getGeoLookupMode() const {
		return _geo_lookup_mode;
	}

	bool ezt::asyncBusy() {
		return _server_async.status == ASYNC_PENDING;
	}

	ezAsyncStatus_t ezt::asyncStatus() {
		return _server_async.status;
	}

	ezAsyncType_t ezt::asyncType() {
		return _server_async.type;
	}

	void ezt::cancelAsync() {
		_server_async.udp.stop();
		clearServerAsyncBuffers();
		_server_async.status = ASYNC_IDLE;
		_server_async.type = ASYNC_NONE;
		_server_async.phase = SERVER_PHASE_IDLE;
		_server_async.started = 0;
		_server_async.query = "";
		_server_async.request_id = "";
		_server_async.result = "";
		_server_async.info_type = "";
		_server_async.target = "";
		_server_async.timezone = NULL;
		_server_async.allow_ext_geoip_fallback = false;
	}

	String ezt::asyncResult() {
		return _server_async.result;
	}

	static bool beginGenericAsyncRequest(const ezAsyncType_t type, const String &query, Timezone *timezone = NULL, const bool allow_ext_geoip_fallback = false) {
		if (ezt::asyncBusy()) {
			triggerError(ASYNC_BUSY);
			return false;
		}

		ezt::cancelAsync();
		_server_async.type = type;
		_server_async.status = ASYNC_PENDING;
		_server_async.query = query;
		_server_async.timezone = timezone;
		_server_async.allow_ext_geoip_fallback = allow_ext_geoip_fallback;
		_server_async.phase = (type == ASYNC_LIST) ? SERVER_PHASE_WAIT_LIST_CHALLENGE : SERVER_PHASE_WAIT_RESPONSE;

		if (type == ASYNC_LIST) {
			_server_async.request_id = nextRequestId();
			_server_async.query += F("#rid=");
			_server_async.query += _server_async.request_id;
		}

		if (!beginServerQueryNonBlocking(_server_async.udp, _server_async.query, _server_async.started)) {
			ezt::cancelAsync();
			return false;
		}

		_last_error = NO_ERROR;
		_server_error = "";
		return true;
	}

	bool ezt::beginGetPublicIP() {
		return beginGenericAsyncRequest(ASYNC_GETIP, F("GETIP"));
	}

	bool ezt::beginInfo(const String infotype, const String target /* = "" */) {
		String normalized_type = trimCopy(infotype);
		if (!normalized_type.length()) {
			triggerError(INVALID_REQUEST);
			return false;
		}

		String query = F("INFO ");
		query += normalized_type;
		String normalized_target = trimCopy(target);
		if (normalized_target.length()) {
			query += ' ';
			query += normalized_target;
		}
		return beginGenericAsyncRequest(ASYNC_INFO, query);
	}

	bool Timezone::beginSetLocation(const String location /* = "GeoIP" */) {
		info(F("Timezone lookup for: "));
		info(location);
		info(F(" ... "));
		if (_locked_to_UTC) { triggerError(LOCKED_TO_UTC); return false; }

		String query = location;
		String normalized = trimCopy(location);
		bool allow_ext_geoip_fallback = false;
		if (!normalized.length() || normalized.equalsIgnoreCase(F("GEOIP"))) {
			switch (_geo_lookup_mode) {
				case EXT_GEOIP_LOOKUP_ONLY:
					query = F("EXT_GEOIP");
					break;
				case GEOIP_LOOKUP_WITH_EXT_FALLBACK:
					query = F("GEOIP");
					allow_ext_geoip_fallback = true;
					break;
				case GEOIP_LOOKUP_ONLY:
				default:
					query = F("GEOIP");
					break;
			}
		} else if (normalized.equalsIgnoreCase(F("EXT_GEOIP"))) {
			query = F("EXT_GEOIP");
		}

		return beginGenericAsyncRequest(ASYNC_SETLOCATION, query, this, allow_ext_geoip_fallback);
	}

	void ezt::pollAsync() {
		if (_ntp_async.active && _ntp_async.phase == NTP_PHASE_WAIT_RESPONSE) {
			if (_ntp_async.udp.parsePacket()) {
				_ntp_async.udp.read(_ntp_async.buffer, NTP_PACKET_SIZE);
				_ntp_async.udp.stop();
				_ntp_async.active = false;

				byte *buffer = _ntp_async.buffer;
				uint32_t highWord = ( buffer[16] << 8 | buffer[17] ) & 0x0000FFFF;
				uint32_t lowWord = ( buffer[18] << 8 | buffer[19] ) & 0x0000FFFF;
				uint32_t reftsSec = highWord << 16 | lowWord;
				highWord = ( buffer[32] << 8 | buffer[33] ) & 0x0000FFFF;
				lowWord = ( buffer[34] << 8 | buffer[35] ) & 0x0000FFFF;
				uint32_t rcvtsSec = highWord << 16 | lowWord;
				highWord = ( buffer[40] << 8 | buffer[41] ) & 0x0000FFFF;
				lowWord = ( buffer[42] << 8 | buffer[43] ) & 0x0000FFFF;
				uint32_t secsSince1900 = highWord << 16 | lowWord;
				highWord = ( buffer[44] << 8 | buffer[45] ) & 0x0000FFFF;
				lowWord = ( buffer[46] << 8 | buffer[47] ) & 0x0000FFFF;
				uint32_t fraction = highWord << 16 | lowWord;

				if ((buffer[1] < 1) || (buffer[1] > 15) || (reftsSec == 0) || (rcvtsSec == 0) || (rcvtsSec > secsSince1900)) {
					triggerError(INVALID_DATA);
					UTC.setEvent(updateNTP, nowUTC(false) + NTP_RETRY);
				} else {
					ezTime_t t;
					if (!ntpSecondsToUnixTime(secsSince1900, t)) {
						triggerError(INVALID_DATA);
						UTC.setEvent(updateNTP, nowUTC(false) + NTP_RETRY);
					} else {
						uint32_t done = millis();
						uint16_t ms = fraction / 4294967UL;
						unsigned long measured_at = done - ((done - _ntp_async.started) / 2) - ms;
						int64_t correction = ((int64_t)(t - _last_sync_time) * 1000LL) - (int64_t)(measured_at - _last_sync_millis);
						_last_sync_time = t;
						_last_sync_millis = measured_at;
						_last_read_ms = (millis() - measured_at) % 1000;
						info(F("Received time: "));
						info(UTC.dateTime(t, F("l, d-M-y H:i:s.v T")));
						if (_time_status != timeNotSet) {
							info(F(" (internal clock was "));
							if (!correction) {
								infoln(F("spot on)"));
							} else {
								info(int64ToString(correction < 0 ? -correction : correction));
								infoln(correction > 0 ? F(" ms fast)") : F(" ms slow)"));
							}
						} else {
							infoln("");
						}
						if (_ntp_interval) UTC.setEvent(updateNTP, t + _ntp_interval);
						_time_status = timeSet;
						_last_error = NO_ERROR;
					}
				}
			} else if (millis() - _ntp_async.started > NTP_TIMEOUT) {
				_ntp_async.udp.stop();
				_ntp_async.active = false;
				for (uint8_t next = _ntp_async.server_index + 1; next < 3; next++) {
					String next_server = _ntp_servers[next];
					next_server.trim();
					if (next_server.length()) {
						info(F("NTP query failed, trying fallback "));
						info(next + 1);
						info(F(": "));
						infoln(next_server);
						if (beginNtpAsyncRequest(next)) {
							return;
						}
					}
				}
				triggerError(TIMEOUT);
				if (nowUTC(false) > _last_sync_time + _ntp_interval + NTP_STALE_AFTER) {
					_time_status = timeNeedsSync;
				}
				UTC.setEvent(updateNTP, nowUTC(false) + NTP_RETRY);
			}
		}

		if (_server_async.status != ASYNC_PENDING) {
			return;
		}

		if (!_server_async.udp.parsePacket()) {
			if (millis() - _server_async.started > TIMEZONED_TIMEOUT) {
				triggerError(TIMEOUT);
				_server_async.status = ASYNC_ERROR;
				_server_async.udp.stop();
				clearServerAsyncBuffers();
			}
			return;
		}

		String packet;
		while (_server_async.udp.available()) {
			packet += (char)_server_async.udp.read();
		}
		packet.trim();

		if (_server_async.phase == SERVER_PHASE_WAIT_RESPONSE) {
			if (startsWithIgnoreCase(packet, F("ERROR "))) {
				_server_error = packet.substring(6);
				triggerError(classifyServerError(_server_error));
				if (_server_async.allow_ext_geoip_fallback && (_server_error == F("GEOIP Lookup Failed") || _server_error == F("GEOIP Internal IP"))) {
					info(F("retrying with EXT_GEOIP ... "));
					_server_async.allow_ext_geoip_fallback = false;
					_server_async.query = F("EXT_GEOIP");
					if (beginServerQueryNonBlocking(_server_async.udp, _server_async.query, _server_async.started)) {
						return;
					}
				}
				_server_async.status = ASYNC_ERROR;
				_server_async.udp.stop();
				return;
			}

			if (_server_async.type == ASYNC_INFO) {
				if (!startsWithIgnoreCase(packet, F("INFO OK "))) {
					triggerError(PROTOCOL_ERROR);
					_server_async.status = ASYNC_ERROR;
					_server_async.udp.stop();
					return;
				}
				_server_async.result = packet.substring(8);
				_server_async.status = ASYNC_SUCCESS;
				_server_async.udp.stop();
				return;
			}

			if (_server_async.type == ASYNC_SETLOCATION) {
				if (!startsWithIgnoreCase(packet, F("OK "))) {
					triggerError(PROTOCOL_ERROR);
					_server_async.status = ASYNC_ERROR;
					_server_async.udp.stop();
					return;
				}
				int separator = packet.indexOf(' ', 3);
				if (separator < 0) {
					triggerError(PROTOCOL_ERROR);
					_server_async.status = ASYNC_ERROR;
					_server_async.udp.stop();
					return;
				}
				String olson = packet.substring(3, separator);
				String posix = packet.substring(separator + 1);
				if (!_server_async.timezone || !_server_async.timezone->applyTimezoneData(olson, posix)) {
					_server_async.status = ASYNC_ERROR;
					_server_async.udp.stop();
					return;
				}
				_server_async.result = olson;
				_server_async.status = ASYNC_SUCCESS;
				_server_async.udp.stop();
				return;
			}

			_server_async.result = packet;
			_server_async.status = ASYNC_SUCCESS;
			_server_async.udp.stop();
			return;
		}

		if (_server_async.phase == SERVER_PHASE_WAIT_LIST_CHALLENGE) {
			if (startsWithIgnoreCase(packet, F("ERROR "))) {
				_server_error = packet.substring(6);
				triggerError(classifyServerError(_server_error));
				_server_async.status = ASYNC_ERROR;
				_server_async.udp.stop();
				return;
			}
			if (!startsWithIgnoreCase(packet, F("LIST CHALLENGE "))) {
				triggerError(CHALLENGE_FAILED);
				_server_async.status = ASYNC_ERROR;
				_server_async.udp.stop();
				return;
			}
			String token = packet.substring(15);
			token.trim();
			String challenged_query = _server_async.query + F("&token=") + token;
			if (!beginServerQueryNonBlocking(_server_async.udp, challenged_query, _server_async.started)) {
				_server_async.status = ASYNC_ERROR;
				return;
			}
			_server_async.phase = SERVER_PHASE_WAIT_LIST_CHUNKS;
			return;
		}

		if (_server_async.phase == SERVER_PHASE_WAIT_LIST_CHUNKS) {
			ezChunkPacket_t chunk_info;
			if (!parseChunkPacket(packet, chunk_info)) {
				triggerError(PROTOCOL_ERROR);
				_server_async.status = ASYNC_ERROR;
				_server_async.udp.stop();
				return;
			}
			if (chunk_info.has_crc && crc32String(chunk_info.chunk) != chunk_info.crc) {
				triggerError(CRC_ERROR);
				_server_async.status = ASYNC_ERROR;
				_server_async.udp.stop();
				return;
			}
			if (!validateChunkPacket(chunk_info, _server_async.request_id)) {
				triggerError(CHUNK_ERROR);
				_server_async.status = ASYNC_ERROR;
				_server_async.udp.stop();
				return;
			}

			if (_server_async.chunks == NULL) {
				_server_async.total_chunks = chunk_info.total;
				_server_async.chunks = new String[_server_async.total_chunks];
				_server_async.received = new bool[_server_async.total_chunks];
				if (_server_async.chunks == NULL || _server_async.received == NULL) {
					triggerError(CONNECT_FAILED);
					_server_async.status = ASYNC_ERROR;
					_server_async.udp.stop();
					clearServerAsyncBuffers();
					return;
				}
				for (uint16_t i = 0; i < _server_async.total_chunks; i++) {
					_server_async.received[i] = false;
				}
			} else if (chunk_info.total != _server_async.total_chunks) {
				triggerError(CHUNK_ERROR);
				_server_async.status = ASYNC_ERROR;
				_server_async.udp.stop();
				clearServerAsyncBuffers();
				return;
			}

			if (!_server_async.received[chunk_info.index - 1]) {
				_server_async.chunks[chunk_info.index - 1] = chunk_info.chunk;
				_server_async.received[chunk_info.index - 1] = true;
				_server_async.received_count++;
			}

			if (_server_async.received_count >= _server_async.total_chunks) {
				_server_async.result = "";
				for (uint16_t i = 0; i < _server_async.total_chunks; i++) {
					_server_async.result += _server_async.chunks[i];
				}
				_server_async.status = ASYNC_SUCCESS;
				_server_async.udp.stop();
				clearServerAsyncBuffers();
			}
		}
	}

	bool ezt::getPublicIP(String &ip) {
		if (!beginGetPublicIP()) {
			return false;
		}
		while (asyncStatus() == ASYNC_PENDING) {
			pollAsync();
			delay(1);
		}
		if (asyncStatus() != ASYNC_SUCCESS) {
			return false;
		}
		ip = asyncResult();
		ip.trim();
		if (!ip.length() || startsWithIgnoreCase(ip, F("ERROR "))) {
			if (startsWithIgnoreCase(ip, F("ERROR "))) {
				_server_error = ip.substring(6);
				triggerError(classifyServerError(_server_error));
			} else {
				triggerError(DATA_NOT_FOUND);
			}
			return false;
		}

		return true;
	}

	bool ezt::getInfo(const String infotype, String &value, const String target /* = "" */) {
		if (!beginInfo(infotype, target)) {
			return false;
		}
		while (asyncStatus() == ASYNC_PENDING) {
			pollAsync();
			delay(1);
		}
		if (asyncStatus() != ASYNC_SUCCESS) {
			return false;
		}
		value = asyncResult();
		if (trimCopy(infotype).equalsIgnoreCase(F("all")) && startsWithIgnoreCase(value, F("all"))) {
			value = value.substring(3);
			value.trim();
		}
		return true;
	}

	bool ezt::getInfoAll(String &info_data, const String target /* = "" */) {
		return getInfo(F("all"), info_data, target);
	}

	bool ezt::infoItem(const String &info_data, const String &key, String &value) {
		String key_lc = key;
		key_lc.trim();
		key_lc.toLowerCase();

		int start = 0;
		while (start < info_data.length()) {
			int end = info_data.indexOf(';', start);
			if (end < 0) end = info_data.length();
			String entry = info_data.substring(start, end);
			entry.trim();
			if (entry.length()) {
				int separator = entry.indexOf(':');
				if (separator > 0) {
					String current_key = entry.substring(0, separator);
					current_key.trim();
					current_key.toLowerCase();
					if (current_key == key_lc) {
						value = entry.substring(separator + 1);
						value.trim();
						return true;
					}
				}
			}
			start = end + 1;
			while (start < info_data.length() && (info_data[start] == '\n' || info_data[start] == '\r')) {
				start++;
			}
		}
		return false;
	}

	bool Timezone::setLocation(const String location /* = "GeoIP" */) {
		if (!beginSetLocation(location)) {
			return false;
		}
		while (ezt::asyncStatus() == ASYNC_PENDING) {
			ezt::pollAsync();
			delay(1);
		}
		return ezt::asyncStatus() == ASYNC_SUCCESS;
	}

	#ifdef EZTIME_SERVER_LIST_ENABLE

		bool ezt::beginListTimezones(const String list_name) {
			String normalized = trimCopy(list_name);
			normalized.toLowerCase();
			if (!normalized.length()) {
				triggerError(DATA_NOT_FOUND);
				return false;
			}
			String query = F("LIST ");
			query += normalized;
			return beginGenericAsyncRequest(ASYNC_LIST, query);
		}

		bool ezt::listTimezones(const String list_name, String &list_data) {
			if (!beginListTimezones(list_name)) {
				return false;
			}
			while (asyncStatus() == ASYNC_PENDING) {
				pollAsync();
				delay(1);
			}
			if (asyncStatus() != ASYNC_SUCCESS) {
				return false;
			}
			list_data = asyncResult();
			return true;
		}

		uint16_t ezt::listLength(const String &list_data) {
			uint16_t count = 0;
			for (uint16_t i = 0; i < list_data.length(); i++) {
				if (list_data[i] == ';') {
					count++;
				}
			}
			return count;
		}

		bool ezt::listItem(const String &list_data, const uint16_t item_index, String &name, uint16_t &child_count) {
			uint16_t current_index = 0;
			int start = 0;

			while (start < list_data.length()) {
				int end = list_data.indexOf(';', start);
				if (end < 0) {
					break;
				}

				String entry = list_data.substring(start, end);
				entry.trim();
				if (entry.length()) {
					if (current_index == item_index) {
						int separator = entry.lastIndexOf(':');
						if (separator < 0) {
							return false;
						}
						name = entry.substring(0, separator);
						child_count = entry.substring(separator + 1).toInt();
						return true;
					}
					current_index++;
				}

				start = end + 1;
				while (start < list_data.length() && (list_data[start] == '\n' || list_data[start] == '\r')) {
					start++;
				}
			}

			return false;
		}

	#endif
	
	
	String Timezone::getOlson() {
		return _olson;
	}

	String Timezone::getOlsen() {
		return _olson;
	}

	bool Timezone::applyTimezoneData(const String &olson, const String &posix, const bool write_cache /* = true */) {
		if (_locked_to_UTC) {
			triggerError(LOCKED_TO_UTC);
			return false;
		}

		_olson = olson;
		_posix = posix;
		infoln(F("success."));
		info(F("  Olson: ")); infoln(_olson);
		info(F("  Posix: ")); infoln(_posix);
		#if defined(EZTIME_CACHE_EEPROM) || defined(EZTIME_CACHE_NVS)
			if (write_cache) {
				String tzinfo = _olson + " " + _posix;
				this->writeCache(tzinfo);
			}
		#else
			(void)write_cache;
		#endif
		return true;
	}


	#if defined(EZTIME_CACHE_EEPROM) || defined(EZTIME_CACHE_NVS)
	
		#if defined(ESP32) || defined(ESP8266)
			#define eepromBegin()	EEPROM.begin(4096)
			#define eepromEnd()		EEPROM.end()
			#define eepromLength()	(4096)
		#else
			#define eepromBegin()	""
			#define eepromEnd()		""
			#define eepromLength()	EEPROM.length()
		#endif
		
		#ifdef EZTIME_CACHE_EEPROM
			bool Timezone::setCache(const int16_t address) {
				eepromBegin();
				if (address + EEPROM_CACHE_LEN > eepromLength()) { triggerError(CACHE_TOO_SMALL); return false; }
				_eeprom_address = address;
				eepromEnd();
				return setCache();
			}
		#endif
	
		#ifdef EZTIME_CACHE_NVS
			bool Timezone::setCache(const String name, const String key) {
				_nvs_name = name;
				_nvs_key = key;
				return setCache();
			}
		#endif

		bool Timezone::setCache() {
			String olson, posix;
			uint8_t months_since_jan_2018;
			if (readCache(olson, posix, months_since_jan_2018)) {
				applyTimezoneData(olson, posix, false);
				_cache_month = months_since_jan_2018;
				if ( (year() - 2018) * 12 + month(LAST_READ) - months_since_jan_2018 > MAX_CACHE_AGE_MONTHS) {
					infoln(F("Cache stale, getting fresh"));
					setLocation(olson);
				}
				return true;
			}
			return false;
		}
		
		void Timezone::clearCache(const bool delete_section /* = false */) {
		
			#ifdef EZTIME_CACHE_EEPROM
				eepromBegin();
				if (_eeprom_address < 0) { triggerError(NO_CACHE_SET); return; }
				for (int16_t n = _eeprom_address; n < _eeprom_address + EEPROM_CACHE_LEN; n++) EEPROM.write(n, 0);
				eepromEnd();
			#endif

			#ifdef EZTIME_CACHE_NVS
				if (_nvs_name == "" || _nvs_key == "") { triggerError(NO_CACHE_SET); return; }
				Preferences prefs;
				prefs.begin(_nvs_name.c_str(), false);
				if (delete_section) {
					prefs.clear();
				} else {
					prefs.remove(_nvs_key.c_str());
				}
				prefs.end();
			#endif
		}

		bool Timezone::writeCache(String &str) {
			uint8_t months_since_jan_2018 = 0;
			if (year() >= 2018) months_since_jan_2018 = (year(LAST_READ) - 2018) * 12 + month(LAST_READ) - 1;

			#ifdef EZTIME_CACHE_EEPROM
				if (_eeprom_address < 0) return false;

				info(F("Caching timezone data  "));
				if (str.length() > MAX_CACHE_PAYLOAD) { triggerError(CACHE_TOO_SMALL); return false; }
				
				uint16_t last_byte = _eeprom_address + EEPROM_CACHE_LEN - 1;	
				uint16_t addr = _eeprom_address;
				
				eepromBegin();
				
				// First byte is cache age, in months since 2018
				EEPROM.write(addr++, months_since_jan_2018);
				
				// Second byte is length of payload
				EEPROM.write(addr++, str.length());
				
				// Followed by payload, compressed. Every 4 bytes to three by encoding only 6 bits, ASCII all-caps
				str.toUpperCase();
				uint8_t store = 0;
				for (uint8_t n = 0; n < str.length(); n++) {
					unsigned char c = str.charAt(n) - 32;
					if ( c > 63) c = 0;
					switch (n % 4) {
						case 0:
							store = c << 2;					//all of 1st
							break;
						case 1:
							store |= c >> 4;				//high two of 2nd
							EEPROM.write(addr++, store);	 
							store = c << 4;					//low four of 2nd
							break;
						case 2:
							store |= c >> 2;				//high four of 3rd
							EEPROM.write(addr++, store);
							store = c << 6;					//low two of third
							break;
						case 3:
							store |= c;						//all of 4th
							EEPROM.write(addr++, store);
							store = 0;
					}
				}
				if (store) EEPROM.write(addr++, store);
				
				// Fill rest of cache (except last byte) with zeroes
				for (; addr < last_byte; addr++) EEPROM.write(addr, 0);

				// Add all bytes in cache % 256 and add 42, that is the checksum written to last byte.
				// The 42 is because then checksum of all zeroes then isn't zero.
				uint8_t checksum = 0;
				for (uint16_t n = _eeprom_address; n < last_byte; n++) checksum += EEPROM.read(n);
				checksum += 42;
				EEPROM.write(last_byte, checksum);
				eepromEnd();
				infoln();
				return true;
			#endif
			
			#ifdef EZTIME_CACHE_NVS
				if (_nvs_name == "" || _nvs_key == "") return false;
				infoln(F("Caching timezone data"));
				Preferences prefs;
				prefs.begin(_nvs_name.c_str(), false);
				String tmp = String(months_since_jan_2018) + " " + str;
				prefs.putString(_nvs_key.c_str(), tmp);
				prefs.end();
				return true;
			#endif
		}
	

		bool Timezone::readCache(String &olson, String &posix, uint8_t &months_since_jan_2018) {

			#ifdef EZTIME_CACHE_EEPROM
				if (_eeprom_address < 0) { triggerError(NO_CACHE_SET); return false; }
				eepromBegin();
				uint16_t last_byte = _eeprom_address + EEPROM_CACHE_LEN - 1;			
				
				for (uint16_t n = _eeprom_address; n <= last_byte; n++) {
					debug(n);
					debug(F(" "));
					debugln(EEPROM.read(n), HEX);
				}
				
				// return false if checksum incorrect
				uint8_t checksum = 0;
				for (uint16_t n = _eeprom_address; n < last_byte; n++) checksum += EEPROM.read(n);
				checksum += 42;				
				if (checksum != EEPROM.read(last_byte)) { eepromEnd(); return false; }
				debugln(F("Checksum OK"));
				
				// Return false if length impossible
				uint8_t len = EEPROM.read(_eeprom_address + 1);
				debug("Length: "); debugln(len);
				if (len > MAX_CACHE_PAYLOAD) { eepromEnd(); return false; }
				
				// OK, we're gonna decompress
				olson.reserve(len + 3);		// Everything goes in olson first. Decompression might overshoot 3 
				months_since_jan_2018 = EEPROM.read(_eeprom_address);
				
				for (uint8_t n = 0; n < EEPROM_CACHE_LEN - 3; n++) {
					uint16_t addr = n + _eeprom_address + 2;
					uint8_t c = EEPROM.read(addr);
					uint8_t p = EEPROM.read(addr - 1);	// previous byte
					switch (n % 3) {
						case 0:
							olson += (char)( ((c & 0b11111100) >> 2) + 32 );
							break;
						case 1:
							olson += (char)( ((p & 0b00000011) << 4) + ((c & 0b11110000) >> 4) + 32 );
							break;
						case 2:
							olson += (char)( ((p & 0b00001111) << 2) + ((c & 0b11000000) >> 6) + 32 );
							olson += (char)( (c & 0b00111111) + 32 );
					}
					if (olson.length() >= len) break;
				}
				
				uint8_t first_space = olson.indexOf(' ');
				posix = olson.substring(first_space + 1, len);
				olson = olson.substring(0, first_space);
				
				// Restore case of olson (best effort)
				String olson_lowercase = olson;
				olson_lowercase.toLowerCase();
				for (uint8_t n = 1; n < olson.length(); n++) {
					unsigned char p = olson.charAt(n - 1);	// previous character
					if (p != '_' && p != '/' && p != '-') {
						olson.setCharAt(n, olson_lowercase[n]);
					}
				}
				info(F("Cache read. Olson: ")); info(olson); info (F("  Posix: ")); infoln(posix);
				eepromEnd();
				return true;
			#endif						
			
			#ifdef EZTIME_CACHE_NVS
				if (_nvs_name == "" || _nvs_key == "") { triggerError(NO_CACHE_SET); return false; }
				
				Preferences prefs;
				prefs.begin(_nvs_name.c_str(), true);
				String read_string = prefs.getString(_nvs_key.c_str());
				read_string.trim();
				prefs.end();
				if (read_string == "") return false;
				
				uint8_t first_space = read_string.indexOf(' ');
				uint8_t second_space = read_string.indexOf(' ', first_space + 1);
				if (first_space && second_space) {
					months_since_jan_2018 = read_string.toInt();
					posix = read_string.substring(second_space + 1);
					olson = read_string.substring(first_space + 1, second_space);
					info(F("Cache read. Olson: ")); info(olson); info (F("  Posix: ")); infoln(posix);
					return true;
				}
				return false;
			#endif
		}
		
	#endif	// defined(EZTIME_CACHE_EEPROM) || defined(EZTIME_CACHE_NVS)


#endif // EZTIME_NETWORK_ENABLE


void Timezone::setDefault() {
	defaultTZ = this;
	debug(F("Default timezone set to ")); debug(_olson); debug(F("  "));debugln(_posix);
}

bool Timezone::isDST(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	String tzname;
	bool is_dst;
	int16_t offset;
	t = tzTime(t, local_or_utc, tzname, is_dst, offset);
	return is_dst;
}

String Timezone::getTimezoneName(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	String tzname;
	bool is_dst;
	int16_t offset;
	t = tzTime(t, local_or_utc, tzname, is_dst, offset);
	return tzname;
}

int16_t Timezone::getOffset(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	String tzname;
	bool is_dst;
	int16_t offset;
	t = tzTime(t, local_or_utc, tzname, is_dst, offset);
	return offset;
}

uint8_t Timezone::setEvent(void (*function)(), const uint8_t hr, const uint8_t min, const uint8_t sec, const uint8_t day, const uint8_t mnth, uint16_t yr) {
	ezTime_t t = ezt::makeTime(hr, min, sec, day, mnth, yr);
	return setEvent(function, t);
}

uint8_t Timezone::setEvent(void (*function)(), ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	for (uint8_t n = 0; n < MAX_EVENTS; n++) {
		if (!_events[n].function) {
			_events[n].function = function;
			_events[n].time = t;
			debug(F("Set event (#")); debug(n + 1); debug(F(") to trigger on: ")); debugln(UTC.dateTime(t));
			return n + 1;
		}
	}
	triggerError(TOO_MANY_EVENTS);
	return 0;
}

void Timezone::setTime(const ezTime_t t, const uint16_t ms /* = 0 */) {
	int16_t offset;
	offset = getOffset(t);
	_last_sync_time = t + offset * 60;
	_last_sync_millis = millis() - ms;
	_time_status = timeSet;
}

void Timezone::setTime(const uint8_t hr, const uint8_t min, const uint8_t sec, const uint8_t day, const uint8_t mnth, uint16_t yr) {
	tmElements_t tm;
	// year can be given as full four digit year or two digts (2010 or 10 for 2010);  
	// it is converted to years since 1970
	if( yr > 99) {
		yr = yr - 1970;
	} else {
		yr += 30; 
	}
	tm.Year = yr;
	tm.Month = mnth;
	tm.Day = day;
	tm.Hour = hr;
	tm.Minute = min;
	tm.Second = sec;
	setTime(ezt::makeTime(tm));
}

String Timezone::dateTime(const String format /* = DEFAULT_TIMEFORMAT */) {
	return dateTime(TIME_NOW, format);
}

String Timezone::dateTime(const ezTime_t t, const String format /* = DEFAULT_TIMEFORMAT */) {
	return dateTime(t, LOCAL_TIME, format);
}

String Timezone::dateTime(ezTime_t t, const ezLocalOrUTC_t local_or_utc, const String format /* = DEFAULT_TIMEFORMAT */) {

	String tzname;
	bool is_dst;
	int16_t offset;

	if (t == TIME_NOW || t == LAST_READ || local_or_utc == UTC_TIME) {
		// in these cases we actually want tzTime to translate the time for us
		// back in to this timezone's time as well as grab the timezone info
		// from the stored POSIX data
		t = tzTime(t, UTC_TIME, tzname, is_dst, offset);
	} else {
		// when receiving a local time we don't want to translate the timestamp
		// but rather use tzTime to just parse the info about the timezone from
		// the stored POSIX data
		tzTime(t, LOCAL_TIME, tzname, is_dst, offset);
	}

	String tmpstr;
	uint8_t tmpint8;
	String out = "";

	tmElements_t tm;
	ezt::breakTime(t, tm);

	int8_t hour12 = tm.Hour % 12;
	if (hour12 == 0) hour12 = 12;
	
	int32_t o;

	bool escape_char = false;
	
	for (uint8_t n = 0; n < format.length(); n++) {
	
		char c = format.charAt(n);
		
		if (escape_char) {
			out += String(c);
			escape_char = false;
		} else {
		
			switch (c) {
		
				case '\\':	// Escape character, ignore this one, and let next through as literal character
				case '~':	// Same but easier without all the double escaping
					escape_char = true;
					break;
				case 'd':	// Day of the month, 2 digits with leading zeros
					out += ezt::zeropad(tm.Day, 2);
					break;
				case 'D':	// A textual representation of a day, usually two or three letters
					out += ezt::dayShortStr(tm.Wday);
					break;
				case 'j':	// Day of the month without leading zeros
					out += String(tm.Day);
					break;
				case 'l':	// (lowercase L) A full textual representation of the day of the week
					out += ezt::dayStr(tm.Wday);
					break;
				case 'N':	// ISO-8601 numeric representation of the day of the week. ( 1 = Monday, 7 = Sunday )
					tmpint8 = tm.Wday - 1;
					if (tmpint8 == 0) tmpint8 = 7;
					out += String(tmpint8);
					break;
				case 'S':	// English ordinal suffix for the day of the month, 2 characters (st, nd, rd, th)
					switch (tm.Day) {
						case 1:
						case 21:
						case 31:
							out += F("st"); break;
						case 2:
						case 22:
							out += F("nd"); break;
						case 3:
						case 23:
							out += F("rd"); break;
						default:
							out += F("th"); break;
					}
					break;
				case 'w':	// Numeric representation of the day of the week ( 0 = Sunday )
					out += String(tm.Wday);
					break;
				case 'F':	// A full textual representation of a month, such as January or March
					out += ezt::monthStr(tm.Month);
					break;
				case 'm':	// Numeric representation of a month, with leading zeros
					out += ezt::zeropad(tm.Month, 2);
					break;
				case 'M':	// A short textual representation of a month, usually three letters
					out += ezt::monthShortStr(tm.Month);
					break;
				case 'n':	// Numeric representation of a month, without leading zeros
					out += String(tm.Month);
					break;
				case 't':	// Number of days in the given month
					out += String(monthDays[tm.Month - 1]);
					break;
				case 'Y':	// A full numeric representation of a year, 4 digits
					out += String(tm.Year + 1970);
					break;
				case 'y':	// A two digit representation of a year
					out += ezt::zeropad((tm.Year + 1970) % 100, 2);
					break;
				case 'a':	// am or pm
					out += (tm.Hour < 12) ? F("am") : F("pm");
					break;
				case 'A':	// AM or PM
					out += (tm.Hour < 12) ? F("AM") : F("PM");
					break;
				case 'g':	// 12-hour format of an hour without leading zeros
					out += String(hour12);
					break;
				case 'G':	// 24-hour format of an hour without leading zeros
					out += String(tm.Hour);
					break;
				case 'h':	// 12-hour format of an hour with leading zeros
					out += ezt::zeropad(hour12, 2);
					break;
				case 'H':	// 24-hour format of an hour with leading zeros
					out += ezt::zeropad(tm.Hour, 2);
					break;
				case 'i':	// Minutes with leading zeros
					out += ezt::zeropad(tm.Minute, 2);
					break;
				case 's':	// Seconds with leading zeros
					out += ezt::zeropad(tm.Second, 2);
					break;
				case 'T':	// abbreviation for timezone
					out += tzname;	
					break;
				case 'v':	// milliseconds as three digits
					out += ezt::zeropad(_last_read_ms, 3);				
					break;
				#ifdef EZTIME_NETWORK_ENABLE
					case 'e':	// Timezone identifier (Olson)
						out += getOlson();
						break;
				#endif
				case 'O':	// Difference to Greenwich time (GMT) in hours and minutes written together (+0200)
				case 'P':	// Difference to Greenwich time (GMT) in hours and minutes written with colon (+02:00)
					o = offset;
					out += (o < 0) ? "+" : "-";		// reversed from our offset
					if (o < 0) o = 0 - o;
					out += ezt::zeropad(o / 60, 2);
					out += (c == 'P') ? ":" : "";
					out += ezt::zeropad(o % 60, 2);
					break;	
				case 'Z':	//Timezone offset in seconds. West of UTC is negative, east of UTC is positive.
					out += String(0 - offset * 60);
					break;
				case 'z':
					out += String(dayOfYear(t)); // The day of the year (starting from 0)
					break;
				case 'W':
					out += ezt::zeropad(weekISO(t), 2); // ISO-8601 week number of year, weeks starting on Monday
					break;
				case 'X':
					out += String(yearISO(t)); // ISO-8601 year-week notation year, see https://en.wikipedia.org/wiki/ISO_week_date
					break;
				case 'B':
					out += militaryTZ(t);
					break;
				default:
					out += String(c);

			}
		}
	}
	
	return out;
}

String Timezone::militaryTZ(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	int16_t o = getOffset(t);
	if (o % 60) return "?"; // If it's not a whole hour from UTC, it's not a timezone with a military letter code
	o = o / 60;
	if (o > 0) return String((char)('M' + o));
	if (o < 0 && o >= -9) return String((char)('A' - o - 1));	// Minus a negative number == plus 1
	if (o < -9) return String((char)('A' - o));				// Crazy, they're skipping 'J'
	return "Z";
}


uint8_t Timezone::hour(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	return t / 3600 % 24;
}

uint8_t Timezone::minute(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	return t / 60 % 60;
}

uint8_t Timezone::second(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	return t % 60;
}

uint16_t Timezone::ms(ezTime_t t /*= TIME_NOW */) {
	// Note that here passing anything but TIME_NOW or LAST_READ is pointless
	if (t == TIME_NOW) { nowUTC(); return _last_read_ms; }
	if (t == LAST_READ) return _last_read_ms;
	return 0;
}

uint8_t Timezone::day(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	tmElements_t tm;
	ezt::breakTime(t, tm);
	return tm.Day;
}

uint8_t Timezone::weekday(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	tmElements_t tm;
	ezt::breakTime(t, tm);
	return tm.Wday;
}

uint8_t Timezone::month(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	tmElements_t tm;
	ezt::breakTime(t, tm);
	return tm.Month;
}

uint16_t Timezone::year(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	tmElements_t tm;
	ezt::breakTime(t, tm);
	return tm.Year + 1970;
}

uint16_t Timezone::dayOfYear(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	ezTime_t jan_1st = ezt::makeTime(0, 0, 0, 1, 1, year(t));
	return (t - jan_1st) / SECS_PER_DAY;
}

uint8_t Timezone::hourFormat12(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	uint8_t h = t / 3600 % 12;
	if (h) return h;
	return 12;
}

bool Timezone::isAM(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	return (t / 3600 % 24 < 12);
}

bool Timezone::isPM(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	return (t / 3600 % 24 >= 12);
}


// Now this is where this gets a little obscure. The ISO year can be different from the
// actual (Gregorian) year. That is: you can be in january and still be in week 53 of past
// year, _and_ you can be in december and be in week one of the next. The ISO 8601 
// definition for week 01 is the week with the Gregorian year's first Thursday in it.  
// See https://en.wikipedia.org/wiki/ISO_week_date
//
#define startISOyear(year...) ezt::makeOrdinalTime(0, 0, 0, FIRST, THURSDAY, JANUARY, year) - ((ezTime_t)3 * SECS_PER_DAY);
uint8_t Timezone::weekISO(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	int16_t yr = year(t);
	ezTime_t this_year = startISOyear(yr);
	ezTime_t prev_year = startISOyear(yr - 1);
	ezTime_t next_year = startISOyear(yr + 1);
	if (t < this_year) this_year = prev_year;
	if (t >= next_year) this_year = next_year;
	return (t - this_year) / ( SECS_PER_DAY * 7UL) + 1;
}

uint16_t Timezone::yearISO(ezTime_t t /*= TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) {
	t = tzTime(t, local_or_utc);
	int16_t yr = year(t);
	ezTime_t this_year = startISOyear(yr);
	ezTime_t next_year = startISOyear(yr + 1);
	if (t < this_year) return yr - 1;
	if (t >= next_year) return yr + 1;
	return yr;
}


Timezone UTC;
Timezone *defaultTZ = &UTC;

namespace ezt {
	// All bounce-throughs to defaultTZ
	String dateTime(const String format /* = DEFAULT_TIMEFORMAT */) { return (defaultTZ->dateTime(format)); }
	String dateTime(ezTime_t t, const String format /* = DEFAULT_TIMEFORMAT */) { return (defaultTZ->dateTime(t, format)); }
	String dateTime(ezTime_t t, const ezLocalOrUTC_t local_or_utc, const String format /* = DEFAULT_TIMEFORMAT */) { return (defaultTZ->dateTime(t, local_or_utc, format)); }
	uint8_t day(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->day(t, local_or_utc)); } 
	uint16_t dayOfYear(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->dayOfYear(t, local_or_utc)); }
	int16_t getOffset(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->getOffset(t, local_or_utc)); }
	String getTimezoneName(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->getTimezoneName(t, local_or_utc)); }
	uint8_t hour(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->hour(t, local_or_utc)); }
	uint8_t hourFormat12(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->hourFormat12(t, local_or_utc)); }
	bool isAM(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->isAM(t, local_or_utc)); }
	bool isDST(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->isDST(t, local_or_utc)); }
	bool isPM(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->isPM(t, local_or_utc)); }
	String militaryTZ(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->militaryTZ(t, local_or_utc)); }
	uint8_t minute(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->minute(t, local_or_utc)); }
	uint8_t month(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->month(t, local_or_utc)); } 
	uint16_t ms(ezTime_t t /* = TIME_NOW */) { return (defaultTZ->ms(t)); }
	ezTime_t now() { return  (defaultTZ->now()); }
	uint8_t second(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->second(t, local_or_utc)); } 
	uint8_t setEvent(void (*function)(), const uint8_t hr, const uint8_t min, const uint8_t sec, const uint8_t day, const uint8_t mnth, uint16_t yr) { return (defaultTZ->setEvent(function,hr, min, sec, day, mnth, yr)); }
	uint8_t setEvent(void (*function)(), ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->setEvent(function, t, local_or_utc)); }
	void setTime(const uint8_t hr, const uint8_t min, const uint8_t sec, const uint8_t day, const uint8_t month, const uint16_t yr) { defaultTZ->setTime(hr, min, sec, day, month, yr); }
	void setTime(ezTime_t t) { defaultTZ->setTime(t); }
	uint8_t weekISO(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->weekISO(t, local_or_utc)); }
	uint8_t weekday(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->weekday(t, local_or_utc)); }
	uint16_t year(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->year(t, local_or_utc)); } 
	uint16_t yearISO(ezTime_t t /* = TIME_NOW */, const ezLocalOrUTC_t local_or_utc /* = LOCAL_TIME */) { return (defaultTZ->yearISO(t, local_or_utc)); }
}

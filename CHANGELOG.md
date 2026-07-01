# Changelog

## 1.1.0 / timezoned 3.0.4 - 2026-07-01

- added client-side `INFO` helpers with both synchronous and non-blocking request paths
- added local DST introspection with `hasDST()` and `nextDSTChange(...)`
- moved background NTP syncing onto the event-driven non-blocking path
- added finer client error classes for rate limits, protocol errors, challenge failures, chunk failures, CRC failures, and async-busy states
- added `EZTIME_CACHE_DISABLE` so cache code can be compiled out completely
- expanded the full self-test with async, INFO, DST transition, staged-summary, and retry coverage
- clarified migration and configuration documentation, including the new async and INFO APIs
- corrected `LIST` hierarchy documentation and examples to describe Olson path building properly
- improved example diagnostics so WiFi and sync failures are visible instead of failing silently
- changed server log timestamps to explicit local time with timezone name and offset instead of a misleading hard-coded `Z`

## 1.0.0 / timezoned 3.0.3 - 2026-07-01

- renamed the internal library sources to `ezTime2.h` and `ezTime2.cpp`
- added `ezTime2Config.h` support so compile-time options can be set per sketch without editing the library
- moved documented compile-time options to the new config-based workflow, including network, cache, list, debug, backend, and namespace toggles
- added configurable NTP fallback defaults with `NTP_SERVER`, `NTP_SERVER_2`, and `NTP_SERVER_3`
- added runtime `setServers(...)` support for primary and fallback NTP server chains
- expanded the full self-test sketch to cover the new NTP fallback API
- fixed the `LIST` advertised child count mismatch in the generated server data
- clarified the README around migration, configuration, and no-network builds

## 3.0.2 - 2026-06-28

- fixed `EXT_GEOIP` parsing for HTTP services that respond with `Transfer-Encoding: chunked`
- improved `EXT_GEOIP` error logging with upstream status and JSON/body diagnostics

## 3.0.1 - 2026-06-28

- fixed `LIST` challenge follow-up requests being counted by the rate limiter
- reduced default `LIST` chunk size from `768` to `512` bytes
- clarified `querytest.py` chunk output so single-chunk frames are not reported as multi-chunk responses

## 3.0.0 - 2026-06-28

- rebuilt the timezone server with public-facing hardening in mind
- added global and command-specific rate limits with environment-based configuration
- added stateless `LIST` challenge/response protection, request IDs, CRC validation, and consistent chunk framing
- added bounded caching and timeout controls for `GEOIP` and `EXT_GEOIP`
- added HTTP response size limits for external GeoIP lookups
- switched tzdata download to HTTPS with optional signature verification
- added a GHCR publish workflow for the `timezoned` image
- switched the container runtime to a non-root user
- aligned `INFO` timezone resolution with normal lookup behavior

# Changelog

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

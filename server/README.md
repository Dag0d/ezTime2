# Timezoned Server

>If you do not plan to run your own timezone information server, you do not need anything in this directory...

## Overview

The Timezoned Server is a high-performance UDP-based service that provides comprehensive timezone information, geographic lookups, and time calculations. It supports multiple query types and provides detailed timezone data including DST calculations, city/country information, and POSIX timezone strings.

## Server Configuration

### Environment Variables

### UDP_PORT
- **Default:** `2342`
- **Description:** UDP port the server listens on
- **Example:** `UDP_PORT=3000`

### RATE_LIMITING_ENABLED
- **Default:** `true`
- **Description:** Enable/disable rate limiting (3 seconds between requests)
- **Values:** `true` or `false`
- **Example:** `RATE_LIMITING_ENABLED=false` (for load testing)

### GEOIP_API_HOST
- **Default:** `geoip-api`
- **Description:** Hostname/IP of external GeoIP API service
- **Example:** `GEOIP_API_HOST=geoip.example.com`

### GEOIP_API_PORT
- **Default:** `8080`
- **Description:** Port of external GeoIP API service
- **Example:** `GEOIP_API_PORT=8080`

### TZ
- **Default:** `UTC`
- **Description:** Server timezone for logging
- **Example:** `TZ=Europe/Berlin`

### Rate Limiting
- Standard queries: 1 request per 3 seconds per IP
- LIST queries: 1 request per 1 second per IP
- Health checks: No rate limiting
- Can be disabled with `RATE_LIMITING_ENABLED=false`

## Query Types & Responses

### 1. Direct Timezone Queries

#### Timezone Name Query
```
Query: Europe/Berlin
Response: OK Europe/Berlin CET-1CEST,M3.5.0,M10.5.0/3
```

#### Country Code Query
```
Query: DE
Response: OK Europe/Berlin CET-1CEST,M3.5.0,M10.5.0/3

Query: US
Response: ERROR Country Spans Multiple Timezones
```

### 2. IP-based Queries

#### GEOIP - Local GeoIP Lookup
```
Query: GEOIP 8.8.8.8
Response: OK America/Los_Angeles PST8PDT,M3.2.0,M11.1.0

Query: GEOIP
Response: OK Europe/Berlin CET-1CEST,M3.5.0,M10.5.0/3
(Uses client IP)
```

#### EXT_GEOIP - External GeoIP Service
```
Query: EXT_GEOIP 8.8.8.8
Response: OK America/Los_Angeles PST8PDT,M3.2.0,M11.1.0

Query: EXT_GEOIP
Response: OK Europe/Berlin CET-1CEST,M3.5.0,M10.5.0/3
(Uses client IP)
```

### 3. Utility Queries

#### GETIP - Get Client IP
```
Query: GETIP
Response: 192.168.1.100
```

### 4. LIST Queries - Timezone Lists

#### Available Lists
```
Query: LIST regions
Response: (Chunked) All available regions

Query: LIST europe
Response: (Chunked) All European timezones

Query: LIST america
Response: (Chunked) All American timezones

Query: LIST asia
Response: (Chunked) All Asian timezones

Query: LIST africa
Response: (Chunked) All African timezones

Query: LIST oceania
Response: (Chunked) All Oceania timezones

Query: LIST antarctica
Response: (Chunked) All Antarctica timezones
```

### 5. INFO Queries - Detailed Timezone Information

#### Syntax
```
INFO <infotype> [<ip|timezone>]
```

#### Available Info Types
- `olson` - Olson timezone name
- `posix` - POSIX timezone string
- `utcoffset` - Current UTC offset (e.g., +02:00)
- `hasdst` - Has daylight saving time (true/false)
- `indst` - Currently in DST (true/false)
- `city` - City name
- `countrycode` - Two-letter country code
- `date` - Current date in timezone (YYYY-MM-DD)
- `day` - Current day of week (lowercase)
- `currenttime` - Current time in timezone (HH:MM:SS)
- `epoch` - Current Unix timestamp
- `all` - All information combined

#### INFO Examples

##### Single Information
```
Query: INFO utcoffset Europe/Berlin
Response: INFO OK +02:00

Query: INFO city 8.8.8.8
Response: INFO OK Mountain View

Query: INFO hasdst Asia/Dubai
Response: INFO OK false

Query: INFO epoch
Response: INFO OK 1753018957
(Uses client IP)
```

##### Complete Information
```
Query: INFO all Europe/Berlin
Response: INFO OK all
olson:Europe/Berlin;
posix:CET-1CEST,M3.5.0,M10.5.0/3;
utcoffset:+02:00;
hasdst:true;
indst:true;
date:2025-07-21;
day:monday;
currenttime:13:45:30;
epoch:1753018957;
city:Berlin;
countrycode:DE;
```

### 6. Health Check (Internal)

#### Basic Health Check
```
Query: HEALTHCHECK
Response: OK CORE_FUNCTIONS_WORKING
(Localhost only, silent)
```

#### Full Health Check
```
Query: HEALTHCHECK FULL
Response: OK FULL ALL_TESTS_PASSED
(Localhost only, silent)
```

## Error Responses

### Common Errors
```
ERROR Timezone Not Found
ERROR Country Not Found
ERROR Country Spans Multiple Timezones
ERROR GEOIP Lookup Failed
ERROR GEOIP Internal IP
ERROR EXT_GEOIP Failed
ERROR EXT_GEOIP Internal IP
ERROR INFO Invalid infotype: <type>
ERROR LIST Missing list name
ERROR LST File Not Found
ERROR HEALTHCHECK Access denied
```

## Response Formats

### Standard Responses
- Success: `OK <timezone> <posix>`
- Error: `ERROR <description>`

### INFO Responses
- Single: `INFO OK <value>`
- Multiple: `INFO OK all\n<key>:<value>;\n...`

### LIST Responses
- Chunked format with metadata
- Format: `<total>:<current>|<data>`

## Performance Characteristics

### Tested Performance
- **Load tested:** 2,260 requests/second
- **Success rate:** 100% at high load
- **Response time:** <10ms for most queries
- **Memory usage:** ~25MB

### Optimization Features
- O(1) country code lookups via indexing
- O(1) exact timezone name lookups
- Memory cleanup for connection tracking
- Efficient POSIX timezone parsing
- Cached DST calculations

## Docker Usage

### Building
```bash
VERSION=$(curl -s ftp://ftp.iana.org/tz/data/version)
docker build -t timezoned-server --build-arg VERSION=${VERSION} .
```

### Running
```bash
docker run -d \
  --name timezoned-server \
  -p 2342:2342/udp \
  -e RATE_LIMITING_ENABLED=true \
  -e GEOIP_API_HOST=geoip-api \
  -e GEOIP_API_PORT=8080 \
  --restart=unless-stopped \
  timezoned-server
```

### Health Monitoring
- Automatic health checks every 30 seconds
- Container restart on health check failure with autoheal (https://github.com/willfarrell/docker-autoheal)
- Health status visible in `docker ps`

## Client Examples

### Python Client
```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(b"INFO all Europe/Berlin", ("server-ip", 2342))
response, addr = sock.recvfrom(1472)
print(response.decode())
```

### Bash Client
```bash
echo "INFO utcoffset Europe/Berlin" | nc -u server-ip 2342
```

### Load Testing
```bash
python3 querytest.py
# Select option 2 for load testing
```

## Security Features

### Access Control
- Health checks restricted to localhost
- IP validation for all external calls
- Shell injection prevention
- Rate limiting protection

### Input Validation
- Strict IP format validation
- Command parameter validation
- Safe shell command execution
- Error boundary protection

## Logging Format

### Standard Log Format
```
[Day, DD Mon YYYY HH:MM:SS]Z -- IP:PORT -- STATUS FUNCTION: details
```

### Examples
```
Mon, 21 Jul 2025 13:45:30Z -- 192.168.1.100:54321 -- OK INFO: utcoffset [Europe/Berlin] -> +02:00
Mon, 21 Jul 2025 13:45:31Z -- 192.168.1.100:54321 -- OK LIST: regions.lst
Mon, 21 Jul 2025 13:45:32Z -- 192.168.1.100:54321 -- ERR INFO: ERROR Invalid infotype: invalid
```

## Troubleshooting

### Common Issues
1. **No response:** Check UDP port 2342 accessibility
2. **Rate limiting:** Wait or disable with environment variable
3. **GeoIP failures:** Check external API configuration

### Debug Commands
```bash
# Test basic connectivity
echo "GETIP" | nc -u server-ip 2342

# Test timezone lookup
echo "Europe/Berlin" | nc -u server-ip 2342

# Test INFO function
echo "INFO utcoffset" | nc -u server-ip 2342

# Check Docker health
docker inspect container-name | grep -A 10 Health
```

## API Compatibility

### Backward Compatibility to the original ezTime by Rop Gonggrijp (https://github.com/ropg/ezTime)
- All existing queries continue to work
- No breaking changes to existing functionality

### Version Information
- Server supports both old and new command formats
- Graceful handling of unknown commands
- Consistent error messaging across all functions
import socket
import urllib.request
import uuid
import zlib

# Server configuration
server_addr = "timezoned.circuitflow.eu"  # Replace with your server's address
server_port = 2342                       # Replace with your server's UDP port
buffer_size = 1472                       # Should match the server's max packet size
max_retries = 3                          # Number of retries for missing chunks

# Configuration for external lookup features
externalGeoLookup = False   # Enable (True) or disable (False) external geo lookup
externalIPLookup = True    # Enable (True) or disable (False) external IP lookup fallback

def attach_request_id(query):
    """Attach a request ID to the outgoing query."""
    request_id = uuid.uuid4().hex[:12]
    if "#" in query:
        return f"{query}&rid={request_id}", request_id
    return f"{query}#rid={request_id}", request_id

def attach_token(query, token):
    """Attach a challenge token to the outgoing query."""
    if "#" in query:
        return f"{query}&token={token}"
    return f"{query}#token={token}"

def send_query(sock, query):
    """Send a query to the server."""
    sock.sendto(query.encode(), (server_addr, server_port))
    print(f"Query sent: {query}")

def parse_chunk_packet(response):
    """Parse old and new chunk packet formats."""
    if not (response.startswith("<") and response.endswith(">") and "|" in response):
        return None

    packet = response[1:-1]
    metadata_str, chunk = packet.split("|", 1)

    if metadata_str.startswith("RID="):
        metadata = {}
        for entry in metadata_str.split(";"):
            if "=" not in entry:
                continue
            key, value = entry.split("=", 1)
            metadata[key] = value

        try:
            total_chunks = int(metadata["TOT"])
            current_chunk = int(metadata["IDX"])
            chunk_len = int(metadata["LEN"])
        except (KeyError, ValueError):
            return None

        return {
            "rid": metadata.get("RID"),
            "total": total_chunks,
            "index": current_chunk,
            "length": chunk_len,
            "crc": metadata.get("CRC", "").lower(),
            "chunk": chunk,
        }

    total_str, current_str = metadata_str.split(":")
    return {
        "rid": None,
        "total": int(total_str),
        "index": int(current_str),
        "length": len(chunk),
        "crc": None,
        "chunk": chunk,
    }

def receive_response(sock, request_id=None):
    """Receive the full response from the server."""
    try:
        data, addr = sock.recvfrom(buffer_size)
        response = data.decode()

        chunk_info = parse_chunk_packet(response)
        if chunk_info is not None:
            if request_id and chunk_info["rid"] and chunk_info["rid"] != request_id:
                print(f"Ignoring chunk for different request ID: {chunk_info['rid']}")
                return None
            if chunk_info["total"] > 1:
                print(f"\nResponse is split into {chunk_info['total']} chunks.")
            else:
                print("\nResponse received in a single chunk frame.")
            return reassemble_chunks(sock, chunk_info, request_id)
        else:
            return response
    except socket.timeout:
        print("Timeout reached while waiting for response.")
        return None

def reassemble_chunks(sock, first_chunk_info, request_id=None):
    """
    Reassemble a multi-chunk response.
    Supports both <total:current|chunk_data> and
    <RID=id;TOT=n;IDX=i;LEN=len;CRC=crc32|chunk_data>.
    """
    chunks = {}
    total_chunks = first_chunk_info["total"]
    expected_request_id = first_chunk_info["rid"] or request_id

    if first_chunk_info["length"] != len(first_chunk_info["chunk"]):
        print("Invalid first chunk length.")
        return None

    if first_chunk_info["crc"] is not None:
        first_crc = zlib.crc32(first_chunk_info["chunk"].encode()) & 0xFFFFFFFF
        if f"{first_crc:08x}" != first_chunk_info["crc"]:
            print("CRC mismatch in first chunk.")
            return None

    if total_chunks > 1:
        print(f"Expecting {total_chunks} chunks...")
    chunks[first_chunk_info["index"]] = first_chunk_info["chunk"]
    if total_chunks > 1:
        print(f"Received chunk {first_chunk_info['index']}/{total_chunks}...")

    while len(chunks) < total_chunks:
        try:
            data, addr = sock.recvfrom(buffer_size)
            packet = data.decode().strip()
            chunk_info = parse_chunk_packet(packet)
            if chunk_info is None:
                print("Invalid packet format received.")
                continue

            if expected_request_id and chunk_info["rid"] and chunk_info["rid"] != expected_request_id:
                print(f"Ignoring chunk for different request ID: {chunk_info['rid']}")
                continue

            if chunk_info["total"] != total_chunks:
                print("Inconsistent total chunks received.")
                return None

            if chunk_info["length"] != len(chunk_info["chunk"]):
                print(f"Invalid length for chunk {chunk_info['index']}.")
                return None

            if chunk_info["crc"] is not None:
                actual_crc = zlib.crc32(chunk_info["chunk"].encode()) & 0xFFFFFFFF
                if f"{actual_crc:08x}" != chunk_info["crc"]:
                    print(f"CRC mismatch in chunk {chunk_info['index']}.")
                    return None

            if chunk_info["index"] not in chunks:
                chunks[chunk_info["index"]] = chunk_info["chunk"]
                if total_chunks > 1:
                    print(f"Received chunk {chunk_info['index']}/{total_chunks}...")
        except socket.timeout:
            print("Timeout reached while receiving chunks.")
            break

    missing_chunks = set(range(1, total_chunks + 1)) - set(chunks.keys())
    if missing_chunks:
        print(f"Missing chunks: {sorted(missing_chunks)}")
        return None

    if total_chunks > 1:
        print("All chunks received. Reassembling response...")
    full_response = "".join(chunks[i] for i in range(1, total_chunks + 1))
    return full_response

def get_external_ip():
    """Retrieve external IP address using a public API via urllib."""
    try:
        with urllib.request.urlopen("https://api.ipify.org") as response:
            external_ip = response.read().decode('utf-8').strip()
            return external_ip
    except Exception as e:
        print(f"Failed to retrieve external IP: {e}")
        return None

def execute_query(query, sock):
    """Send query with retries and return response."""
    base_query, request_id = attach_request_id(query)
    query_with_id = base_query
    retry_count = 0
    while retry_count < max_retries:
        send_query(sock, query_with_id)
        print("Waiting for server response...")
        response = receive_response(sock, request_id=request_id)
        if response:
            if response.startswith("LIST CHALLENGE "):
                token = response[len("LIST CHALLENGE "):].strip()
                print(f"Received LIST challenge token: {token}")
                query_with_id = attach_token(base_query, token)
                continue
            return response
        else:
            retry_count += 1
            print(f"Retrying... ({retry_count}/{max_retries})")
    return None

def main():
    print("UDP Client for Server Testing")
    print("Type your requests below (e.g., Asia.lst, Europe/Berlin, GB, GEOIP, GETIP). Type 'exit' to quit.")

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2)  # Set timeout for receiving data

    try:
        while True:
            query = input("\nEnter your query: ").strip()
            if query.lower() == "exit":
                print("Exiting the client. Goodbye!")
                break

            # If external geo lookup is enabled and the query starts with "GEOIP", change it to "EXT_GEOIP"
            tokens = query.split(maxsplit=1)
            if externalGeoLookup and tokens[0] == "GEOIP":
                new_query = "EXT_GEOIP" + (" " + tokens[1] if len(tokens) > 1 else "")
                print("External geo lookup is enabled, changing query from GEOIP to EXT_GEOIP.")
                query = new_query

            # Execute the query
            response = execute_query(query, sock)

            if response is None:
                print("Failed to retrieve complete response after multiple retries.")
                continue

            # Handle internal IP error responses
            if response in ("ERROR GEOIP Internal IP", "ERROR EXT_GEOIP Internal IP"):
                if externalIPLookup:
                    print("Received internal IP error from API.")
                    ext_ip = get_external_ip()
                    if ext_ip:
                        # Build new query based on the error received.
                        if response == "ERROR GEOIP Internal IP":
                            new_query = f"GEOIP {ext_ip}"
                        else:
                            new_query = f"EXT_GEOIP {ext_ip}"
                        print(f"Using external IP service. External IP: {ext_ip}")
                        print(f"Resending query as: {new_query}")
                        new_response = execute_query(new_query, sock)
                        if new_response:
                            print("\nServer Response (after external IP lookup):")
                            print(new_response)
                        else:
                            print("Failed to retrieve response after external IP lookup.")
                    else:
                        print("Could not retrieve external IP. Original response:")
                        print(response)
                else:
                    print("Server Response:")
                    print(response)
            else:
                print("\nServer Response:")
                print(response)
    finally:
        sock.close()

def loadtest():
    """Load test function - sends many concurrent requests to test server performance."""
    import time
    import threading
    import random
    
    print("LOAD TEST MODE")
    print("This will send many requests rapidly to test server performance")
    print("Press Ctrl+C to stop the test\n")
    
    # Test queries to cycle through
    test_queries = [
        "Europe/Berlin",
        "America/New_York", 
        "Asia/Tokyo",
        "US",
        "DE",
        "GB",
        "GETIP",
        "INFO utcoffset 8.8.8.8",
        "INFO city 8.8.8.8",
        "INFO all 8.8.8.8",
        "Australia/Sydney",
        "America/Los_Angeles",
        "Asia/Dubai",
        "Europe/London",
        "FR",
        "JP",
        "INFO epoch 8.8.8.8",
        "INFO hasdst 8.8.8.8",
        "Asia/Shanghai",
        "Europe/Paris",
        "CA",
        "AU",
        "INFO date 8.8.8.8",
        "INFO day 8.8.8.8",
        "INFO currenttime 8.8.8.8",
        "America/Chicago",
        "Europe/Rome",
        "LIST regions",
        "LIST america"
    ]
    
    # Statistics
    stats = {
        'sent': 0,
        'received': 0,
        'errors': 0,
        'timeouts': 0,
        'start_time': time.time(),
        'running': True
    }
    
    def worker_thread(thread_id, requests_per_second):
        """Worker thread that sends requests continuously."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(0.5)  # Short timeout for load testing
        
        delay = 1.0 / requests_per_second
        
        try:
            while stats['running']:
                query = random.choice(test_queries)
                
                try:
                    # Send query
                    base_query, request_id = attach_request_id(query)
                    sock.sendto(base_query.encode(), (server_addr, server_port))
                    stats['sent'] += 1
                    
                    # Try to receive response
                    try:
                        response = receive_response(sock, request_id=request_id)
                        if response and response.startswith("LIST CHALLENGE "):
                            token = response[len("LIST CHALLENGE "):].strip()
                            challenged_query = attach_token(base_query, token)
                            sock.sendto(challenged_query.encode(), (server_addr, server_port))
                            stats['sent'] += 1
                            response = receive_response(sock, request_id=request_id)

                        if response is None:
                            stats['timeouts'] += 1
                            time.sleep(delay)
                            continue
                        stats['received'] += 1
                        
                        # Print some responses for monitoring
                        if stats['received'] % 200 == 0:
                            print(f"[T{thread_id:02d}] {query} -> {response[:40]}...")
                            
                    except socket.timeout:
                        stats['timeouts'] += 1
                        
                except Exception as e:
                    stats['errors'] += 1
                    if stats['errors'] % 50 == 0:
                        print(f"[T{thread_id:02d}] Error: {e}")
                
                time.sleep(delay)
                
        except KeyboardInterrupt:
            pass
        finally:
            sock.close()
    
    def stats_thread():
        """Thread that prints statistics every few seconds."""
        try:
            while stats['running']:
                time.sleep(3)
                elapsed = time.time() - stats['start_time']
                rps = stats['sent'] / elapsed if elapsed > 0 else 0
                success_rate = (stats['received'] / stats['sent'] * 100) if stats['sent'] > 0 else 0
                
                print(f"\n=== STATS (after {elapsed:.1f}s) ===")
                print(f"Sent:     {stats['sent']:6d} ({rps:6.1f} req/s)")
                print(f"Received: {stats['received']:6d} ({success_rate:5.1f}% success)")
                print(f"Timeouts: {stats['timeouts']:6d}")
                print(f"Errors:   {stats['errors']:6d}")
                print("=" * 35)
                
        except KeyboardInterrupt:
            pass
    
    # Get test parameters
    try:
        print(f"Target server: {server_addr}:{server_port}")
        
        num_threads = input("Number of threads (default 5): ").strip()
        num_threads = int(num_threads) if num_threads else 5
        
        rps_per_thread = input("Requests/sec per thread (default 20): ").strip()
        rps_per_thread = int(rps_per_thread) if rps_per_thread else 20
        
        duration = input("Test duration in seconds (default: until Ctrl+C): ").strip()
        duration = int(duration) if duration else None
        
    except ValueError:
        print("Invalid input, using defaults")
        num_threads = 5
        rps_per_thread = 20
        duration = None
    except KeyboardInterrupt:
        print("\nLoad test aborted")
        return
    
    # Validate parameters
    if num_threads < 1 or num_threads > 50:
        print("Number of threads must be between 1 and 50")
        return
    
    if rps_per_thread < 1 or rps_per_thread > 100:
        print("RPS per thread must be between 1 and 100")
        return
    
    total_rps = num_threads * rps_per_thread
    if total_rps > 1000:
        confirm = input(f"WARNING: High load: {total_rps} RPS total. Continue? (y/N): ")
        if confirm.lower() != 'y':
            print("Load test aborted")
            return
    
    print(f"\nStarting load test:")
    print(f"   Threads: {num_threads}")
    print(f"   RPS/Thread: {rps_per_thread}")
    print(f"   Total RPS: {total_rps}")
    if duration:
        print(f"   Duration: {duration} seconds")
    else:
        print("   Duration: Until Ctrl+C")
    print(f"   Test queries: {len(test_queries)} different types")
    print("\nPress Ctrl+C to stop...\n")
    
    # Start stats thread
    stats_t = threading.Thread(target=stats_thread, daemon=True)
    stats_t.start()
    
    # Start worker threads
    threads = []
    for i in range(num_threads):
        t = threading.Thread(target=worker_thread, args=(i+1, rps_per_thread), daemon=True)
        t.start()
        threads.append(t)
    
    try:
        if duration:
            # Run for specified duration
            time.sleep(duration)
            stats['running'] = False
        else:
            # Run until keyboard interrupt
            while True:
                time.sleep(1)
    except KeyboardInterrupt:
        print("\n\nStopping load test...")
        stats['running'] = False
    
    # Wait a bit for threads to finish
    time.sleep(1)
    
    # Final stats
    elapsed = time.time() - stats['start_time']
    rps = stats['sent'] / elapsed if elapsed > 0 else 0
    success_rate = (stats['received'] / stats['sent'] * 100) if stats['sent'] > 0 else 0
    
    print(f"\nFINAL RESULTS:")
    print(f"Duration:     {elapsed:.1f} seconds")
    print(f"Total sent:   {stats['sent']}")
    print(f"Total recv:   {stats['received']}")
    print(f"Success rate: {success_rate:.1f}%")
    print(f"Average RPS:  {rps:.1f}")
    print(f"Timeouts:     {stats['timeouts']}")
    print(f"Errors:       {stats['errors']}")
    
    if success_rate < 95:
        print("WARNING: Low success rate - server may be overloaded")
    elif success_rate > 99:
        print("EXCELLENT: Server handling load very well")

def menu():
    """Main menu to choose between normal mode and load test."""
    print("Timezone Server Test Client")
    print("1. Normal interactive mode")
    print("2. Load test mode")
    
    choice = input("\nSelect mode (1 or 2): ").strip()
    
    if choice == "2":
        loadtest()
    else:
        main()

if __name__ == "__main__":
    menu()

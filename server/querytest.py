import socket
import json

# Server configuration
server_addr = "timezoned.circuitflow.eu"  # Replace with your server's address
server_port = 2342  # Replace with your server's UDP port
buffer_size = 1472  # Match the server's max packet size
max_retries = 3      # Number of retries for missing chunks

def send_query(sock, query):
    """Send a query to the server."""
    sock.sendto(query.encode(), (server_addr, server_port))
    print(f"Query sent: {query}")

def receive_response(sock):
    """Receive the full response from the server."""
    response = b""
    try:
        # Receive the first packet
        data, addr = sock.recvfrom(buffer_size)
        response = data.decode()

        # Check if this is a multi-packet JSON response
        if response.startswith("{") and "|" in response:
            print("\nResponse is split into multiple chunks.")
            return reassemble_chunks(sock, response)

        # If not a JSON response, return as-is
        return response
    except socket.timeout:
        print("Timeout reached while waiting for response.")
        return None

def reassemble_chunks(sock, first_packet):
    """Reassemble a multi-chunk JSON response."""
    chunks = {}
    total_chunks = None

    # Process the first packet
    try:
        metadata, chunk = first_packet.split("|", 1)
        metadata = json.loads(metadata)
        total_chunks = metadata['total']
        print(f"Expecting {total_chunks} chunks...")
        chunks[metadata['current']] = chunk
        print(f"Receiving chunk {metadata['current']}/{total_chunks}...")

        # Receive remaining chunks
        while len(chunks) < total_chunks:
            try:
                data, addr = sock.recvfrom(buffer_size)
                metadata, chunk = data.decode().split("|", 1)
                metadata = json.loads(metadata)
                if metadata['current'] not in chunks:
                    chunks[metadata['current']] = chunk
                    print(f"Receiving chunk {metadata['current']}/{total_chunks}...")
            except socket.timeout:
                print("Timeout reached while receiving chunks.")
                break

        # Check for missing chunks
        missing_chunks = set(range(1, total_chunks + 1)) - set(chunks.keys())
        if missing_chunks:
            print(f"Missing chunks: {sorted(missing_chunks)}")
            return None

        # Reassemble the full response
        print("All chunks received. Reassembling response...")
        full_response = "".join(chunks[i] for i in range(1, total_chunks + 1))
        return full_response
    except (KeyError, ValueError) as e:
        print(f"Error during chunk reassembly: {e}")
        return None

def main():
    print("UDP Client for Server Testing")
    print("Type your requests below (e.g., Asia.json, Europe/Berlin, GB, GEOIP). Type 'exit' to quit.")

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2)  # Set timeout for receiving data

    try:
        while True:
            query = input("\nEnter your query: ").strip()
            if query.lower() == "exit":
                print("Exiting the client. Goodbye!")
                break

            retry_count = 0
            while retry_count < max_retries:
                send_query(sock, query)
                print("Waiting for server response...")

                response = receive_response(sock)
                if response:
                    print("\nServer Response:")
                    print(response)
                    break
                else:
                    retry_count += 1
                    print(f"Retrying... ({retry_count}/{max_retries})")

            if retry_count == max_retries:
                print("Failed to retrieve complete response after multiple retries.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
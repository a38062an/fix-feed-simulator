import socket
import struct
import threading
import time
import collections
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.gridspec import GridSpec

# ------------------------------------------------------------
# Configuration
# ------------------------------------------------------------
# Multicast group and port used by the C++ UDP senders
MULTICAST_GROUP = "239.255.1.1"
MULTICAST_PORT = 9999
RECEIVE_BUFFER_SIZE = 4096

# ------------------------------------------------------------
# Data Storage (time-series buffers)
# ------------------------------------------------------------
# Keep a fixed-length history for plotting
price_history = collections.deque([100.0] * 100, maxlen=100)
throughput_history = collections.deque([0] * 50, maxlen=50)
log_history = collections.deque(maxlen=12)

# Metrics shared between threads
packet_counter = 0
previous_packet_counter = 0
current_mid_price = 100.0
running_flag = True


# ------------------------------------------------------------
# FIX Message Parser
# ------------------------------------------------------------
def parse_fix_message(raw_bytes):
    """
    Parse a FIX message from raw UDP bytes and return a dictionary
    with descriptive keys. We extract bid/ask price and sizes (tags
    270 and 271) for entry types 0 (bid) and 1 (ask).
    """
    try:
        decoded = raw_bytes.decode("utf-8", errors="ignore")
        fields = decoded.split("\x01")

        result = {
            "raw_message": decoded.replace("\x01", "|")
        }

        # Working variables while iterating
        current_entry_type = None

        # Initialize with sensible defaults
        result["bid_price"] = 0.0
        result["bid_size"] = 0
        result["ask_price"] = 0.0
        result["ask_size"] = 0

        for field in fields:
            if not field or "=" not in field:
                continue
            tag, value = field.split("=", 1)

            if tag == "35":
                result["message_type"] = value
            elif tag == "55":
                result["symbol"] = value
            elif tag == "269":
                # Entry type: 0 = Bid, 1 = Ask
                current_entry_type = value
            elif tag == "270":
                # Price
                if current_entry_type == "0":
                    result["bid_price"] = float(value)
                elif current_entry_type == "1":
                    result["ask_price"] = float(value)
            elif tag == "271":
                # Size / Quantity
                if current_entry_type == "0":
                    result["bid_size"] = int(value)
                elif current_entry_type == "1":
                    result["ask_size"] = int(value)

        # Compute mid price when both bid and ask are present
        if result["bid_price"] > 0 and result["ask_price"] > 0:
            result["mid_price"] = (result["bid_price"] + result["ask_price"]) / 2.0

        return result

    except Exception as exc:  # keep it simple and return error info
        return {"error": str(exc)}


# ------------------------------------------------------------
# Network receiver thread (FIXED FOR MACOS LOOPBACK)
# ------------------------------------------------------------
def network_receive_thread():
    """
    Join the multicast group and receive UDP packets. Parsed messages
    update shared metrics used by the plotting thread.
    """
    global packet_counter, current_mid_price, running_flag

    # 1. Create the UDP Socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

    # 2. Set Reuse Options (Critical for restarting script without 'Address already in use')
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, "SO_REUSEPORT"):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except AttributeError:
        pass

    # 3. Bind to the Port
    # On macOS, we bind to "" (INADDR_ANY) to catch traffic from all interfaces
    try:
        sock.bind(("", MULTICAST_PORT))
    except Exception as e:
        print(f"Error binding to port {MULTICAST_PORT}: {e}")
        return

    # 4. Join the Multicast Group (THE FIX)
    # We must tell the OS to join the group specifically on the LOOPBACK interface (127.0.0.1)
    # because that is where the C++ code is sending the data.
    
    group_bin = socket.inet_aton(MULTICAST_GROUP)
    iface_bin = socket.inet_aton("127.0.0.1") # Explicitly use Loopback
    
    # struct.pack("4s4s", ...) means:
    # 4s = 4 bytes for the Group IP
    # 4s = 4 bytes for the Interface IP
    mreq = struct.pack("4s4s", group_bin, iface_bin)

    try:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    except Exception as e:
        print(f"Failed to join multicast group: {e}")
        return

    print(f"Listening for market feed on {MULTICAST_GROUP}:{MULTICAST_PORT} via Loopback (127.0.0.1)...")

    # 5. Receive Loop
    while running_flag:
        try:
            # Set a timeout so we can check running_flag periodically (allows Ctrl+C to work)
            sock.settimeout(1.0) 
            
            try:
                raw = sock.recv(RECEIVE_BUFFER_SIZE)
            except socket.timeout:
                continue # Loop back and check running_flag

            packet_counter += 1
            parsed = parse_fix_message(raw)

            if "mid_price" in parsed:
                current_mid_price = parsed["mid_price"]

            timestamp = time.strftime("%H:%M:%S")

            # Build a human-friendly log line
            if parsed.get("bid_price", 0) > 0 and parsed.get("ask_price", 0) > 0:
                symbol = parsed.get("symbol", "???")
                line = (
                    f"[{timestamp}] {symbol:<4} | "
                    f"BID {parsed['bid_price']:.2f} ({parsed['bid_size']}) x "
                    f"ASK {parsed['ask_price']:.2f} ({parsed['ask_size']})"
                )
                log_history.append(line)
            else:
                raw_preview = parsed.get("raw_message", "")[:50]
                log_history.append(f"[{timestamp}] RAW: {raw_preview}...")

        except Exception as exc:
            print(f"Socket error: {exc}")
            break
    
    sock.close()

# ------------------------------------------------------------
# Metrics updater thread
# ------------------------------------------------------------
def metrics_update_thread():
    """Periodically compute throughput (TPS) and update series buffers."""
    global previous_packet_counter
    while running_flag:
        time.sleep(0.1)
        diff = packet_counter - previous_packet_counter
        tps = diff * 10  # convert 0.1s window to per-second rate
        throughput_history.append(tps)
        price_history.append(current_mid_price)
        previous_packet_counter = packet_counter


# ------------------------------------------------------------
# Matplotlib animation callback
# ------------------------------------------------------------
def animate_frame(_frame_index, price_axis, tps_axis, log_axis):
    # Price chart
    price_axis.clear()
    price_axis.plot(price_history, color="#00ffff", linewidth=1.5)
    price_axis.set_title(f"Live Market Feed (ESZ5) - Price: {current_mid_price:.2f}", color="white", fontsize=14, pad=10)
    price_axis.set_facecolor("#1e1e1e")
    price_axis.grid(True, color="#333333")
    if price_history:
        min_v = min(price_history)
        max_v = max(price_history)
        price_axis.set_ylim(min_v - 0.25, max_v + 0.25)

    # Throughput chart
    tps_axis.clear()
    tps_axis.plot(throughput_history, color="#ff00ff", linewidth=1.5)
    current_tps = throughput_history[-1] if throughput_history else 0
    tps_axis.set_title(f"Network Throughput: {current_tps} TPS", color="white", fontsize=12, pad=10)
    tps_axis.set_facecolor("#1e1e1e")
    tps_axis.grid(True, color="#333333")
    tps_axis.set_ylim(0, max(max(throughput_history) if throughput_history else 100, 1000))

    # Log panel
    log_axis.clear()
    log_axis.axis("off")
    log_axis.text(0.01, 1.0, "REAL-TIME FIX DECODER LOG (Bid/Ask Volumes)", color="yellow", fontsize=10, weight="bold")

    y = 0.85
    for line in reversed(list(log_history)):
        color = "white"
        if "BID" in line:
            color = "#00ff00"
        log_axis.text(0.01, y, line, color=color, fontsize=9, family="monospace")
        y -= 0.08


if __name__ == "__main__":
    # Start network and metrics threads as daemons so they exit with the process
    receiver_thread = threading.Thread(target=network_receive_thread, daemon=True)
    receiver_thread.start()

    metrics_thread = threading.Thread(target=metrics_update_thread, daemon=True)
    metrics_thread.start()

    plt.style.use("dark_background")
    figure = plt.figure(figsize=(12, 8))
    figure.subplots_adjust(hspace=0.4, top=0.9, bottom=0.05, left=0.08, right=0.95)

    grid = GridSpec(3, 1, height_ratios=[3, 2, 2])
    axis_price = figure.add_subplot(grid[0])
    axis_tps = figure.add_subplot(grid[1])
    axis_log = figure.add_subplot(grid[2])

    animator = animation.FuncAnimation(figure, animate_frame, fargs=(axis_price, axis_tps, axis_log), interval=100)

    try:
        plt.show()
    except KeyboardInterrupt:
        # Allow graceful shutdown if run from terminal
        running_flag = False
        print("Shutting down dashboard...")
import serial
import serial.tools.list_ports
import time
import socket
import struct
import json
import math

def recv_all(sock, num_bytes):
    data = bytearray()
    while len(data) < num_bytes:
        packet = sock.recv(num_bytes - len(data))
        if not packet:
            raise BrokenPipeError("Simulator dropped the connection.")
        data.extend(packet)
    return bytes(data)

class InfiniteFlightV2:
    def __init__(self, ip, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((ip, port))
        self.states = {}
        self.fetch_manifest()

    def fetch_manifest(self):
        print("Fetching aircraft data manifest...")
        self.sock.sendall(struct.pack('<iB', -1, 0))
        
        res_id = struct.unpack('<i', recv_all(self.sock, 4))[0]
        res_len = struct.unpack('<i', recv_all(self.sock, 4))[0]
        payload = recv_all(self.sock, res_len)
        
        str_len = struct.unpack('<i', payload[:4])[0]
        manifest_str = payload[4:4+str_len].decode('utf-8')
        
        for line in manifest_str.split('\n'):
            if line.strip():
                parts = line.split(',')
                if len(parts) == 3:
                    self.states[parts[2]] = {'id': int(parts[0]), 'type': int(parts[1])}
        print(f"Manifest loaded! Tracking {len(self.states)} variables.")

    def get_state(self, path):
        if path not in self.states:
            return 0.0
            
        state_id = self.states[path]['id']
        data_type = self.states[path]['type']
        
        self.sock.sendall(struct.pack('<iB', state_id, 0))
        
        ret_id = struct.unpack('<i', recv_all(self.sock, 4))[0]
        ret_len = struct.unpack('<i', recv_all(self.sock, 4))[0]
        payload = recv_all(self.sock, ret_len)
        
        if ret_id != state_id:
            return 0.0
            
        if data_type == 0:   return float(payload[0] != 0)
        elif data_type == 1: return float(struct.unpack('<i', payload)[0])
        elif data_type == 2: return struct.unpack('<f', payload)[0]
        elif data_type == 3: return struct.unpack('<d', payload)[0]
        else: return 0.0


def discover_infinite_flight():
    print("Listening for Infinite Flight broadcast on UDP 15000...")
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1) 
    udp_socket.bind(("", 15000))
    udp_socket.settimeout(10.0) 

    try:
        data, _ = udp_socket.recvfrom(4096)
        info = json.loads(data.decode('utf-8'))
        
        ip_addr = next((ip for ip in info.get("addresses", []) if "." in ip), None)
        tcp_port = info.get("port", 10112) 
        device_name = info.get("deviceName", "Unknown Device")
        
        if ip_addr:
            print(f"Found '{device_name}' at {ip_addr}:{tcp_port}")
            return ip_addr, tcp_port
        return None, None
    except socket.timeout:
        print("Discovery timed out.")
        return None, None
    finally:
        udp_socket.close()

def auto_connect_gd32():
    print("Searching for GD32 Serial Connection...")
    while True:
        ports = serial.tools.list_ports.comports()
        available_ports = [p.device for p in ports if 'USB' in p.device or 'ACM' in p.device or 'COM' in p.device]
        
        if available_ports:
            for port in available_ports:
                try:
                    print(f"Attempting to connect to {port}...")
                    ser = serial.Serial(port, baudrate=115200, timeout=1)
                    ser.write("START\n".encode('utf-8'))
                    print(f"AEM Mode: SUCCESSFULLY STARTED on {port}\n")
                    return ser
                except serial.SerialException:
                    print(f"Port {port} is busy or unavailable. Trying next...")
        
        print("No valid GD32 board found. Plug it in! Retrying in 2 seconds...")
        time.sleep(2)

gd32_serial = auto_connect_gd32()

if_ip, if_port = discover_infinite_flight()
while not if_ip:
    print("Infinite Flight not found. Retrying in 2 seconds...")
    time.sleep(2)
    if_ip, if_port = discover_infinite_flight()

while True:
    try:
        print(f"Connecting Binary TCP Socket to {if_ip}:{if_port}...")
        if_api = InfiniteFlightV2(if_ip, if_port)
        print("TCP Connected! Streaming telemetry to GD32...")
        
        while True:
            alt_ft = int(if_api.get_state("aircraft/0/altitude_msl"))
            mach = if_api.get_state("aircraft/0/mach_speed")
            surface_temp_c = if_api.get_state("environment/temperature")
            flight_time_min = if_api.get_state("simulator/flight_time") / 60.0
            
            oat_c = surface_temp_c - (1.98 * (alt_ft / 1000.0))
            
            fuel_temp_c = max(oat_c, 25.0 - (flight_time_min * 0.3))
            
            heading_rad = if_api.get_state("aircraft/0/heading_magnetic") 
            heading_deg = math.degrees(heading_rad)
            
            total_weight = if_api.get_state("aircraft/0/systems/load/total_weight")
            left_tank = if_api.get_state("aircraft/0/systems/fuel/tank/0/fuel_remaining")
            right_tank = if_api.get_state("aircraft/0/systems/fuel/tank/1/fuel_remaining")
            
            telemetry_packet = f"{{{alt_ft},{mach:.2f},{oat_c:.1f},{fuel_temp_c:.1f},{heading_deg:.0f},{left_tank:.1f},{right_tank:.1f},{total_weight:.1f}}}\n"
            
            gd32_serial.write(telemetry_packet.encode('utf-8'))
            print(f"Transmitted: {telemetry_packet.strip()}")
            
            time.sleep(1)

    except (BrokenPipeError, ConnectionResetError, ConnectionRefusedError):
        print("Simulator not flying or connection dropped. Retrying in 2 seconds...")
        time.sleep(2)
    except socket.timeout:
        print("TCP timeout. Retrying...")
        time.sleep(2)
    except serial.SerialException:
        print("\nERROR: GD32 Board Unplugged! Halting HIL stream.")
        gd32_serial = auto_connect_gd32()
    except KeyboardInterrupt:
        try:
            gd32_serial.write("STOP\n".encode('utf-8'))
        except:
            pass
        print("\nSystem Safely STOPPED")
        break
import socket

SERVER_IP = "localhost"
SERVER_PORT = 10000

def create_socket():
    try:
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        print("[SUCCESS] Socket created!")
        return client_socket
    except:
        print("[ERROR] Cannot create socket.")
        return NULL


def server_connect(client_socket, server_ip, server_port, init_id):
    try:
        # 클라이언트 ID 송신
        client_socket.connect((server_ip, server_port))
        client_socket.sendall(f"{init_id}\n".encode("utf-8"))
        print("[SUCCESS] Data sent.")
        
        # 핸드셰이크 수신
        f = client_socket.makefile('r', encoding="utf-8")
        packet = f.readline()
        handshake = packet.rstrip('\n')
        
        print(f"[RECEIVED] handshake: {handshake}")
        
        if (handshake == "OK"):
            return True
        else:
            return False
        
    except:
        client_socket.close()
        return False
    
    return True

def send_plate_text(client_socket, gate, action, plate_number):
    client_socket.sendall(f"{gate}:{action}:{plate_number}\n".encode("utf-8"))
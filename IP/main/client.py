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
        client_socket.connect((server_ip, server_port))
        client_socket.sendall(init_id.encode("utf-8"))
        print("[SUCCESS] Data sent.")
    except:
        client_socket.close()
        return False
    
    return True

    
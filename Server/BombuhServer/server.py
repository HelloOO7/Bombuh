from machine import I2C
from machine import Pin
from io import BytesIO
import _thread
import sys

import boardconst
import bitcvtr
from bitcvtr import *

from synch import Semaphore

class ClientSocket:
    COMM_MAGIC_START = 0xFE
    COMM_MAGIC_END = 0xEF
    COMM_RESPONSE_COMMAND = 0xFF

    i2c: I2C
    device: int
    global_i2c_mutex: Semaphore = Semaphore()

    def __init__(self, i2c, device) -> None:
        self.i2c = i2c
        self.device = device

    def id(self) -> int:
        return self.device

    def lock_mutex(self):
        ClientSocket.global_i2c_mutex.lock()

    def release_mutex(self):
        ClientSocket.global_i2c_mutex.release()

    def send_packet(self, content: bytes) -> None:
        self.lock_mutex()
        out = DataOutput().write_u8(ClientSocket.COMM_MAGIC_START).write_u16(len(content))
        if type(content) is bytes:
            out.write(content)
        else:
            out.write(bytes(content))
        
        buf = out.buffer()
        size = len(buf)
        index = 0
        while size > 0:
            write_size = min(size, 32)
            self.send(buf[index:index + write_size])
            size -= write_size
            index += write_size
        self.release_mutex()

    def read_packet(self) -> bytes:
        self.lock_mutex()
        header = self.read(3, True)
        if (header[0] != ClientSocket.COMM_MAGIC_START):
            print("Invalid packet start: ", header)
            return None
        size = bitcvtr.to_u16(header[1:])
        ret = bytes()
        while size > 0:
            read_size = min(size, 32)
            ret += self.read(read_size, True)
            size -= read_size

        print("Read packet content", ret)
        self.release_mutex()
        return ret
    
    @staticmethod
    def ensure_bytes(obj) -> bytes:
        if type(obj) is bytes:
            return obj
        else:
            return bytes(obj)
    
    def send_command_response(self, cmd: int, resp_channel: int, params = None) -> None:
        data = bytes([cmd, resp_channel])
        if (params):
            data += ClientSocket.ensure_bytes(params)
        print("Sending response packet", data)
        self.send_packet(data)

    def send_command(self, cmd: int, params = None) -> bytes:
        data = bytes([cmd])
        if (params):
            data += ClientSocket.ensure_bytes(params)
        
        self.send_packet(data)
        return self.read_packet()

    @staticmethod
    def is_iterable(obj):
        try:
            iter(obj)
            return True
        except TypeError:
            return False

    def send(self, buf: bytes):
        if type(buf) is not bytes:
            raise TypeError("Can only send bytes!")
        try:
            self.i2c.writeto(self.device, buf)
        except OSError as e:
            print("len:", len(buf), "data:", buf, file=sys.stderr)
            raise e

    def read(self, size: int, stop):
        return self.i2c.readfrom(self.device, size, stop)
    
class DeviceHandle:
    __socket__: ClientSocket

    def __init__(self, sock: ClientSocket) -> None:
        self.__socket__ = sock

    def unique_id(self) -> int:
        return self.__socket__.id()
    
    def issock(self, sock: ClientSocket) -> bool:
        return self.__socket__ is sock

class RequestHandler:
    def decode(self, device: DeviceHandle, io: DataInput) -> object:
        return None
    
    def execute(self, request) -> None:
        pass

    def respond(self, request):
        return []

class Server:
    CMD_POLL = 1
    CMD_RESPONSE = 2
    CMD_EVENT = 3
    CMD_HANDSHAKE = 4

    HANDSHAKE_CHECK_CODE = 0x616C754A

    i2c: I2C
    devices: list[ClientSocket]
    permanent_devices: list[ClientSocket]

    handlers: dict[int, RequestHandler]

    mutex: Semaphore

    def __init__(self) -> None:
        self.i2c = I2C(0, freq=boardconst.I2C_BAUDRATE, scl=Pin(boardconst.PIN_I2C_SCL), sda=Pin(boardconst.PIN_I2C_SDA), timeout=1000000)
        self.devices = []
        self.mutex = Semaphore()
        self.handlers = {}
        self.permanent_devices = []

    def i2cwrite(self, addr:int, list) -> None:
        self.i2c.writeto(addr, bytes(list))

    def lock_mutex(self):
        self.mutex.lock()

    def release_mutex(self):
        self.mutex.release()

    def discover(self) -> None:
        self.lock_mutex()

        self.devices.clear()
        for dev in self.i2c.scan():
            print("Found device", dev)
            self.i2cwrite(dev, [0xEA])
            if (self.i2c.readfrom(dev, 1)[0] == 0xAE):
                print("Device ping succeeded")
                self.add_device(dev)
            else:
                print("Device ping failed")
        
        for perm in self.permanent_devices:
            self.devices.append(perm)

        self.release_mutex()

    def add_socket(self, socket: ClientSocket, permanent: bool = False):
        if socket not in self.devices:
            self.devices.append(socket)
            if (permanent):
                self.permanent_devices.append(socket)

    def remove_device(self, socket: ClientSocket):
        if socket in self.devices:
            self.devices.remove(socket)
        else:
            print("Failed to remove socket: not found!")
        if socket in self.permanent_devices:
            self.permanent_devices.remove(socket)

    def shake_hands_with(self, dev: ClientSocket, request, callback):
        self.lock_mutex()
        handshake_resp = dev.send_command(Server.CMD_HANDSHAKE, request)
        io = DataInput(BytesIO(handshake_resp))
        if not callback(DeviceHandle(dev), io):
            print("Handshake failed!")
            self.devices.remove(dev)
            
        self.release_mutex()

    def shake_hands(self, request, callback) -> None:
        self.lock_mutex()

        for dev in self.devices:
            self.shake_hands_with(dev, request, callback)

        self.release_mutex()

    def regist_handler(self, id: str, handler: RequestHandler):
        self.handlers[Server.str_hash(id)] = handler

    @staticmethod
    def str_hash(cmd: str) -> int:
        hval = 0x811c9dc5
        for byte in cmd.encode():
            hval = hval ^ byte
            hval = (hval * 0x01000193) & 0xFFFFFFFF
        return hval

    def add_device(self, address: int) -> ClientSocket:
        sock = ClientSocket(self.i2c, address)
        self.devices.append(sock)
        return sock

    def sync(self) -> None:
        self.lock_mutex()
        exec_queue = []

        for device in self.devices:
            response = device.send_command(Server.CMD_POLL)
            file = DataInput(BytesIO(response))
            count = file.read_u8()
            for i in range(count):
                channel = file.read_u8()
                command_hash = file.read_u32()
                params_size = file.read_u16()
                print("Device", device, "requested command", command_hash, "params size", params_size, "total size", len(response), "pos", file.tell())
                params_start = file.tell()
                handler = self.handlers[command_hash]
                if (handler):
                    exec_queue.append((device, channel, handler, handler.decode(DeviceHandle(device), file)))

                file.seek(params_start + params_size)

        for entry in exec_queue:
            entry[2].execute(entry[3])

        for entry in exec_queue:
            device: ClientSocket = entry[0]
            response = entry[2].respond(entry[3])
            resp_ch = entry[1]
            if (response):
                device.send_command_response(Server.CMD_RESPONSE, resp_ch, response)
            else:
                device.send_command_response(Server.CMD_RESPONSE, resp_ch)

        self.release_mutex()

    def make_event_packet(self, id: int, params = None):
        data = [id]
        if (params):
            data += params
        return data

    def send_event(self, dev: DeviceHandle, event_packet):
        self.lock_mutex()
        dev.__socket__.send_command(Server.CMD_EVENT, event_packet)
        self.release_mutex()


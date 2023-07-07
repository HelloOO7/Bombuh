import server
from bitcvtr import DataOutput
from bomb import Bomb, ModuleFlag

class VirtualModule:
    name: str
    bomb: Bomb
    event_bits: int

    next_response: bytes

    def __init__(self, bomb: Bomb, name, *events) -> None:
        self.name = name
        self.bomb = bomb
        self.event_bits = 0
        for e in events:
            self.event_bits |= (1 << e)

    def create_socket(self) -> server.ClientSocket:
        return VirtualModuleSocket(self)

    def handle_event(self, id: int, data):
        pass

    def comm_poll(self, data: bytes) -> bytes:
        # no queries
        return bytes([0])
    
    def comm_response(self, data: bytes) -> bytes:
        # empty
        return bytes()
    
    def comm_event(self, data: bytes) -> bytes:
        self.handle_event(data[0], data[1:])
        return bytes()
    
    def comm_handshake(self, data: bytes) -> bytes:
        out: DataOutput = DataOutput()
        out.write_u32(0x616C754A)
        out.write_u32(self.event_bits)
        out.write_u8(0) #type = module
        out.write_str(self.name)
        out.write_u8(ModuleFlag.DECORATIVE)
        out.write_u16(0) # extra data size
        out.write_u8(0) # variable count
        return out.buffer()

    def handle_packet(self, data: bytes):
        type = data[0]
        self.next_response = [
            0,
            self.comm_poll,
            self.comm_response,
            self.comm_event,
            self.comm_handshake
        ][type](data[1:])

    def respond(self) -> bytes:
        return self.next_response

class VirtualModuleSocket(server.ClientSocket):
    mod: VirtualModule

    def __init__(self, mod: VirtualModule) -> None:
        super().__init__(None, None)
        self.mod = mod

    def send_packet(self, content: bytes) -> None:
        self.lock_mutex()
        self.mod.handle_packet(content)
        self.release_mutex()

    def read_packet(self) -> bytes:
        self.lock_mutex()
        response = self.mod.respond()
        self.release_mutex()
        return response
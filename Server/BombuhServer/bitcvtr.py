from io import BytesIO
import struct

def to_u16(bytes):
    return (bytes[0] & 0xFF) | ((bytes[1] & 0xFF) << 8)

def to_u32(bytes):
    return (bytes[0] & 0xFF) | ((bytes[1] & 0xFF) << 8) | ((bytes[2] & 0xFF) << 16) | ((bytes[3] & 0xFF) << 24)

def from_u16(val: int) -> bytes:
    return bytes([val & 0xFF, (val >> 8) & 0xFF])

def from_u32(val: int) -> bytes:
    return bytes([val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF, (val >> 24) & 0xFF])

def from_f32(val: float) -> bytes:
    return struct.pack('<f', val)

class DataInput:
    io: BytesIO

    def __init__(self, io: BytesIO) -> None:
        self.io = io

    def read(self, size: int) -> bytes:
        return self.io.read(size)

    def tell(self) -> int:
        return self.io.tell()
    
    def seek(self, offset, whence = 0) -> int:
        return self.io.seek(offset, whence)

    def read_u8(self) -> int:
        return self.io.read(1)[0]

    def read_u16(self) -> int:
        return to_u16(self.io.read(2))
    
    def read_u32(self) -> int:
        return to_u32(self.io.read(4))
    
    def read_str(self) -> str:
        size = self.read_u8()
        return self.read(size).decode()
    
    def read_cstr(self) -> str:
        str = bytes()
        while True:
            byte = self.io.read(1)
            if byte[0] == 0:
                break
            else:
                str += byte
        return str.decode()
    
class DataOutput:
    io: BytesIO

    def __init__(self) -> None:
        self.io = BytesIO()

    def write(self, data):
        self.io.write(data)
        return self

    def tell(self) -> int:
        return self.io.tell()
    
    def seek(self, offset, whence = 0) -> int:
        return self.io.seek(offset, whence)

    def write_b8(self, value: bool):
        return self.write_u8(1 if value else 0)

    def write_u8(self, value: int):
        self.io.write(bytes([value]))
        return self

    def write_u16(self, value: int):
        self.io.write(from_u16(value))
        return self
    
    def write_u32(self, value: int):
        self.io.write(from_u32(value))
        return self
    
    def write_str(self, value: str):
        self.write_u8(len(value))
        self.write(value.encode())
        return self
    
    def write_cstr(self, value: str, null_terminated: bool = True):
        self.write(value.encode())
        if (null_terminated):
            self.write_u8(0)
        return self

    def write_pointer(self, value: int):
        self.write_u16(value)
        return self
    
    def alloc_pointer(self):
        class TempPointer:
            addr: int

            def __init__(self, out) -> None:
                self.out = out
                self.addr = out.tell()
                out.write_pointer(0)

            def set(self, val: int) -> None:
                pos = self.out.tell()
                self.out.seek(self.addr)
                self.out.write_pointer(val)
                self.out.seek(pos)

            def set_here(self) -> None:
                self.set(self.out.tell())

        return TempPointer(self)
    
    def buffer(self) -> bytes:
        return self.io.getvalue()
from bitcvtr import DataInput
from server import *
from bitcvtr import *
import time
import random

from server import DeviceHandle
from dfplayer import Player

class VariableType:
    NULL = 0
    STR = 1
    INT = 2
    LONG = 3
    BOOL = 4
    STR_ENUM = 5

class ComponentVariable:
    name: str
    type: int
    value: object

    def __init__(self, io: DataInput) -> None:
        self.name = io.read_str()
        self.type = io.read_u8()
        self.value = None

class ComponentConfig:
    id: int
    variables: dict[str, str]

    def __init__(self, id: int) -> None:
        self.id = id
        self.variables = dict()

class BombConfig:
    time_limit_ms: int
    strikes: int

    component_vars: list[ComponentConfig]

    def __init__(self) -> None:
        self.component_vars = []

class ComponentHandleBase:
    comm_device: DeviceHandle
    accept_event_bits: int
    variables: list[ComponentVariable]

    config_cache: bytes

    def __init__(self, dev: DeviceHandle) -> None:
        self.comm_device = dev
        self.config_cache = None

    def accepts_event(self, eventId: int) -> bool:
        return (self.accept_event_bits & (1 << eventId)) != 0

    def read_vars(self, io: DataInput) -> None:
        self.variables = []
        var_cnt = io.read_u8()
        for i in range(var_cnt):
            self.variables.append(ComponentVariable(io))

    def id(self):
        return self.comm_device.unique_id()
    
    def build_config(self) -> bytes:
        return bytes()

class ModuleFlag:
    NEEDY = (1 << 0)
    DECORATIVE = (1 << 1)
    DEFUSABLE = (1 << 2)

class ModuleHandle(ComponentHandleBase):
    name: str
    flags: int
    extra: bytes

    def __init__(self, dev:DeviceHandle, io: DataInput) -> None:
        super().__init__(dev)
        self.name = io.read_str()
        self.flags = io.read_u8()
        data_size = io.read_u16()
        if (data_size > 0):
            self.extra = io.read(data_size)
        else:
            self.extra = None
        self.read_vars(io)

    def build_config(self) -> bytes:
        out = DataOutput()
        out.write_u16(len(self.variables))
        vars_ptr = out.alloc_pointer()
        content_ptrs = []
        vars_ptr.set_here()
        for var in self.variables:
            out.write_u32(Server.str_hash(var.name))
            out.write_u8(var.type)
            if (var.type == VariableType.STR) or (var.type == VariableType.STR_ENUM):
                content_ptrs.append(out.alloc_pointer())
                out.write_u16(0) # padding
            else:
                start = out.tell()
                content_ptrs.append(None)
                {
                    VariableType.BOOL: out.write_b8,
                    VariableType.INT: out.write_u16,
                    VariableType.LONG: out.write_u32
                }[var.type](var.value)
                wsize = out.tell() - start # we need to fill in the rest of the union
                while wsize < 4:
                    out.write_u8(0)
                    wsize += 1
        
        for i in range(len(self.variables)):
            if (content_ptrs[i]):
                content_ptrs[i].set_here()
                {
                    VariableType.STR: out.write_cstr,
                    VariableType.STR_ENUM: out.write_cstr
                }[self.variables[i].type](self.variables[i].value)
        
        return out.buffer()

class LabelHandle(ComponentHandleBase):
    text_options: list[str]
    
    used_option: str
    is_lit: bool

    def __init__(self, dev:DeviceHandle, io: DataInput) -> None:
        super().__init__(dev)
        count = io.read_u8()
        self.text_options = [None] * count
        for i in range(count):
            self.text_options.append(io.read_str())
        self.read_vars(io)

    def build_config(self) -> bytes:
        return DataOutput().write_u32(Server.str_hash(self.used_option)).write_b8(self.is_lit).buffer()

class BatteryHandle(ComponentHandleBase):
    count: int
    size: int

    is_used: bool

    def __init__(self, dev:DeviceHandle, io: DataInput) -> None:
        super().__init__(dev)
        self.count = io.read_u8()
        self.size = io.read_u8()
        self.read_vars(io)

    def build_config(self) -> bytes:
        return DataOutput().write_b8(self.is_used).buffer()

class PortHandle(ComponentHandleBase):
    name: str

    is_used: bool

    def __init__(self, dev:DeviceHandle, io: DataInput) -> None:
        super().__init__(dev)
        self.name = io.read_str()
        self.read_vars(io)

    def build_config(self) -> bytes:
        return DataOutput().write_b8(self.is_used).buffer()

class BombEvent:
    RESET = 0
    CONFIGURE = 1
    ARM = 2
    STRIKE = 3
    EXPLOSION = 4
    DEFUSAL = 5
    LIGHTS_OUT = 6
    LIGHTS_ON = 7
    TIMER_TICK = 8
    TIMER_SYNC = 9

class BombState:
    IDLE = 'IDLE'
    INGAME = 'INGAME'
    SUMMARY = 'SUMMARY'

class SerialFlag:
    CONTAINS_VOWEL = 1
    LAST_DIGIT_EVEN = 2
    LAST_DIGIT_ODD = 4

class Bomb:
    STRIKE_TO_TIMER_SCALE = [1.0, 1.25, 1.5, 3.0, 6.0]
    SERIAL_NUMBER_LENGTH = 6
    SERIAL_NUMBER_CHARS = [ # Base on the logic in KTANE
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'E', 'P',      #replace O with E as it could be confused with Q or 0 - but keep the vowel to consonant ratio
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Z', #remove Y as there could be arguments about whether it is a vowel
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9']

    srv: Server
    
    req_exit: bool
    state: str

    serial_number: str
    random_seed: int

    modules: list[ModuleHandle]
    labels: list[LabelHandle]
    ports: list[PortHandle]
    batteries: list[BatteryHandle]
    all_components: list[ComponentHandleBase]
    dev_to_component_dict: dict[int, ComponentHandleBase]

    bomb_cfg_data: bytes

    timer_limit: float
    timer_last_updated: float
    timer_ms: float
    timer_scale: float
    timer_last_synced: float

    modules_to_defuse: set

    strikes: int
    strikes_max: int

    cause_of_explosion: str

    devices_remaining_to_ready: set
    force_status_report: bool

    def __init__(self, srv: Server) -> None:
        self.srv = srv
        self.state = BombState.IDLE
        self.req_exit = False
        self.modules = []
        self.labels = []
        self.ports = []
        self.batteries = []
        self.all_components = []
        self.timer_last_updated = None
        self.timer_last_synced = None
        self.cause_of_explosion = None
        self.devices_remaining_to_ready = set()
        self.modules_to_defuse = set()
        self.bomb_cfg_data = None
        self.dev_to_component_dict = dict()
        self.strikes = 0
        self.strikes_max = 0
        self.timer_limit = 0.0
        self.timer_ms = 0.0
        self.timer_scale = 1.0
        self.serial_number = '000000'
        self.random_seed = 0
        self.force_status_report = False
        self.register_handlers()

    def register_handlers(self):
        srv = self.srv

        bomb = self

        class GetStrikesHandler(RequestHandler):
            def respond(self, request):
                return [bomb.strikes]
            
        class GetClockHandler(RequestHandler):
            def respond(self, request):
                return bitcvtr.from_u32(bomb.real_timer_int()) + bitcvtr.from_f32(bomb.timer_scale)
            
        class DeviceSpecificHandlerBase(RequestHandler):
            def decode(self, device: DeviceHandle, io: DataInput):
                return {'deviceid': device.unique_id()}
            
        class AckReadyToArmHandler(DeviceSpecificHandlerBase):
            def execute(self, request):
                bomb.device_ready(request['deviceid'])

        class GetBombConfigHandler(RequestHandler):
            def respond(self, request):
                return bomb.bomb_cfg_data
        
        class GetComponentConfigHandler(DeviceSpecificHandlerBase):
            def respond(self, request):
                return bomb.dev_to_component_dict[request['deviceid']].config_cache
            
        class AddStrikeHandler(DeviceSpecificHandlerBase):
            def execute(self, request) -> None:
                device = bomb.dev_to_component_dict.get(request['deviceid'])
                if device and type(device) is ModuleHandle:
                    mod:ModuleHandle = device
                    bomb.add_strike(mod.name)

        class DefuseComponentHandler(DeviceSpecificHandlerBase):
            def execute(self, request) -> None:
                component = bomb.dev_to_component_dict.get(request['deviceid'])
                bomb.defuse_component(component)

        srv.regist_handler("GetStrikes", GetStrikesHandler())
        srv.regist_handler("GetClock", GetClockHandler())
        srv.regist_handler("AckReadyToArm", AckReadyToArmHandler())
        srv.regist_handler("GetBombConfig", GetBombConfigHandler())
        srv.regist_handler("GetComponentConfigByBusAddress", GetComponentConfigHandler())
        srv.regist_handler("AddStrike", AddStrikeHandler())
        srv.regist_handler("DefuseComponent", DefuseComponentHandler())

    @staticmethod
    def pack_buffer(buf: bytes):
        return bitcvtr.from_u16(len(buf)) + buf

    def build_configs(self):
        self.bomb_cfg_data = Bomb.pack_buffer(self.build_bomb_config())

        for comp in self.all_components:
            comp.config_cache = Bomb.pack_buffer(comp.build_config())

    def build_bomb_config(self) -> bytes:
        out = DataOutput()
        
        out.write_u32(self.random_seed).write_cstr(self.serial_number, False).write_u16(self.calc_serial_flags())
        
        out.write_u8(self.strikes_max)
        out.write_u32(int(self.timer_limit))

        out.write_u16(len(self.modules))
        modules_ptr = out.alloc_pointer()
        out.write_u16(len(self.labels))
        labels_ptr = out.alloc_pointer()
        out.write_u16(len(self.ports))
        ports_ptr = out.alloc_pointer()
        out.write_u16(len(self.batteries))
        batteries_ptr = out.alloc_pointer()

        modules_ptr.set_here()
        module_exdata_ptrs = []
        for mod in self.modules:
            out.write_u32(Server.str_hash(mod.name)).write_u8(mod.flags)
            if (mod.extra):
                module_exdata_ptrs.append(out.alloc_pointer())
            else:
                out.write_u16(0)
                module_exdata_ptrs.append(None)
        
        for i in range(len(self.modules)):
            if (module_exdata_ptrs[i]):
                module_exdata_ptrs[i].set_here()
                out.write(self.modules[i].extra)

        labels_ptr.set_here()
        for lbl in self.labels:
            out.write_u32(Server.str_hash(lbl.used_option)).write_b8(lbl.is_lit)

        ports_ptr.set_here()
        for port in self.ports:
            out.write_u32(Server.str_hash(port.name))

        batteries_ptr.set_here()
        for batt in self.batteries:
            out.write_u8(batt.count).write_u8(batt.size)

        return out.buffer()

    def calc_serial_flags(self) -> int:
        flags = 0
        for c in self.serial_number:
            if c in ['A', 'E', 'I', 'O', 'U']:
                flags |= SerialFlag.CONTAINS_VOWEL
                break
        if self.serial_number[Bomb.SERIAL_NUMBER_LENGTH - 1] in ['0', '2', '4', '6', '8']:
            flags |= SerialFlag.LAST_DIGIT_EVEN
        else:
            flags |= SerialFlag.LAST_DIGIT_ODD
        return flags

    def set_serial(self, serial: str) -> None:
        self.serial_number = serial
        self.random_seed = Server.str_hash(serial)

    def set_random_serial(self) -> None:
        self.set_serial(self.generate_serial())

    def generate_serial(self) -> str:
        serial = ''

        for i in range(Bomb.SERIAL_NUMBER_LENGTH - 1):
            serial += random.choice(Bomb.SERIAL_NUMBER_CHARS)
        
        serial += str(random.randint(0, 9)) #ensure digit at the end
        return serial
    
    def add_virtual_device(self, socket: ClientSocket):
        self.srv.add_socket(socket, True)

    def device_ready(self, dev: int):
        print("Device ready:", dev)
        self.devices_remaining_to_ready.remove(dev)

    def handshake_callback(self, dev: DeviceHandle, io: DataInput):
        event_bits = io.read_u32()
        type = io.read_u8()
        map = [ModuleHandle, LabelHandle, PortHandle, BatteryHandle]
        lists: list[list] = [self.modules, self.labels, self.ports, self.batteries]
        if (type < len(map)):
            obj: ComponentHandleBase = map[type](dev, io)
            obj.accept_event_bits = event_bits
            self.all_components.append(obj)
            lists[type].append(obj)
            self.dev_to_component_dict[dev.unique_id()] = obj
        else:
            print("Invalid handshake component type!")

    def discover_modules(self) -> None:
        for list in [self.modules, self.labels, self.batteries, self.ports, self.all_components, self.dev_to_component_dict]:
            list.clear()
            
        self.srv.discover()
        self.srv.shake_hands(self.handshake_callback)
        self.reset()

    def configure(self, config: BombConfig) -> None:
        print("Begin configuration")

        self.timer_limit = config.time_limit_ms
        self.timer_ms = self.timer_limit
        self.strikes_max = config.strikes
        self.strikes = 0
        self.set_random_serial()
        print("Bomb serial number", self.serial_number)

        for cc in config.component_vars:
            dev = self.dev_to_component_dict.get(cc.id)
            if (dev):
                for var in dev.variables:
                    raw_value = cc.variables.get(var.name)
                    print('New value of var', var.name, ':', raw_value)

                    def to_int(value):
                        return int(value)
                    
                    def to_str(value):
                        return str(value)
                    
                    def to_bool(value):
                        return bool(value)
                    
                    var.value = {
                        VariableType.BOOL: to_bool,
                        VariableType.INT: to_int,
                        VariableType.LONG: to_int,
                        VariableType.STR: to_str,
                        VariableType.STR_ENUM: to_str
                    }[var.type](raw_value)

        print("Building config binaries...")
        self.build_configs()

        self.devices_remaining_to_ready.clear()
        for component in self.all_components:
            if (component.accepts_event(BombEvent.CONFIGURE)):
                self.devices_remaining_to_ready.add(component.comm_device.unique_id())

        print("Sending configuration event.")
        self.dispatchEvent(BombEvent.CONFIGURE)

    def configuration_in_progress(self) -> bool:
        return not self.configuration_done()

    def configuration_done(self) -> bool:
        return len(self.devices_remaining_to_ready) == 0

    def defuse_component(self, component: ComponentHandleBase) -> None:
        print("Component defused:",component.id(),"!")
        self.modules_to_defuse.remove(component.id())
        if len(self.modules_to_defuse) == 0:
            self.defuse()

    def arm(self) -> None:
        self.timer_ms = self.timer_limit + 200
        self.strikes = 0
        self.update_timescale()
        self.timer_last_updated = None
        self.timer_last_synced = None
        self.cause_of_explosion = None
        self.modules_to_defuse.clear()
        for module in self.modules:
            if (module.flags & ModuleFlag.DEFUSABLE) != 0:
                self.modules_to_defuse.add(module.id())
        self.dispatchEvent(BombEvent.ARM)
        self.state = BombState.INGAME
        self.update_timer() #start timer after all modules have been armed

    def timer_int(self) -> int:
        return int(self.timer_ms)
    
    def real_timer_int(self) -> int:
        ticks = time.ticks_ms()
        return int(self.timer_ms - time.ticks_diff(ticks, self.timer_last_updated) * self.timer_scale)
    
    def update_timescale(self) -> None:
        self.timer_scale = Bomb.STRIKE_TO_TIMER_SCALE[min(self.strikes, len(Bomb.STRIKE_TO_TIMER_SCALE) - 1)]

    def is_force_status_report(self) -> bool:
        retval = self.force_status_report
        self.force_status_report = False
        return retval

    def add_strike(self, cause: str = '') -> None:
        self.strikes += 1
        print("Strike", self.strikes, "of", self.strikes_max)
        if (self.strikes == self.strikes_max):
            self.explode(cause)
        else:
            self.update_timescale()
            self.dispatchEvent(BombEvent.STRIKE)
            self.dispatchEvent(BombEvent.TIMER_SYNC) # sync timescale
            self.force_status_report = True

    def explode(self, cause: str) -> None:
        print("Bomb EXPLODED, reason:", cause)
        self.state = BombState.SUMMARY
        self.cause_of_explosion = cause
        self.dispatchEvent(BombEvent.EXPLOSION)
        self.force_status_report = True

    def has_game_ended(self) -> bool:
        return self.state == BombState.SUMMARY

    def has_exploded(self) -> bool:
        return self.cause_of_explosion != None
    
    def defuse(self) -> None:
        print("Bomb defused!")
        self.state = BombState.SUMMARY
        self.cause_of_explosion = None
        self.dispatchEvent(BombEvent.DEFUSAL)
        self.force_status_report = True

    def update_timer(self) -> None:
        ts = time.ticks_ms()
        if (self.timer_last_updated is None):
            self.timer_last_updated = ts
        else:
            last_timer = self.timer_ms
            self.timer_ms -= time.ticks_diff(ts, self.timer_last_updated) * self.timer_scale
            self.timer_last_updated = ts
            if (self.timer_ms <= 0):
                self.timer_ms = 0
                self.explode('Time ran out')
            else:
                if (self.timer_ms // 1000 != last_timer // 1000):
                    self.dispatchEvent(BombEvent.TIMER_TICK)
                if self.timer_last_synced is None or time.ticks_diff(ts, self.timer_last_synced) > 5000:
                    self.dispatchEvent(BombEvent.TIMER_SYNC)
                    self.timer_last_synced = ts

    def is_sync_online(self):
        return self.is_game_running() or self.configuration_in_progress()

    def is_game_running(self):
        return self.state == BombState.INGAME

    def update(self):
        if self.is_game_running():
            self.update_timer()
        
        if (self.is_sync_online()):
            self.srv.sync()

    def dispatchEvent(self, eventId: int, params = None):
        packet = self.srv.make_event_packet(eventId, params)
        for comp in self.all_components:
            if comp.accepts_event(eventId):
                self.srv.send_event(comp.comm_device, packet)

    def reset(self) -> None:
        self.state = BombState.IDLE
        self.dispatchEvent(BombEvent.RESET)

    def exit(self) -> None:
        self.reset()
        self.req_exit = True
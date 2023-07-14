from bomb import Bomb, BombEvent, BaseService, VirtualModule
import urequests
import _thread
import time
from server import ClientSocket
import gc

class AudioServerAPI:
    baseUri: str
    req_quit: bool

    request_queue: list
    mutex: _thread.LockType

    def __init__(self, deviceIp) -> None:
        self.baseUri = 'http://' + str(deviceIp) + ':80'
        self.req_quit = False
        self.request_queue = []
        self.mutex = _thread.allocate_lock()

        gc.collect()
        _thread.stack_size(1024)
        _thread.start_new_thread(self.http_thread, ())
    
    def quit(self):
        self.req_quit = True

    def make_body(self, content: dict) -> str:
        out = ''
        for k, v in content.items():
            if (len(out)):
                out += '&'
            out += str(k) + '=' + str(v)
        return out
    
    def http_thread(self):
        while not self.req_quit:
            self.mutex.acquire()
            if len(self.request_queue) > 0:
                data = self.request_queue.pop(0)
                self.make_request(data[0], data[1])
                self.next_request_uri = None
            self.mutex.release()
            time.sleep(0.05)
        print("AudioServer HttpThread exited")

    def make_request(self, url: str, content = {}) -> urequests.Response:
        try:
            resp = urequests.post(self.baseUri + url, data=self.make_body(content), headers={'Content-Type': 'application/x-www-form-urlencoded'})
            if (resp):
                resp.close()
        except OSError:
            pass

    def make_request_async(self, url: str, content = {}) -> None:
        self.mutex.acquire()
        self.request_queue.append((url, content))
        self.mutex.release()

    def stop(self) -> None:
        self.make_request_async("/stop")

    def play_victory(self) -> None:
        self.make_request_async("/play-victory")

    def play_setup(self) -> None:
        self.make_request_async("/play-setup")

    def start_gameplay(self, time_limit: int) -> None:
        self.make_request_async("/start-gameplay", {'time_limit': time_limit})

    def update_gameplay(self, timer: int, timescale: float) -> None:
        self.make_request_async("/update-gameplay", {'timer': timer, 'timescale': timescale})

class BGMModule(VirtualModule):
    api: AudioServerAPI

    victory_time: object

    def __init__(self, bomb: Bomb, api: AudioServerAPI) -> None:
        self.api = api
        self.victory_time = None
        super().__init__(bomb, "mod_BGM", BombEvent.DEFUSAL, BombEvent.EXPLOSION, BombEvent.TIMER_SYNC, BombEvent.RESET, BombEvent.ARM)

    def handle_event(self, id: int, data):
        if id == BombEvent.EXPLOSION or id == BombEvent.RESET:
            print("Stop music.")
            self.api.stop()
        elif id == BombEvent.DEFUSAL:
            self.api.stop()
            self.victory_time = time.ticks_add(time.ticks_ms(), 1000)
        elif id == BombEvent.ARM:
            self.api.start_gameplay(int(self.bomb.timer_limit))
        elif id == BombEvent.TIMER_SYNC:
            self.api.update_gameplay(self.bomb.timer_int(), self.bomb.timer_scale)

    def update(self):
        ts = time.ticks_ms()
        if self.victory_time and (time.ticks_diff(ts, self.victory_time) >= 0):
            self.victory_time = None
            self.api.play_victory()

class AudioServerService(BaseService):
    api: AudioServerAPI
    module: BGMModule
    device: ClientSocket

    def __init__(self, bomb: Bomb, address: str) -> None:
        super().__init__(bomb, address)
        self.api = AudioServerAPI(self.address)
        self.module = BGMModule(self.bomb, self.api)
        self.device = self.module.create_socket()

    def register(self) -> None:
        self.bomb.add_virtual_device(self.device)

    def deregister(self):
        self.bomb.remove_virtual_device(self.device)
        self.api.quit()

    def update(self):
        self.module.update()
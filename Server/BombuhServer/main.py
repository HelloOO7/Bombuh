import time
import sys
import gc
import time

sys.path.append('BombuhServer')

from server import Server, RequestHandler
from bomb import *
import wlan
import web
from bitcvtr import *

print("BombuhServer")

def main():
    print("Initializing access point...")
    ap = wlan.create_ap('Bombuh', 'Julka1234')
    print(ap.ifconfig())

    srv = Server()
    bomb = Bomb(srv)
    print("Discovering I2C devices...")
    bomb.discover_modules()
    print("Device scan done")

    class TestReqHandler(RequestHandler):
        def decode(self, device: DeviceHandle, io: DataInput):
            dat = io.read(16).split(bytes([0]))[0]
            return dat.decode()
        
        def respond(self, request):
            print("req", request)
            return list('ahoj taky'.encode()) + [0]

    srv.regist_handler('Test', TestReqHandler())

    web.start_thread(bomb)

    last_web_sync = None
    WEB_SYNC_INTERVAL = 1000

    while True:
        if (bomb.req_exit):
            break
        bomb.update()
        ts = time.ticks_ms()
        if last_web_sync is None or time.ticks_diff(ts, last_web_sync) > WEB_SYNC_INTERVAL or bomb.is_force_status_report():
            web.report_game_status()
            last_web_sync = ts
        
        time.sleep(0.05)

def atexit():
    web.shutdown()
    gc.collect() #important! clean up after web server
    
try:
    main()
    atexit()
except BaseException as e:
    atexit()
    if type(e) != KeyboardInterrupt:
        raise e
import mod_virtual
from bomb import Bomb, BombEvent
from machine import Pin
import tm1637
import time
import _thread

class TimerModule(mod_virtual.VirtualModule):
    led_pins: list[Pin]
    disp: tm1637.TM1637
    next_blink: int
    blink_state: bool
    mutex: _thread.LockType
    is_on: bool

    def __init__(self, bomb: Bomb) -> None:
        super().__init__(bomb, "mod_Timer", BombEvent.ARM, BombEvent.RESET, BombEvent.DEFUSAL, BombEvent.EXPLOSION, BombEvent.TIMER_TICK, BombEvent.STRIKE)
        self.led_pins = []
        self.led_pins.append(Pin(13, Pin.OUT))
        self.led_pins.append(Pin(19, Pin.OUT))
        self.disp = tm1637.TM1637(Pin(12), Pin(18), 7)
        self.next_blink = None
        self.blink_state = True
        self.mutex = _thread.allocate_lock()
        self.turn_off()

    def turn_on_leds_by_strikes(self):
        nleds = min(2, self.bomb.strikes)
        for i in range(nleds):
            self.led_pins[i].on()

    def handle_event(self, id: int, data):
        self.mutex.acquire()
        if (id == BombEvent.STRIKE):
            self.turn_on_leds_by_strikes()
            if (self.bomb.strikes == self.bomb.strikes_max - 1):
                self.next_blink = time.ticks_add(time.ticks_ms(), 100)
                self.blink_state = True
        elif (id == BombEvent.RESET) or (id == BombEvent.EXPLOSION):
            self.turn_off()
        elif (id == BombEvent.DEFUSAL):
            self.next_blink = None
            self.turn_on_leds_by_strikes()
        elif (id == BombEvent.ARM):
            self.is_on = True
        self.mutex.release()

    def turn_off(self):
        self.disp.write(bytearray([0, 0, 0, 0]))
        self.is_on = False
        for pin in self.led_pins:
            pin.off()

    def ext_update_timer(self):
        self.mutex.acquire()

        if self.is_on:
            if (self.next_blink is not None):
                ts = time.ticks_ms()
                if (time.ticks_diff(ts, self.next_blink) >= 0):
                    for pin in self.led_pins:
                        pin.on() if self.blink_state else pin.off()
                    self.blink_state = not self.blink_state
                    self.next_blink = time.ticks_add(self.next_blink, 100)

            timer = self.bomb.timer_int()
            if (timer < 60 * 1000):
                secs = timer // 1000
                hundredths = (timer // 10) % 100
                digits = [
                    secs // 10,
                    secs % 10,
                    hundredths // 10,
                    hundredths % 10
                ]
                dot = True
            else:
                secs = timer // 1000
                minutes = secs // 60
                if (minutes >= 100):
                    minutes = 99
                secs = secs % 60
                digits = [
                    minutes // 10,
                    minutes % 10,
                    secs // 10,
                    secs % 10
                ]
                dot = False

            self.disp.write(bytearray([
                self.disp.encode_digit(digits[0]),
                self.disp.encode_digit(digits[1]) | 0x80,
                self.disp.encode_digit_flip(digits[2]) | (0 if dot else 0x80),
                self.disp.encode_digit(digits[3])
            ]))
        self.mutex.release()
        time.sleep(0.01)

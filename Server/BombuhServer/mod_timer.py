from bomb import Bomb, BombEvent, VirtualModule
from machine import Pin
import tm1637
import time
import _thread

class TimerModule(VirtualModule):
    led_pins: list[Pin]
    disp: tm1637.TM1637
    mutex: _thread.LockType
    
    clock_is_on: bool

    next_panic_blink: int
    panic_blink_state: bool

    next_clock_blink: int
    clock_blink_state: bool
    clock_blink_count: int

    def __init__(self, bomb: Bomb) -> None:
        super().__init__(bomb, "mod_Timer", BombEvent.ARM, BombEvent.RESET, BombEvent.DEFUSAL, BombEvent.EXPLOSION, BombEvent.TIMER_TICK, BombEvent.STRIKE)
        self.led_pins = []
        self.led_pins.append(Pin(13, Pin.OUT))
        self.led_pins.append(Pin(19, Pin.OUT))
        self.disp = tm1637.TM1637(Pin(12), Pin(18), 7)
        self.next_panic_blink = None
        self.panic_blink_state = True
        self.next_clock_blink = None
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
                self.next_panic_blink = time.ticks_add(time.ticks_ms(), 100)
                self.panic_blink_state = True
        elif (id == BombEvent.RESET) or (id == BombEvent.EXPLOSION):
            self.turn_off()
            self.next_clock_blink = None
            self.next_panic_blink = None
        elif (id == BombEvent.DEFUSAL):
            self.next_panic_blink = None
            self.clock_blink_state = False
            self.clock_blink_count = 0
            self.write_off_clock()
            self.next_clock_blink = time.ticks_add(time.ticks_ms(), 375)
            self.turn_on_leds_by_strikes()
        elif (id == BombEvent.ARM):
            self.clock_is_on = True
        self.mutex.release()

    def write_off_clock(self):
        self.disp.write(bytearray([0, 0, 0, 0]))

    def turn_off_leds(self):
        for pin in self.led_pins:
            pin.off()

    def turn_off(self):
        self.write_off_clock()
        self.clock_is_on = False
        self.turn_off_leds()

    def ext_update_timer(self):
        self.mutex.acquire()

        if self.clock_is_on:
            ts = time.ticks_ms()
            if (self.next_panic_blink is not None):
                if (time.ticks_diff(ts, self.next_panic_blink) >= 0):
                    if self.panic_blink_state:
                        self.turn_on_leds_by_strikes()
                    else:
                        self.turn_off_leds()
                    self.panic_blink_state = not self.panic_blink_state
                    self.next_panic_blink = time.ticks_add(self.next_panic_blink, 100)

            if (self.next_clock_blink is None or self.clock_blink_state):
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
            else:
                self.write_off_clock()

            if (self.next_clock_blink is not None):
                if (time.ticks_diff(ts, self.next_clock_blink) >= 0):
                    self.next_clock_blink = time.ticks_add(self.next_clock_blink, 375)
                    self.clock_blink_state = not self.clock_blink_state
                    self.clock_blink_count += 1
                    if (self.clock_blink_count == 3):
                        self.next_clock_blink = None

        self.mutex.release()

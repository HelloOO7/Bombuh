import mod_virtual
from bomb import Bomb, BombEvent
from dfplayer import Player
import random
import time

class SfxFolder:
    STATIC_SFX = 1
    EXPLOSIONS = 2
    BEEPS = 3

class SfxModule(mod_virtual.VirtualModule):
    STATIC_SFX_TABLE = {
        BombEvent.STRIKE: (1, 0.8),
        BombEvent.DEFUSAL: (2, 0.7)
    }
    EXPLOSION_SOUND_COUNT = 16

    player: Player
    cooldown_end: int

    def __init__(self, bomb: Bomb) -> None:
        super().__init__(bomb, "mod_SFX", BombEvent.DEFUSAL, BombEvent.EXPLOSION, BombEvent.TIMER_TICK, BombEvent.STRIKE)
        self.player = Player()
        self.cooldown_end = None

    def handle_event(self, id: int, data):
        ts = time.ticks_ms()
        if self.cooldown_end is not None and (time.ticks_diff(ts, self.cooldown_end) < 0):
            return
        if id in SfxModule.STATIC_SFX_TABLE:
            info = SfxModule.STATIC_SFX_TABLE[id]
            self.player.volume(info[1])
            self.player.play(SfxFolder.STATIC_SFX, info[0])
            if (id == BombEvent.STRIKE):
                self.cooldown_end = time.ticks_add(ts, 500) # interrupt beeping for 500ms
        else:
            if id == BombEvent.EXPLOSION:
                self.player.volume(0.9)
                self.player.play(SfxFolder.EXPLOSIONS, random.randint(1, SfxModule.EXPLOSION_SOUND_COUNT))
            elif id == BombEvent.TIMER_TICK:
                self.player.volume(0.6)
                index = min(2, self.bomb.strikes) + 1
                self.player.play(SfxFolder.BEEPS, index)

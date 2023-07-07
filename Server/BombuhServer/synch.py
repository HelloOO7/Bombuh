import _thread

class Semaphore:
    lock_count: int
    owner: int
    hw_mutex: _thread.LockType

    def __init__(self) -> None:
        self.lock_count = 0
        self.hw_mutex = _thread.allocate_lock()
        self.owner = None

    def lock(self):
        id = _thread.get_ident()
        if (self.owner == id):
            self.lock_count += 1
        else:
            self.hw_mutex.acquire()
            self.lock_count = 1
            self.owner = id

    def release(self):
        if (self.owner == _thread.get_ident()):
            self.lock_count -= 1
            if (self.lock_count == 0):
                self.hw_mutex.release()
                self.owner = None
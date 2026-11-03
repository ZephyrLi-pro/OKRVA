"""One-slot camera mailbox: inference always consumes the newest frame."""
import threading


class LatestFrame:
    def __init__(self):
        self.condition = threading.Condition()
        self.sequence = 0
        self.value = None
        self.closed = False
        self.error = None

    def publish(self, frame, timestamp):
        with self.condition:
            if self.closed:
                return
            self.sequence += 1
            self.value = (self.sequence, timestamp, frame)
            self.condition.notify_all()

    def wait_after(self, sequence, timeout=.1):
        with self.condition:
            self.condition.wait_for(
                lambda: self.closed or self.sequence > sequence, timeout)
            if self.error is not None:
                raise RuntimeError(self.error)
            if self.closed or self.sequence <= sequence:
                return None
            return self.value

    def close(self, error=None):
        with self.condition:
            self.closed = True
            self.error = str(error) if error is not None else None
            self.condition.notify_all()

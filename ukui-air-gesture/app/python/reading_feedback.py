"""Small native hand indicator, independent of the camera calibration window."""
import time

from gesture_actions import AirGestureDBusClient


class ReadingFeedback:
    def __init__(self, emit, client=None, missing_delay=.6, max_age=1.):
        self.client = client or AirGestureDBusClient()
        self.emit = emit
        self.missing_delay = missing_delay
        self.max_age = max_age
        self.visible = False
        self.last_seen = None

    def update(self, snapshot, now=None):
        now = time.monotonic() if now is None else now
        age = now - snapshot.get('frame_time', 0.)
        if snapshot.get('hand_present') and 0 <= age <= self.max_age:
            self.last_seen = now
        visible = self.last_seen is not None and now - self.last_seen < self.missing_delay
        if visible == self.visible:
            return
        if visible:
            self.client.show_icon('up')
        else:
            self.client.hide_icon()
        self.visible = visible
        self.emit('gesture_indicator', visible=visible, delivery='requested')

    def close(self):
        self.client.hide_icon()
        if self.visible:
            self.emit('gesture_indicator', visible=False, delivery='requested')
        self.visible = False

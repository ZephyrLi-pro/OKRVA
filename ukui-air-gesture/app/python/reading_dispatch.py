"""Deliver reading gestures without replaying an action after a failure."""
from reading_navigation import ReadingNavigationController


class ReadingDispatcher:
    def __init__(self, send, emit):
        self.controller = ReadingNavigationController()
        self.send = send
        self.emit = emit

    def process(self, landmarks, timestamp, aspect=.75, age=0.):
        # Late results must not turn a page after the user's hand has moved on.
        if age > .65:
            self.controller.update(None, timestamp, aspect)
            return None
        direction = self.controller.update(landmarks, timestamp, aspect)
        if direction is None:
            return None
        try:
            self.send(direction)
        except Exception as error:
            self.emit('action_error', direction=direction, error=str(error),
                      automatic_retry=False)
            return None
        self.emit('action_candidate', algorithm='reading', kind='horizontal',
                  direction=direction, delivery='dbus_acknowledged')
        return direction

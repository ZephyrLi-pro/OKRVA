"""The existing native action interface, with bounded D-Bus acknowledgement."""
import asyncio
from dbus_next import Message, MessageType
from gesture_actions import AirGestureDBusClient


class ReadingDBusActions(AirGestureDBusClient):
    async def _send_horizontal(self, direction):
        bus = await self._get_bus()
        reply = await asyncio.wait_for(bus.call(Message(
            destination=self.SERVICE_NAME, path=self.OBJECT_PATH,
            interface=self.INTERFACE_NAME, member='horizontal',
            signature='s', body=[direction])), timeout=1.)
        if reply is None or reply.message_type == MessageType.ERROR:
            raise RuntimeError('Native gesture action failed: ' +
                               (str(reply.body) if reply else 'no reply'))

    def horizontal(self, direction):
        if direction not in ('left', 'right'):
            raise ValueError('Invalid horizontal direction')
        self._ensure_runtime()
        future = asyncio.run_coroutine_threadsafe(
            self._send_horizontal(direction), self._loop)
        try:
            return future.result(timeout=1.5)
        except Exception:
            future.cancel()
            raise

"""Gesture target for an explicit reader session. Never inject global keys."""
import asyncio
import json
from dbus_next import Message, MessageType, MessageFlag
from gesture_actions import AirGestureDBusClient


class ReaderDBusActions(AirGestureDBusClient):
    async def _navigate(self,direction):
        bus=await self._get_bus()
        reply=await asyncio.wait_for(bus.call(Message(
            destination='org.riscv.AIReader',path='/org/riscv/AIReader',
            interface='org.riscv.AIReader1',member='Navigate',
            signature='s',body=[direction],flags=MessageFlag.NO_AUTOSTART)),timeout=20.)
        if reply is None or reply.message_type==MessageType.ERROR:
            raise RuntimeError('Reading window unavailable; no desktop key sent')
        state=json.loads(reply.body[0])
        if not state.get('active') or state.get('page',0)<1:
            raise RuntimeError('Reader has no confirmed current page')
        return state

    def horizontal(self,direction):
        if direction not in ('left','right'):
            raise ValueError('Invalid reading direction')
        self._ensure_runtime()
        future=asyncio.run_coroutine_threadsafe(self._navigate(direction),self._loop)
        try:return future.result(timeout=20.5)
        except Exception:
            future.cancel()
            raise

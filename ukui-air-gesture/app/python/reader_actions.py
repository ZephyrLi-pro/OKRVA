"""Gesture target for an explicit reader session. Never inject global keys."""
import asyncio
import json
from dbus_next import Message, MessageType, MessageFlag
from gesture_actions import AirGestureDBusClient


class ReaderDBusActions(AirGestureDBusClient):
    async def _request(self,member,signature,body,owner=None,timeout=20.):
        bus=await self._get_bus()
        reply=await asyncio.wait_for(bus.call(Message(
            destination=owner or 'org.riscv.AIReader',path='/org/riscv/AIReader',
            interface='org.riscv.AIReader1',member=member,
            signature=signature,body=body,flags=MessageFlag.NO_AUTOSTART)),timeout=timeout)
        if reply is None or reply.message_type==MessageType.ERROR:
            raise RuntimeError('Reading window unavailable; no desktop key sent')
        state=json.loads(reply.body[0])
        if (not isinstance(state,dict) or not state.get('active')
                or type(state.get('page')) is not int or state['page']<1):
            raise RuntimeError('Reader has no confirmed current page')
        state['reader_owner']=reply.sender
        return state

    def _invoke(self,member,signature,body,owner=None,timeout=20.):
        self._ensure_runtime()
        future=asyncio.run_coroutine_threadsafe(
            self._request(member,signature,body,owner,timeout),self._loop)
        try:return future.result(timeout=timeout+.5)
        except Exception:
            future.cancel();raise

    def state(self):
        return self._invoke('GetState','',[],timeout=1.)

    def goto(self,page,owner):
        if type(page) is not int or page<1:raise ValueError('Invalid reader page')
        if not isinstance(owner,str) or not owner.startswith(':'):
            raise ValueError('A unique reader session owner is required')
        return self._invoke('GotoPage','u',[page],owner)

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

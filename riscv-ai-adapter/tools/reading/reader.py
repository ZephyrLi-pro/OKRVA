"""Presentation-specific navigation and current-page publication over session D-Bus."""
import argparse
import asyncio
import concurrent.futures
import datetime
import json
import os
import signal
import socket
from pathlib import Path

from dbus_next import Message, MessageFlag, MessageType, NameFlag, RequestNameReply, DBusError
from dbus_next.aio import MessageBus
from dbus_next.service import ServiceInterface, method, signal as dbus_signal

from office_reader import OfficeReader
from reader_core import ReaderBridge

SERVICE='org.riscv.AIReader'
OBJECT='/org/riscv/AIReader'
INTERFACE='org.riscv.AIReader1'
VOICE='org.ukui.voice_assistant'
VOICE_OBJECT='/org/ukui/voice_assistant'


def notify(text):
    address=os.environ.get('NOTIFY_SOCKET')
    if not address:return
    if address.startswith('@'):address='\0'+address[1:]
    with socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM) as client:
        client.connect(address)
        client.sendall(text.encode())


async def voice_context(bus,title,text,page):
    reply=await asyncio.wait_for(bus.call(Message(destination=VOICE,path=VOICE_OBJECT,
        interface=VOICE,member='SetReadingContext',signature='ssi',body=[title,text,page],
        flags=MessageFlag.NO_AUTOSTART)),timeout=1.5)
    if reply is None or reply.message_type==MessageType.ERROR:
        raise RuntimeError('Voice context unavailable: '+str(reply.body if reply else 'no reply'))
    return reply.body==[True]


class ReaderInterface(ServiceInterface):
    def __init__(self,bridge,stop):
        super().__init__(INTERFACE)
        self.bridge,self.stop=bridge,stop

    @method()
    def GetState(self)->'s':
        return json.dumps(self.bridge.state(),ensure_ascii=False)

    @method()
    async def Navigate(self,direction:'s')->'s':
        try:
            return json.dumps(await self.bridge.navigate(direction),ensure_ascii=False)
        except Exception as error:
            raise DBusError(SERVICE+'.NavigationError',str(error))

    @method()
    async def GotoPage(self,page:'u')->'s':
        try:
            return json.dumps(await self.bridge.goto(page),ensure_ascii=False)
        except Exception as error:
            raise DBusError(SERVICE+'.NavigationError',str(error))

    @method()
    def Close(self)->'b':
        self.stop.set()
        return True

    @dbus_signal()
    def PageChanged(self,state)->'s':
        return state


async def serve(args):
    bus=await MessageBus().connect()
    if args.clear_context:
        try:return await voice_context(bus,'','',0)
        finally:bus.disconnect()
    result=await bus.request_name(SERVICE,NameFlag.DO_NOT_QUEUE)
    if result!=RequestNameReply.PRIMARY_OWNER:
        bus.disconnect()
        raise RuntimeError('Another presentation reader is already running')
    state_dir=Path(args.state_dir).expanduser().resolve()
    state_dir.mkdir(parents=True,exist_ok=True,mode=0o700)
    state_path=state_dir/'state.json'
    last_error=[None]
    def emit(event,**data):
        # Repeated unavailable-voice messages need not fill logs every poll.
        if event=='context_publish_failed':
            if data.get('error')==last_error[0]:return
            last_error[0]=data.get('error')
        elif event=='context_published':last_error[0]=None
        row=dict(event=event,time=datetime.datetime.now().isoformat(timespec='milliseconds'),**data)
        print(json.dumps(row,ensure_ascii=False),flush=True)
    office=OfficeReader(args.file,state_dir/'office-profile',emit,args.start_page)
    worker=concurrent.futures.ThreadPoolExecutor(max_workers=1,thread_name_prefix='reader-uno')
    loop=asyncio.get_running_loop()
    async def call_office(action,*values):
        return await loop.run_in_executor(worker,lambda:getattr(office,action)(*values))
    async def publish(title,text,page):
        return await voice_context(bus,title,text,page)
    stop=asyncio.Event()
    for sig in (signal.SIGTERM,signal.SIGINT):loop.add_signal_handler(sig,stop.set)
    bridge=ReaderBridge(call_office,publish,args.title or Path(args.file).stem,emit)
    interface=ReaderInterface(bridge,stop)
    def voice_owner_changed(message):
        if (message.message_type==MessageType.SIGNAL and message.interface=='org.freedesktop.DBus'
            and message.member=='NameOwnerChanged' and message.body[0]==VOICE):
            bridge.published_version=-1
            bridge.published_at=None
    bus.add_message_handler(voice_owner_changed)
    await bus.call(Message(destination='org.freedesktop.DBus',path='/org/freedesktop/DBus',
        interface='org.freedesktop.DBus',member='AddMatch',signature='s',body=[
        "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='org.ukui.voice_assistant'"]))
    try:
        await call_office('open')
        await bridge.refresh()
        bus.export(OBJECT,interface)
        emit('reader_bridge_ready',service=SERVICE,page=bridge.state()['page'])
        notify('READY=1\nSTATUS=Presentation ready, page '+str(bridge.state()['page']))
        old_version=-1
        while not stop.is_set():
            state=await bridge.refresh()
            # Text stays in memory / D-Bus; the status file contains metadata.
            metadata={key:value for key,value in state.items() if key!='text'}
            temporary=state_path.with_suffix('.tmp')
            temporary.write_text(json.dumps(metadata,ensure_ascii=False,indent=2))
            temporary.chmod(0o600)
            temporary.replace(state_path)
            if state['version']!=old_version:
                interface.PageChanged(json.dumps(metadata,ensure_ascii=False))
                old_version=state['version']
            if not state['active']:
                stop.set()
                break
            try:await asyncio.wait_for(stop.wait(),timeout=.4)
            except asyncio.TimeoutError:pass
    finally:
        await bridge.clear()
        await call_office('close')
        state_path.write_text(json.dumps(bridge.state(),ensure_ascii=False,indent=2))
        state_path.chmod(0o600)
        worker.shutdown(wait=False,cancel_futures=True)
        bus.unexport(OBJECT)
        bus.disconnect()
        emit('reader_bridge_closed',source_unchanged=True)


def main():
    parser=argparse.ArgumentParser(description='Read-only presentation bridge for gestures and voice')
    parser.add_argument('--file')
    parser.add_argument('--title')
    parser.add_argument('--start-page',type=int,default=1)
    parser.add_argument('--state-dir',default='~/.local/state/riscv-ai-reader')
    parser.add_argument('--clear-context',action='store_true')
    args=parser.parse_args()
    if not args.clear_context and not args.file:parser.error('--file is required')
    asyncio.run(serve(args))


if __name__=='__main__':main()

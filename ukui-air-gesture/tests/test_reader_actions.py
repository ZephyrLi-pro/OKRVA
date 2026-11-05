import json
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'app/python'))
from dbus_next import MessageType,MessageFlag
from reader_actions import ReaderDBusActions


class ReaderActionTests(unittest.IsolatedAsyncioTestCase):
    async def test_state_identifies_the_unique_reader_owner_without_autostart(self):
        reply=SimpleNamespace(message_type=MessageType.METHOD_RETURN,sender=':1.77',
            body=[json.dumps({'active':True,'page':13})])
        bus=SimpleNamespace(call=AsyncMock(return_value=reply))
        client=ReaderDBusActions();client._get_bus=AsyncMock(return_value=bus)
        state=await client._request('GetState','',[],timeout=1.)
        self.assertEqual(state['reader_owner'],':1.77')
        request=bus.call.call_args.args[0]
        self.assertEqual(request.member,'GetState')
        self.assertEqual(request.flags,MessageFlag.NO_AUTOSTART)

    async def test_absolute_target_is_sent_to_the_same_unique_owner(self):
        reply=SimpleNamespace(message_type=MessageType.METHOD_RETURN,sender=':1.77',
            body=[json.dumps({'active':True,'page':18})])
        bus=SimpleNamespace(call=AsyncMock(return_value=reply))
        client=ReaderDBusActions();client._get_bus=AsyncMock(return_value=bus)
        state=await client._request('GotoPage','u',[18],owner=':1.77')
        self.assertEqual(state['page'],18)
        request=bus.call.call_args.args[0]
        self.assertEqual((request.destination,request.member,request.body),(':1.77','GotoPage',[18]))
        self.assertEqual(request.flags,MessageFlag.NO_AUTOSTART)

    async def test_unconfirmed_or_malformed_snapshot_is_rejected(self):
        client=ReaderDBusActions()
        for state in ({'active':False,'page':0},{'active':True,'page':True},[]):
            reply=SimpleNamespace(message_type=MessageType.METHOD_RETURN,sender=':1.77',body=[json.dumps(state)])
            client._get_bus=AsyncMock(return_value=SimpleNamespace(call=AsyncMock(return_value=reply)))
            with self.assertRaises(RuntimeError):await client._request('GetState','',[])

    async def test_missing_reader_fails_without_falling_back_to_global_keys(self):
        bus=SimpleNamespace(call=AsyncMock(return_value=SimpleNamespace(message_type=MessageType.ERROR,body=['reader closed'])))
        client=ReaderDBusActions()
        client._get_bus=AsyncMock(return_value=bus)
        with self.assertRaises(RuntimeError):await client._navigate('left')
        self.assertEqual(bus.call.await_count,1)
        request=bus.call.call_args.args[0]
        self.assertEqual(request.destination,'org.riscv.AIReader')
        self.assertEqual(request.member,'Navigate')
        self.assertEqual(request.flags,MessageFlag.NO_AUTOSTART)

    async def test_only_confirmed_reader_state_is_accepted(self):
        bus=SimpleNamespace(call=AsyncMock(return_value=SimpleNamespace(message_type=MessageType.METHOD_RETURN,body=[json.dumps({'active':True,'page':13})])))
        client=ReaderDBusActions()
        client._get_bus=AsyncMock(return_value=bus)
        self.assertEqual((await client._navigate('right'))['page'],13)
        bus.call.return_value.body=[json.dumps({'active':False,'page':0})]
        with self.assertRaises(RuntimeError):await client._navigate('right')


if __name__=='__main__':unittest.main()

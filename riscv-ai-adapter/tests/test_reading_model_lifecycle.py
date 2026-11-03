import contextlib
import dataclasses
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from riscv_ai_adapter.backends import BackendRegistry, BackendUnavailableError, ProcessManager
from riscv_ai_adapter.config import AdapterConfig, BackendConfig, ServerConfig, load_config
from riscv_ai_adapter.metrics import Metrics
from riscv_ai_adapter.reference_context import HEADER, QUESTION, page_summary_backend, reference_chat_request
from riscv_ai_adapter.backends import _SMALL_K_VOICE_FORMAT_PREFIX as PREFIX


class DemandTests(unittest.TestCase):
    def setUp(self):
        self.backend=BackendConfig('reader','chat','http://127.0.0.1:1/v1/chat/completions',
            managed=True,on_demand=True,health_url='http://127.0.0.1:1/health',
            timeout_seconds=.02,resource_group='models')
        self.events=[]
        self.manager=Mock()
        self.manager.start_one.side_effect=lambda _:self.events.append('start')
        self.manager.snapshot.return_value=[{'name':'reader','running':True}]
        self.registry=BackendRegistry(AdapterConfig(ServerConfig(),(self.backend,),{'models':1}),Metrics(),self.manager)
        self.registry.probe=Mock(return_value=(True,'ok'))
        self.reply=Mock(status=200,headers={})
        self.reply.close.side_effect=lambda:self.events.append('close')
        self.manager.stop_one.side_effect=self.stopping

    def stopping(self,_):
        self.events.append('stop')
        snapshot=self.registry.scheduler.snapshot()
        self.assertEqual(snapshot['group:models']['active'],1)

    def open(self,**kw):
        return self.registry.open(self.backend,b'{}','application/json','application/json',50,**kw)

    def test_worker_stays_owned_until_response_closed_and_lease_released(self):
        with patch('urllib.request.urlopen',return_value=self.reply):
            with self.open() as response:
                self.assertEqual(response.status,200)
                self.assertEqual(self.events,['start'])
        self.assertEqual(self.events,['start','close','stop'])
        self.assertEqual(self.registry.scheduler.snapshot()['group:models']['active'],0)

    def test_client_error_cleans_up_and_next_request_starts_fresh(self):
        with patch('urllib.request.urlopen',return_value=self.reply):
            with self.assertRaises(BrokenPipeError):
                with self.open():raise BrokenPipeError()
            with self.open():pass
        self.assertEqual(self.events,['start','close','stop']*2)

    def test_cancel_before_loading_never_spawns_a_worker(self):
        with self.assertRaises(ConnectionResetError):
            with self.open(cancelled=lambda:True):self.fail('Cancelled request was opened')
        self.manager.start_one.assert_not_called()
        self.manager.stop_one.assert_not_called()

    def test_cancel_during_loading_and_load_timeout_stop_owned_worker(self):
        cancelled=[False]
        def not_ready(_):
            cancelled[0]=True
            return False,'loading'
        self.registry.probe.side_effect=not_ready
        with self.assertRaises(ConnectionResetError):
            with self.open(cancelled=lambda:cancelled[0]):self.fail('Still loading')
        self.assertEqual(self.events,['start','stop'])
        self.events.clear();self.registry.probe.side_effect=None
        self.registry.probe.return_value=(False,'loading')
        with patch('time.sleep',return_value=None), self.assertRaises(BackendUnavailableError):
            with self.open():self.fail('Timed-out model was opened')
        self.assertEqual(self.events,['start','stop'])

    def test_ordinary_managed_workers_keep_boot_behavior(self):
        ordinary=dataclasses.replace(self.backend,name='ordinary',on_demand=False)
        manager=ProcessManager((self.backend,ordinary))
        with patch.object(manager,'start_one') as start,patch('threading.Thread'):
            manager.start()
        self.assertEqual(start.call_args_list,[unittest.mock.call('ordinary')])


class RoutingTests(unittest.TestCase):
    def setUp(self):
        self.reader=BackendConfig('reader','chat','http://localhost:1',upstream_model='reader-model')
        self.main=BackendConfig('main','chat','http://localhost:2',reading_summary_backend='reader')

    def body(self,question,kind='reading_page'):
        page={'kind':kind,'title':'Title','page':1,'text':'Supports X; jointly releases Y.'}
        return json.dumps({'messages':[{'role':'user','content':PREFIX+HEADER+json.dumps(
            {'reference_text':json.dumps(page)})+QUESTION+question}], 'stream':True}).encode()

    def test_only_current_page_summary_selects_the_configured_model(self):
        original=self.body('这一页主要讲了什么？')
        self.assertIs(page_summary_backend(original,self.main,(self.main,self.reader)),self.reader)
        for question in ('比较上一页和这一页','这一页中的术语是什么意思'):
            self.assertIsNone(page_summary_backend(self.body(question),self.main,(self.main,self.reader)))
        self.assertIsNone(page_summary_backend(self.body('总结',kind='log'),self.main,(self.main,self.reader)))
        self.assertIsNone(page_summary_backend(b'{}',self.main,(self.main,self.reader)))

    def test_narrative_is_opt_in_and_references_remain_data(self):
        original=self.body('概括这一页')
        legacy=json.loads(reference_chat_request(original,'normal-model'))
        narrative=json.loads(reference_chat_request(original,'reader-model',narrative=True))
        self.assertIn('主题',json.loads(legacy['messages'][-1]['content'])['output_requirements'])
        self.assertNotEqual(legacy['messages'][0]['content'],narrative['messages'][0]['content'])
        self.assertIn(json.loads(legacy['messages'][-1]['content'])['page_text'],
                      narrative['messages'][-1]['content'])
        self.assertTrue(narrative['stream'])
        self.assertNotIn('tools',narrative)

    def test_asr_terminal_punctuation_does_not_change_summary_input(self):
        requests=[reference_chat_request(self.body('这一页主要讲了什么'+ending),'reader-model',narrative=True)
                  for ending in ('。','？','?','！')]
        self.assertTrue(all(request==requests[0] for request in requests))
        legacy=json.loads(reference_chat_request(self.body('这一页主要讲了什么。'),'normal-model'))
        self.assertEqual(json.loads(legacy['messages'][-1]['content'])['question'],'这一页主要讲了什么。')


class ConfigTests(unittest.TestCase):
    def load(self,extra):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'test.toml'
            path.write_text('[[backends]]\nname="reader"\ncapability="chat"\nurl="http://127.0.0.1:1"\n'+extra)
            return load_config(path)

    def test_optional_backend_must_be_managed_serial_and_health_checked(self):
        valid='on_demand=true\nmanaged=true\ncommand=["/usr/bin/true"]\nhealth_url="http://127.0.0.1:1/health"\n'
        self.assertTrue(self.load(valid).backends[0].on_demand)
        for extra in (valid+'required=true\n',valid+'max_concurrency=2\n',
                      valid.replace('managed=true','managed=false'),valid.replace('on_demand=true','on_demand="yes"')):
            with self.subTest(extra=extra),self.assertRaises(ValueError):self.load(extra)

    def test_missing_reading_model_target_is_rejected(self):
        with self.assertRaises(ValueError):self.load('reading_summary_backend="missing"\n')

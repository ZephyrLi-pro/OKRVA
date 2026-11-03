import dataclasses
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from riscv_ai_adapter.backends import BackendRegistry, ProcessManager
from riscv_ai_adapter.config import AdapterConfig, BackendConfig, ServerConfig, load_config
from riscv_ai_adapter.metrics import Metrics


def backends():
    main = BackendConfig('main', 'chat', 'http://127.0.0.1:1', required=True,
                         managed=True, health_url='http://127.0.0.1:1/health',
                         command=('main',), resource_group='llm', exclusive_with=('reader',),
                         acquire_timeout_seconds=2)
    reader = dataclasses.replace(main, name='reader', required=False, on_demand=True,
                                 command=('reader',), exclusive_with=('main',))
    return main, reader


def child(pid):
    p = Mock(pid=pid)
    p.poll.return_value = None
    p.wait.side_effect = lambda **kw: setattr(p.poll, 'return_value', -15)
    return p


class ResidencyTests(unittest.TestCase):
    def test_old_model_is_reaped_before_new_model_spawns_and_not_respawned(self):
        main, reader = backends()
        speech = dataclasses.replace(main, name='asr', capability='transcription', exclusive_with=())
        manager = ProcessManager((main, reader, speech))
        old, new, restored, asr = [child(i) for i in range(4)]
        def spawn(command, **kw):
            if command == ['reader']:
                self.assertIsNotNone(old.poll())
                return new
            return old if manager._processes.get('main') is None else restored
        with patch('subprocess.Popen', side_effect=spawn):
            manager._processes['asr'] = asr
            manager._desired_running.add('asr')
            manager.start_one('main')
            manager.start_one('reader')
            self.assertTrue(manager.is_standby('main'))
            self.assertNotIn('main', manager._desired_running)
            with patch.object(manager._stop_event, 'wait', side_effect=[False, True]):
                manager._monitor_loop()
            self.assertEqual(manager.process_ids(), {'reader': 1, 'asr': 3})
            manager.start_one('main')
            self.assertIsNotNone(new.poll())
            self.assertEqual(manager.process_ids()['main'], 2)
            self.assertFalse(manager.is_standby('main'))
            asr.terminate.assert_not_called()

    def test_cancelled_request_does_not_evict_any_model(self):
        main, reader = backends()
        manager = ProcessManager((main, reader))
        registry = BackendRegistry(AdapterConfig(ServerConfig(), (main, reader), {'llm': 1}), Metrics(), manager)
        with patch.object(manager, 'start_one') as start:
            with self.assertRaises(ConnectionResetError):
                with registry.open(reader, b'{}', '', '', 50, cancelled=lambda: True):
                    self.fail('cancelled request opened')
        start.assert_not_called()

    def test_active_response_keeps_its_model_until_shared_lease_is_released(self):
        main, reader = backends()
        manager = ProcessManager((main, reader))
        registry = BackendRegistry(AdapterConfig(ServerConfig(), (main, reader), {'llm': 1}), Metrics(), manager)
        registry.probe = Mock(return_value=(True, 'ok'))
        old, new = child(1), child(2)
        entered, completed = threading.Event(), threading.Event()
        errors = []
        def next_request():
            entered.set()
            try:
                with registry.open(reader, b'{}', '', '', 50):pass
            except Exception as e:errors.append(e)
            completed.set()
        with patch('subprocess.Popen', side_effect=[old, new]), \
                patch('urllib.request.urlopen', side_effect=lambda *a, **k: Mock(status=200, headers={})):
            with registry.open(main, b'{}', '', '', 50):
                thread = threading.Thread(target=next_request)
                thread.start()
                self.assertTrue(entered.wait(1))
                self.assertFalse(completed.wait(.05))
                old.terminate.assert_not_called()
            thread.join(2)
        self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])
        old.terminate.assert_called_once()
        self.assertEqual(registry.scheduler.snapshot()['group:llm']['active'], 0)

    def test_required_standby_is_reported_without_hiding_real_worker_failure(self):
        main, reader = backends()
        manager = ProcessManager((main, reader))
        registry = BackendRegistry(AdapterConfig(ServerConfig(), (main, reader), {'llm': 1}), Metrics(), manager)
        registry.probe = Mock(return_value=(False, 'listener absent'))
        self.assertFalse(registry.readiness()[0])
        manager._standby['main'] = 'reader'
        ready, details = registry.readiness()
        self.assertTrue(ready)
        self.assertIn('standby', details[0]['detail'])
        manager._standby.clear()
        self.assertFalse(registry.readiness()[0])

    def test_eviction_invalidates_old_idle_timer_across_model_round_trip(self):
        main, reader = backends()
        reader = dataclasses.replace(reader, idle_timeout_seconds=60)
        manager = ProcessManager((main, reader))
        timers = []
        def timer(delay, callback):
            t = Mock();t.callback = callback;timers.append(t);return t
        first, middle, last = child(1), child(2), child(3)
        with patch('subprocess.Popen', side_effect=[first, middle, last]), patch('threading.Timer', side_effect=timer):
            manager.start_one('reader');manager.stop_after_idle('reader', 60)
            manager.start_one('main');manager.start_one('reader')
            timers[0].callback()
            self.assertEqual(manager.process_ids()['reader'], 3)
            last.terminate.assert_not_called()


class ConfigTests(unittest.TestCase):
    TEMPLATE = '''[resource_groups]
llm=1
[[backends]]
name="main"
capability="chat"
url="http://127.0.0.1:1"
health_url="http://127.0.0.1:1/health"
managed=true
required=true
command=["/bin/true"]
resource_group="llm"
exclusive_with=["reader"]
[[backends]]
name="reader"
capability="chat"
url="http://127.0.0.1:2"
health_url="http://127.0.0.1:2/health"
managed=true
on_demand=true
command=["/bin/true"]
resource_group="llm"
exclusive_with=["main"]
'''
    def test_configuration_requires_symmetric_owned_models_and_one_shared_slot(self):
        invalid = [self.TEMPLATE.replace('llm=1', 'llm=2'),
                   self.TEMPLATE.replace('exclusive_with=["main"]', 'exclusive_with=[]'),
                   self.TEMPLATE.replace('exclusive_with=["main"]', 'exclusive_with=["missing"]'),
                   self.TEMPLATE.replace('exclusive_with=["reader"]', 'exclusive_with=["main"]'),
                   self.TEMPLATE.replace('managed=true', 'managed=false', 1),
                   self.TEMPLATE.replace('exclusive_with=["reader"]', 'exclusive_with=["reader","reader"]')]
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'config.toml';p.write_text(self.TEMPLATE)
            self.assertEqual(load_config(p).backends[0].exclusive_with, ('reader',))
            for text in invalid:
                p.write_text(text)
                with self.subTest(text=text), self.assertRaises(ValueError):load_config(p)


if __name__ == '__main__':unittest.main()

import dataclasses
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from riscv_ai_adapter.backends import BackendRegistry, ProcessManager
from riscv_ai_adapter.config import AdapterConfig, BackendConfig, ServerConfig, load_config
from riscv_ai_adapter.metrics import Metrics


class Timer:
    def __init__(self, delay, callback):
        self.delay, self.callback, self.cancelled = delay, callback, False

    def start(self):
        pass

    def cancel(self):
        self.cancelled = True


def backend():
    return BackendConfig('reader', 'chat', 'http://127.0.0.1:1',
                         managed=True, on_demand=True, command=('/bin/true',),
                         health_url='http://127.0.0.1:1/health',
                         idle_timeout_seconds=60, resource_group='models')


def process(pid):
    child = Mock(pid=pid)
    child.poll.return_value = None
    child.wait.side_effect = lambda **kw: setattr(child.poll, 'return_value', -15)
    return child


class IdleTimerTests(unittest.TestCase):
    def setUp(self):
        self.manager = ProcessManager((backend(),))
        self.child = process(101)
        self.timers = []

    def timer(self, delay, callback):
        timer = Timer(delay, callback)
        self.timers.append(timer)
        return timer

    def test_new_request_invalidates_already_awakened_old_expiry(self):
        with patch('subprocess.Popen', return_value=self.child) as spawn, \
                patch('threading.Timer', side_effect=self.timer):
            self.manager.start_one('reader')
            self.manager.stop_after_idle('reader', 60)
            old = self.timers[-1]
            self.manager.start_one('reader')
            old.callback()  # cancel() could have raced with this callback.
            self.child.terminate.assert_not_called()
            self.assertTrue(old.cancelled)
            self.assertEqual(spawn.call_count, 1)
            self.assertNotIn('idle_unload_seconds_remaining', self.manager.snapshot()[0])
            self.manager.stop_after_idle('reader', 60)
            self.assertGreater(self.manager.snapshot()[0]['idle_unload_seconds_remaining'], 0)
            old.callback()
            self.child.terminate.assert_not_called()
            self.timers[-1].callback()
            self.child.terminate.assert_called_once()
            self.assertFalse(self.manager.snapshot()[0]['desired_running'])

    def test_expiring_old_listener_exits_before_replacement_spawns(self):
        terminating = threading.Event()
        permit_exit = threading.Event()
        started = threading.Event()
        replacement = process(202)
        self.child.terminate.side_effect = terminating.set

        def wait(**kw):
            if not permit_exit.wait(2):
                raise AssertionError('test did not release the old listener')
            self.child.poll.return_value = -15

        self.child.wait.side_effect = wait
        with patch('subprocess.Popen', side_effect=[self.child, replacement]) as spawn, \
                patch('threading.Timer', side_effect=self.timer):
            self.manager.start_one('reader')
            self.manager.stop_after_idle('reader', 60)
            expiry = threading.Thread(target=self.timers[-1].callback)
            expiry.start()
            self.assertTrue(terminating.wait(1))
            request = threading.Thread(target=lambda: (self.manager.start_one('reader'), started.set()))
            request.start()
            self.assertFalse(started.wait(.03))
            self.assertEqual(spawn.call_count, 1)
            permit_exit.set()
            expiry.join(1)
            request.join(1)
            self.assertTrue(started.is_set())
            self.assertEqual(self.manager.snapshot()[0]['pid'], 202)
            replacement.terminate.assert_not_called()

    def test_shutdown_cancels_idle_timer_and_reaps_child(self):
        with patch('subprocess.Popen', return_value=self.child), \
                patch('threading.Timer', side_effect=self.timer):
            self.manager.start_one('reader')
            self.manager.stop_after_idle('reader', 60)
            self.manager.stop()
            self.assertTrue(self.timers[-1].cancelled)
            self.timers[-1].callback()
            self.child.terminate.assert_called_once()

    def test_idle_worker_exit_clears_timer_without_automatic_reload(self):
        self.manager = ProcessManager((dataclasses.replace(backend(), restart_on_failure=False),))
        with patch('subprocess.Popen', return_value=self.child) as spawn, \
                patch('threading.Timer', side_effect=self.timer):
            self.manager.start_one('reader')
            self.manager.stop_after_idle('reader', 60)
            self.child.poll.return_value = 1  # Includes a RAM-guard exit.
            with patch.object(self.manager._stop_event, 'wait', side_effect=[False, True]):
                self.manager._monitor_loop()
            self.assertTrue(self.timers[-1].cancelled)
            self.assertEqual(spawn.call_count, 1)
            state = self.manager.snapshot()[0]
            self.assertFalse(state['desired_running'])
            self.assertNotIn('idle_unload_seconds_remaining', state)


class RegistryTests(unittest.TestCase):
    def test_only_success_keeps_worker_warm_and_releases_scheduler(self):
        for mode in ('success', 'upstream-error', 'client-disconnect'):
            with self.subTest(mode=mode):
                cfg = backend()
                manager = Mock()
                manager.snapshot.return_value = [{'name': 'reader', 'running': True}]
                registry = BackendRegistry(AdapterConfig(ServerConfig(), (cfg,), {'models': 1}),
                                           Metrics(), manager)
                registry.probe = Mock(return_value=(True, 'ok'))
                reply = Mock(status=503 if mode == 'upstream-error' else 200, headers={})
                with patch('urllib.request.urlopen', return_value=reply):
                    try:
                        with registry.open(cfg, b'{}', 'application/json', '', 50):
                            if mode == 'client-disconnect':
                                raise BrokenPipeError()
                    except BrokenPipeError:
                        pass
                reply.close.assert_called_once()
                if mode == 'success':
                    manager.stop_after_idle.assert_called_once_with('reader', 60)
                    manager.stop_one.assert_not_called()
                else:
                    manager.stop_one.assert_called_once_with('reader')
                    manager.stop_after_idle.assert_not_called()
                self.assertEqual(registry.scheduler.snapshot()['group:models']['active'], 0)


class ConfigTests(unittest.TestCase):
    def test_retention_is_finite_bounded_and_optional(self):
        template = '''[[backends]]
name="reader"
capability="chat"
url="http://127.0.0.1:1"
health_url="http://127.0.0.1:1/health"
managed=true
command=["/bin/true"]
on_demand=true
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'config.toml'
            for value in ('true', '"60"', 'nan', 'inf', '-1', '121'):
                path.write_text(template + 'idle_timeout_seconds=' + value)
                with self.subTest(value=value), self.assertRaises(ValueError):
                    load_config(path)
            path.write_text(template + 'idle_timeout_seconds=60')
            self.assertEqual(load_config(path).backends[0].idle_timeout_seconds, 60)
            path.write_text(template.replace('on_demand=true', 'on_demand=false') +
                            'idle_timeout_seconds=60')
            with self.assertRaises(ValueError):
                load_config(path)


if __name__ == '__main__':
    unittest.main()

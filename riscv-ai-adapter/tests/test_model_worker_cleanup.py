import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from riscv_ai_adapter import model_worker


class WorkerTests(unittest.TestCase):
    def test_low_startup_memory_never_launches_a_child(self):
        with patch('subprocess.Popen') as spawn,self.assertRaises(RuntimeError):
            model_worker.supervise(['/not/launched'],available=lambda:0)
        spawn.assert_not_called()

    def test_runtime_memory_guard_terminates_and_reaps_its_child(self):
        launched=[]
        real_spawn=subprocess.Popen
        def spawn(*args,**kw):
            child=real_spawn(*args,**kw);launched.append(child);return child
        with patch('subprocess.Popen',side_effect=spawn),self.assertRaises(RuntimeError):
            model_worker.supervise([sys.executable,'-c','import time;time.sleep(60)'],
                available=Mock(side_effect=[4096,256]),interval=.01)
        self.assertEqual(len(launched),1)
        self.assertIsNotNone(launched[0].poll())

    def test_stopping_parent_reaps_model_instead_of_orphaning_it(self):
        with tempfile.TemporaryDirectory() as d:
            pid_file=Path(d)/'child.pid'
            child=Path(d)/'child.py'
            child.write_text('import os,time\nfrom pathlib import Path\nPath('+repr(str(pid_file))+').write_text(str(os.getpid()))\ntime.sleep(60)\n')
            worker=subprocess.Popen([sys.executable,'-B',model_worker.__file__,
                '--startup-min-mib','2','--runtime-min-mib','1','--',sys.executable,str(child)])
            try:
                deadline=time.monotonic()+5
                while not pid_file.exists() and time.monotonic()<deadline:
                    self.assertIsNone(worker.poll());time.sleep(.02)
                self.assertTrue(pid_file.exists())
                pid=int(pid_file.read_text())
                worker.terminate();worker.wait(timeout=5)
                with self.assertRaises(ProcessLookupError):os.kill(pid,0)
            finally:
                if worker.poll() is None:
                    worker.terminate();worker.wait(timeout=5)

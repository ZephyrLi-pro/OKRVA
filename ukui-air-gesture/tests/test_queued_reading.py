import sys
import threading
import time
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'app/python'))
from queued_reading import QueuedReadingDispatcher
from test_reading_navigation import pose


class QueuedReadingTests(unittest.TestCase):
    def test_slow_page_render_does_not_block_tracking_return_or_next_stroke(self):
        started,release=threading.Event(),threading.Event()
        calls,events=[],[]
        def send(direction):
            calls.append(direction)
            if len(calls)==1:started.set();release.wait(3)
        dispatcher=QueuedReadingDispatcher(send,lambda event,**data:events.append((event,data)))
        try:
            for index,x in enumerate([.5,.42,.34]):dispatcher.process(pose(x),index*.25)
            self.assertTrue(started.wait(1))
            start=time.monotonic()
            for index,x in enumerate([.28,.4,.5,.58,.66,.72,.6,.5],start=3):dispatcher.process(pose(x),index*.25)
            self.assertLess(time.monotonic()-start,.5)
            self.assertEqual(calls,['left'])
            self.assertEqual([d['direction'] for e,d in events if e=='action_queued'],['left','right'])
            release.set()
            deadline=time.monotonic()+2
            while sum(e=='action_candidate' for e,d in events)<2 and time.monotonic()<deadline:time.sleep(.01)
            self.assertEqual(calls,['left','right'])
            self.assertEqual(sum(e=='action_candidate' for e,d in events),2)
        finally:release.set();dispatcher.close()

    def test_close_drops_pending_navigation_without_replay(self):
        started,release=threading.Event(),threading.Event()
        calls=[]
        def send(direction):calls.append(direction);started.set();release.wait(2)
        dispatcher=QueuedReadingDispatcher(send,lambda *a,**kw:None)
        dispatcher._enqueue('left')
        self.assertTrue(started.wait(1))
        dispatcher._enqueue('right')
        dispatcher.close();release.set()
        dispatcher.worker.join(1)
        self.assertEqual(calls,['left'])

    def test_full_queue_rejects_additional_intent_instead_of_building_backlog(self):
        started,release=threading.Event(),threading.Event()
        def send(direction):started.set();release.wait(2)
        dispatcher=QueuedReadingDispatcher(send,lambda *a,**kw:None)
        try:
            dispatcher._enqueue('left');self.assertTrue(started.wait(1))
            dispatcher._enqueue('right')
            with self.assertRaises(RuntimeError):dispatcher._enqueue('left')
        finally:release.set();dispatcher.close()


if __name__=='__main__':unittest.main()

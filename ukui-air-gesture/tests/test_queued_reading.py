import sys,threading,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'app/python'))
from queued_reading import QueuedReadingDispatcher


class Reader:
    def __init__(self,page=13,total=32):
        self.owner=':1.1';self.pages={self.owner:page};self.total=total;self.active=True
        self.calls=[];self.started=threading.Event();self.release=threading.Event()
        self.block_first=True;self.fail_first=False

    def state(self):
        return dict(active=self.active,page=self.pages[self.owner],total=self.total,reader_owner=self.owner)

    def goto(self,page,owner):
        self.calls.append((owner,page));first=len(self.calls)==1
        if first:
            self.started.set()
            if self.block_first and not self.release.wait(2):raise RuntimeError('test release not received')
            if self.fail_first:raise RuntimeError('ambiguous delivery failure')
        self.pages[owner]=page
        return dict(active=True,page=page,total=self.total,reader_owner=owner)


class PageTargetTests(unittest.TestCase):
    def make(self,reader=None,**kwargs):
        reader=reader or Reader();events=[]
        d=QueuedReadingDispatcher(reader,lambda event,**data:events.append(dict(event=event,**data)),**kwargs)
        self.addCleanup(d.close)
        return reader,d,events

    def idle(self,d):
        with d.condition:
            self.assertTrue(d.condition.wait_for(lambda:not d.inflight and d.target is None,timeout=2))

    def start(self,d,r,direction='right'):
        d._enqueue(direction);self.assertTrue(r.started.wait(1))

    def test_five_fast_strokes_keep_only_the_latest_target(self):
        r,d,events=self.make();self.start(d,r)
        for _ in range(4):d._enqueue('right')
        self.assertEqual(d.diagnostics()['requested_page'],18)
        r.release.set();self.idle(d)
        self.assertEqual(r.calls,[(':1.1',14),(':1.1',18)])
        self.assertEqual(r.pages[r.owner],18)
        self.assertEqual([e['page'] for e in events if e['event']=='page_navigation_confirmed'],[14,18])

    def test_boundary_clamping_preserves_the_order_of_opposite_strokes(self):
        r,d,_=self.make(Reader(page=1,total=10));self.start(d,r,'left')
        d._enqueue('left');d._enqueue('right');d._enqueue('right')
        r.release.set();self.idle(d)
        self.assertEqual(r.pages[r.owner],3)

    def test_latest_target_expires_instead_of_replaying_after_a_long_render(self):
        clock=[0.];r,d,events=self.make(max_age=2.,clock=lambda:clock[0]);self.start(d,r)
        d._enqueue('right');d._enqueue('right');clock[0]=3.
        r.release.set();self.idle(d)
        self.assertEqual(r.calls,[(':1.1',14)])
        self.assertTrue(any(e['event']=='action_error' and not e['automatic_retry'] for e in events))

    def test_delivery_failure_cancels_pending_target_without_retry(self):
        r,d,events=self.make();r.fail_first=True;self.start(d,r)
        d._enqueue('right');r.release.set();self.idle(d)
        self.assertEqual(r.calls,[(':1.1',14)])
        self.assertIsNone(d.diagnostics()['requested_page'])
        self.assertFalse(any(e['event']=='page_navigation_confirmed' for e in events))
        d._enqueue('left');self.idle(d)
        self.assertEqual(r.calls[-1],(':1.1',12))

    def test_reader_restart_discards_old_target_and_addresses_new_unique_owner(self):
        r,d,_=self.make();self.start(d,r);d._enqueue('right')
        r.owner=':1.2';r.pages[r.owner]=5;d._enqueue('right')
        r.release.set();self.idle(d)
        self.assertEqual(r.calls,[(':1.1',14),(':1.2',6)])
        self.assertEqual(d.diagnostics()['confirmed_page'],6)

    def test_old_owner_failure_does_not_cancel_new_owner_target(self):
        r,d,_=self.make();r.fail_first=True;self.start(d,r)
        r.owner=':1.2';r.pages[r.owner]=5;d._enqueue('left')
        r.release.set();self.idle(d)
        self.assertEqual(r.calls,[(':1.1',14),(':1.2',4)])

    def test_close_cancels_unsent_targets(self):
        r,d,_=self.make();self.start(d,r);d._enqueue('right')
        d.close();r.release.set();d.worker.join(1)
        self.assertFalse(d.worker.is_alive())
        self.assertEqual(r.calls,[(':1.1',14)])

    def test_desktop_cancellation_drops_unsent_target_and_waits_for_inflight(self):
        r,d,events=self.make();self.start(d,r);d._enqueue('right')
        with self.assertRaisesRegex(RuntimeError,'PPT仍在翻页'):
            d.cancel_pending(wait_timeout=.01)
        self.assertIsNone(d.diagnostics()['requested_page'])
        r.release.set();self.idle(d)
        d.cancel_pending(wait_timeout=.01)
        self.assertEqual(r.calls,[(':1.1',14)])
        self.assertFalse(any(e['event']=='page_navigation_confirmed' for e in events))

    def test_manual_page_change_is_respected_when_no_target_is_pending(self):
        r,d,_=self.make();r.block_first=False;self.start(d,r);self.idle(d)
        r.pages[r.owner]=7;d._enqueue('left');self.idle(d)
        self.assertEqual(r.calls[-1],(':1.1',6))

    def test_closed_reader_never_queues_a_page(self):
        r,d,_=self.make();r.active=False
        with self.assertRaises(RuntimeError):d._enqueue('right')
        self.assertEqual(r.calls,[])

if __name__=='__main__':unittest.main()

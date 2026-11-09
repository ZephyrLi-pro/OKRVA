"""Maintain one latest absolute page target instead of a queue of old strokes."""
import threading
import time

from reading_dispatch import ReadingDispatcher


class QueuedReadingDispatcher:
    def __init__(self,reader,emit,max_age=8.,clock=time.monotonic):
        self.reader,self.emit,self.max_age,self.clock=reader,emit,max_age,clock
        self.condition=threading.Condition()
        self.closed=False
        self.target=None
        self.owner=None
        self.epoch=0
        self.sequence=0
        self.created=0.
        self.last_direction=None
        self.inflight=False
        self.confirmed_page=None
        self.dispatcher=ReadingDispatcher(self._enqueue,self._queued_event)
        self.controller=self.dispatcher.controller
        self.worker=threading.Thread(target=self._deliver,name='reading-page-target',daemon=True)
        self.worker.start()

    def _enqueue(self,direction):
        # GetState is an in-memory snapshot; it also identifies this reader's
        # unique D-Bus owner. It does not run an Office operation or start an app.
        state=self.reader.state()
        owner=state.get('reader_owner')
        if (not state.get('active') or type(state.get('page')) is not int
                or type(state.get('total')) is not int
                or not 1<=state['page']<=state['total']
                or not isinstance(owner,str) or not owner.startswith(':')):
            raise RuntimeError('阅读器没有可确认的当前页')
        with self.condition:
            if self.closed:raise RuntimeError('Reading session is stopping')
            if owner!=self.owner:
                self.epoch+=1;self.owner=owner;self.target=None
            base=state['page'] if self.target is None else self.target
            self.target=max(1,min(state['total'],base+(1 if direction=='right' else -1)))
            self.sequence+=1;self.created=self.clock();self.last_direction=direction
            self.condition.notify_all()

    def _queued_event(self,event,**data):
        if event=='action_candidate':
            self.emit('action_target_updated',direction=data['direction'],
                      gesture_id=self.controller.event_id,**self.diagnostics())
        else:self.emit(event,**data)

    def _deliver(self):
        while True:
            with self.condition:
                self.condition.wait_for(lambda:self.closed or self.target is not None)
                if self.closed:return
                target,owner,epoch,sequence,created=(self.target,self.owner,self.epoch,
                    self.sequence,self.created)
                direction=self.last_direction
                if self.clock()-created>self.max_age:
                    self.target=None
                    self.emit('action_error',direction=direction,target_page=target,
                              error='翻页目标已过期，已取消',automatic_retry=False)
                    self.condition.notify_all();continue
                self.inflight=True
            try:
                # Address the same unique owner used to choose the target.
                # A restarted reader never receives an old session's page.
                state=self.reader.goto(target,owner)
                if (not state.get('active') or state.get('page')!=target
                        or state.get('reader_owner')!=owner):
                    raise RuntimeError('阅读器未确认目标页')
            except Exception as error:
                with self.condition:
                    self.inflight=False
                    if epoch==self.epoch:self.target=None
                    self.emit('action_error',direction=direction,target_page=target,
                              error=str(error),automatic_retry=False)
                    self.condition.notify_all()
            else:
                with self.condition:
                    self.inflight=False
                    current=epoch==self.epoch and not self.closed
                    if current:
                        self.confirmed_page=target
                        if sequence==self.sequence:self.target=None
                        self.emit('page_navigation_confirmed',page=target,target_sequence=sequence,
                                  direction=direction,delivery='reader_page_confirmed')
                    self.condition.notify_all()

    def cancel_pending(self, wait_timeout=3.):
        # An already sent UNO call cannot be recalled. Drop unsent targets,
        # then wait before hiding the desktop so a late Office response cannot
        # raise the presentation after the desktop action. Do not wait forever.
        with self.condition:
            self.epoch += 1
            self.target = None
            self.condition.notify_all()
            if not self.condition.wait_for(lambda:not self.inflight, timeout=wait_timeout):
                raise RuntimeError('PPT仍在翻页，已取消返回桌面，请等待后重新做手势')

    def reset_tracking(self):
        self.dispatcher.reset_tracking()
        self.controller = self.dispatcher.controller

    def process(self,landmarks,timestamp,aspect=.75,age=0.):
        self.dispatcher.process(landmarks,timestamp,aspect,age)
        return None

    def diagnostics(self):
        with self.condition:
            return dict(page_navigation_busy=self.inflight or self.target is not None,
                        requested_page=self.target,confirmed_page=self.confirmed_page,
                        target_sequence=self.sequence)

    def close(self):
        with self.condition:
            self.closed=True;self.target=None;self.condition.notify_all()
        self.worker.join(timeout=.2)

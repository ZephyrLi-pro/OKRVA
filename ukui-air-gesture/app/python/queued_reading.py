"""Keep recognition responsive while the reader renders a slow slide."""
import queue
import threading
import time

from reading_dispatch import ReadingDispatcher


class QueuedReadingDispatcher:
    def __init__(self,send,emit,max_age=8.):
        self.send,self.emit,self.max_age=send,emit,max_age
        self.pending=queue.Queue(maxsize=1)
        self.closed=threading.Event()
        self.dispatcher=ReadingDispatcher(self._enqueue,self._queued_event)
        self.controller=self.dispatcher.controller
        self.worker=threading.Thread(target=self._deliver,name='reading-page-actions',daemon=True)
        self.worker.start()

    def _enqueue(self,direction):
        if self.closed.is_set():raise RuntimeError('Reading session is stopping')
        try:self.pending.put_nowait((direction,time.monotonic()))
        except queue.Full:raise RuntimeError('翻页处理中，请等页面更新后再挥动')

    def _queued_event(self,event,**data):
        if event=='action_candidate':
            self.emit('action_queued',algorithm='reading',kind='horizontal',direction=data['direction'])
        else:self.emit(event,**data)

    def _deliver(self):
        while not self.closed.is_set():
            try:direction,created=self.pending.get(timeout=.1)
            except queue.Empty:continue
            try:
                if self.closed.is_set():break
                if time.monotonic()-created>self.max_age:
                    self.emit('action_error',direction=direction,error='待翻页动作已过期',automatic_retry=False)
                    continue
                self.send(direction)
                if not self.closed.is_set():
                    self.emit('action_candidate',algorithm='reading',kind='horizontal',
                              direction=direction,delivery='dbus_acknowledged')
            except Exception as error:
                self.emit('action_error',direction=direction,error=str(error),automatic_retry=False)
            finally:self.pending.task_done()

    def process(self,landmarks,timestamp,aspect=.75,age=0.):
        self.dispatcher.process(landmarks,timestamp,aspect,age)
        # Counts reflect native acknowledgements emitted by the delivery worker.
        return None

    def close(self):
        self.closed.set()
        self.worker.join(timeout=.2)

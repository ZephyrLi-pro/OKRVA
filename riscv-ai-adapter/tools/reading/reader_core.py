"""Page snapshots shared by the presentation view, gestures and voice context."""
import asyncio
import hashlib
import time
from dataclasses import dataclass

EMPTY_PAGE = '当前页没有可提取的原生文字，可能以图片为主。没有图像识别结果，请不要猜测页面内容。'


def limit_utf16(text, maximum):
    data = str(text).encode('utf-16-le', errors='replace')
    if len(data) <= maximum*2:
        return data.decode('utf-16-le')
    suffix = '…（文本已截断）'
    budget = maximum-len(suffix.encode('utf-16-le'))//2
    return data[:budget*2].decode('utf-16-le',errors='ignore')+suffix


@dataclass(frozen=True)
class Page:
    number: int
    total: int
    text: str
    mode: str = 'document'
    heading: str = ''


class ReaderBridge:
    """Only acknowledge navigation after confirming the selected UNO page.

    `office` calls are serialized by the caller's dedicated UNO worker.
    A single asyncio lock prevents an old heartbeat from publishing over a
    newer navigation snapshot. Voice unavailability does not block reading.
    """
    def __init__(self, office, voice, title, emit, clock=time.monotonic):
        self.office, self.voice, self.emit = office, voice, emit
        self.title = limit_utf16(title,160)
        self.clock = clock
        self.lock = asyncio.Lock()
        self.page = None
        self.active = False
        self.version = 0
        self.published_version = -1
        self.published_at = None
        self.publish_error = None

    async def _publish(self, force=False):
        if not force and self.published_version == self.version and self.published_at is not None and self.clock()-self.published_at<10:
            return
        text = limit_utf16(self.page.text.strip() or EMPTY_PAGE,2000) if self.active else ''
        try:
            page_title = limit_utf16(self.page.heading or self.title,160) if self.active else ''
            accepted = await self.voice(page_title,text,
                                        self.page.number if self.active else 0)
            if not accepted:
                raise RuntimeError('Voice rejected page snapshot')
        except Exception as error:
            self.publish_error = str(error)
            self.emit('context_publish_failed', error=self.publish_error, version=self.version)
            return
        self.published_at = self.clock()
        self.published_version = self.version
        self.publish_error = None
        self.emit('context_published', page=self.page.number if self.active else 0,
                  version=self.version, text_utf16=len(text.encode('utf-16-le'))//2)

    async def refresh(self):
        async with self.lock:
            page = await self.office('current')
            self._select(page)
            await self._publish()
            return self.state()

    def _select(self, page):
        if page is not None and not (1<=page.number<=page.total):
            raise ValueError('Invalid selected page from office')
        if page != self.page:
            self.page = page
            self.active = page is not None
            self.version += 1
            self.emit('page_changed', page=page.number if page else 0,
                      total=page.total if page else 0, version=self.version)

    async def navigate(self, direction):
        if direction not in ('left','right'):
            raise ValueError('Direction must be left or right')
        async with self.lock:
            # The reader, not an OS key, chooses the document and slide.
            page = await self.office('navigate',direction)
            self._select(page)
            await self._publish()
            if not self.active:
                raise RuntimeError('Reading window is closed')
            return self.state()

    async def goto(self, number):
        async with self.lock:
            page = await self.office('goto',number)
            self._select(page)
            await self._publish()
            if not self.active:
                raise RuntimeError('Reading window is closed')
            return self.state()

    async def clear(self):
        async with self.lock:
            self._select(None)
            await self._publish(force=True)

    def state(self):
        text = self.page.text if self.active else ''
        return dict(active=self.active, title=self.title if self.active else '',
            page_title=self.page.heading if self.active else '',
            page=self.page.number if self.active else 0,
            total=self.page.total if self.active else 0,
            mode=self.page.mode if self.active else 'closed',
            text=limit_utf16(text,2000), text_chars=len(text),
            text_sha256=hashlib.sha256(text.encode()).hexdigest() if self.active else None,
            version=self.version, context_published=self.published_version==self.version and self.publish_error is None,
            publish_error=self.publish_error)

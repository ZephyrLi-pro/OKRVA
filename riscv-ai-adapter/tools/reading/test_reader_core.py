import asyncio
import unittest
from reader_core import ReaderBridge,Page,EMPTY_PAGE,limit_utf16
from office_reader import page_heading


class ReaderTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.number=1
        self.pages=['first page','second page','third page']
        self.closed=False
        self.calls=[]
        self.accept=True
        self.now=0.
        async def office(action,*args):
            if self.closed:return None
            if action=='navigate':self.number=max(1,min(3,self.number+(1 if args[0]=='right' else -1)))
            if action=='goto':
                if not 1<=args[0]<=3:raise ValueError('out of range')
                self.number=args[0]
            return Page(self.number,3,self.pages[self.number-1])
        async def voice(title,text,page):
            self.calls.append((title,text,page))
            return self.accept
        self.bridge=ReaderBridge(office,voice,'Test presentation',lambda *args,**kw:None,lambda:self.now)

    async def test_navigation_and_returned_page_text_are_consistent(self):
        await self.bridge.refresh()
        result=await self.bridge.navigate('right')
        self.assertEqual((result['page'],result['text']),(2,'second page'))
        self.assertEqual(self.calls[-1],('Test presentation','second page',2))
        self.assertTrue(result['context_published'])
        result=await self.bridge.navigate('left')
        self.assertEqual(result['page'],1)

    async def test_voice_receives_selected_page_heading_instead_of_generic_file_name(self):
        async def office(action,*args):
            return Page(1,3,'Native page heading\nActual page body','slideshow','Native page heading')
        self.bridge.office=office
        result=await self.bridge.refresh()
        self.assertEqual(self.calls[-1][0],'Native page heading')
        self.assertEqual(result['title'],'Test presentation')
        self.assertEqual(result['page_title'],'Native page heading')

    async def test_boundaries_never_wrap_to_other_pages(self):
        self.assertEqual((await self.bridge.navigate('left'))['page'],1)
        await self.bridge.goto(3)
        self.assertEqual((await self.bridge.navigate('right'))['page'],3)

    async def test_invalid_direction_does_not_change_selection(self):
        with self.assertRaises(ValueError):await self.bridge.navigate('down')
        self.assertEqual(self.number,1)
        self.assertEqual(self.calls,[])

    async def test_manual_page_change_publishes_actual_page(self):
        await self.bridge.refresh()
        self.number=3
        await self.bridge.refresh()
        self.assertEqual(self.calls[-1][1:],('third page',3))

    async def test_close_clears_page_and_voice_context(self):
        await self.bridge.goto(2)
        self.closed=True
        state=await self.bridge.refresh()
        self.assertFalse(state['active'])
        self.assertEqual(state['text'],'')
        self.assertEqual(self.calls[-1],('','',0))
        with self.assertRaises(RuntimeError):await self.bridge.navigate('right')

    async def test_failed_publish_is_retried_without_claiming_success(self):
        self.accept=False
        state=await self.bridge.refresh()
        self.assertFalse(state['context_published'])
        self.accept=True
        state=await self.bridge.refresh()
        self.assertTrue(state['context_published'])
        self.assertEqual(len(self.calls),2)

    async def test_context_heartbeat_refreshes_same_page_without_version_change(self):
        state=await self.bridge.refresh()
        self.now=9.
        await self.bridge.refresh()
        self.assertEqual(len(self.calls),1)
        self.now=11.
        result=await self.bridge.refresh()
        self.assertEqual(len(self.calls),2)
        self.assertEqual(result['version'],state['version'])

    async def test_empty_page_cannot_reuse_previous_text(self):
        await self.bridge.refresh()
        self.pages[1]=''
        state=await self.bridge.navigate('right')
        self.assertEqual(state['text'],'')
        self.assertEqual(self.calls[-1][1],EMPTY_PAGE)
        self.assertNotIn('first page',self.calls[-1][1])

    async def test_old_heartbeat_cannot_overwrite_new_navigation(self):
        started,release=asyncio.Event(),asyncio.Event()
        old_office=self.bridge.office
        async def slow(action,*args):
            if action=='current' and not started.is_set():
                started.set();await release.wait()
            return await old_office(action,*args)
        self.bridge.office=slow
        refresh=asyncio.create_task(self.bridge.refresh())
        await started.wait()
        navigate=asyncio.create_task(self.bridge.navigate('right'))
        release.set()
        await asyncio.gather(refresh,navigate)
        self.assertEqual(self.calls[-1][1:],('second page',2))

    async def test_large_text_uses_qt_utf16_budget(self):
        self.pages[0]='😀汉'*1800
        await self.bridge.refresh()
        text=self.calls[-1][1]
        self.assertLessEqual(len(text.encode('utf-16-le'))//2,2000)
        self.assertTrue(text.endswith('…（文本已截断）'))


class TextBoundsTests(unittest.TestCase):
    def test_page_number_is_not_used_as_the_heading(self):
        self.assertEqual(page_heading('2\nA real page title\nBody'),'A real page title')
        self.assertEqual(page_heading('13 / 32\nA real page title'),'A real page title')

    def test_short_unicode_is_unchanged(self):
        self.assertEqual(limit_utf16('当前页😀',160),'当前页😀')

    def test_truncation_never_splits_surrogate_pair(self):
        text=limit_utf16('😀'*1500,2000)
        self.assertLessEqual(len(text.encode('utf-16-le'))//2,2000)
        self.assertNotIn('\ufffd',text)


if __name__=='__main__':unittest.main()

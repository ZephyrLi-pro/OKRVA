"""A restored caption is trusted only after an exact bounded native probe."""
import asyncio
import os
from pathlib import Path
import subprocess
import types
import unittest
from unittest.mock import Mock, patch

from office_reader import OfficeReader, exact_window_visibility
from reader_core import Page, ReaderBridge

TITLE='managed [v1]*.pptx — LibreOffice Impress'


def result(code=0, output='', error=''):
    return subprocess.CompletedProcess([],code,output,error)


class ExactVisibilityTests(unittest.TestCase):
    def probe(self, outcomes):
        with patch.dict(os.environ, {'WAYLAND_DISPLAY':'wayland-0'}), \
                patch('office_reader.Path.is_file',return_value=True), \
                patch('office_reader.subprocess.run',side_effect=outcomes) as run:
            return exact_window_visibility(TITLE,10.),run

    def test_exact_unminimized_caption_is_read_without_focus_or_restore(self):
        state,run=self.probe([result(output='libreoffice-impress: '+TITLE+'\n'),result()])
        self.assertEqual(state,dict(presentation_visible=True,
            visibility_source='wayland_exact_window',visibility_checked_at=10.))
        self.assertEqual([call.args[0][2] for call in run.call_args_list],['list','find'])
        for call in run.call_args_list:
            self.assertIn('title:'+TITLE,call.args[0])
            self.assertIn('app_id:libreoffice-impress',call.args[0])
            self.assertLessEqual(call.kwargs['timeout'],.5)

    def test_other_visible_impress_does_not_make_managed_window_visible(self):
        state,run=self.probe([result(output='libreoffice-impress: '+TITLE+'\n'),result(1)])
        self.assertIs(state['presentation_visible'],False)
        self.assertEqual(state['visibility_source'],'wayland_exact_window')
        self.assertEqual(run.call_args.args[0][-1],'state:unminimized')

    def test_overmatching_or_duplicate_caption_is_unknown(self):
        for output in ('libreoffice-impress: other.pptx\n',
                       ('libreoffice-impress: '+TITLE+'\n')*2,''):
            with self.subTest(output=output):
                state,run=self.probe([result(output=output)])
                self.assertIs(state['presentation_visible'],False)
                self.assertEqual(state['visibility_source'],'unknown')
                self.assertEqual(run.call_count,1)

    def test_probe_failure_is_unknown_instead_of_reusing_visible(self):
        for failure in (result(2,error='missing protocol'),
                        subprocess.TimeoutExpired('wlrctl',.5),OSError('not executable')):
            state,_=self.probe([failure])
            self.assertIs(state['presentation_visible'],False)
            self.assertEqual(state['visibility_source'],'unknown')
            self.assertEqual(state['visibility_checked_at'],0.)


class VisibilityCacheTests(unittest.TestCase):
    def setUp(self):
        self.reader=OfficeReader(None,Path('/tmp/unused-profile'),lambda *args,**kwargs:None)
        self.reader.path=Path('/tmp/managed.pptx')
        self.reader.doc=Mock()
        self.reader.doc.getCurrentController.return_value.getFrame.return_value.getContainerWindow.return_value.getAccessibleContext.return_value.getAccessibleName.return_value=TITLE
        self.now=10.

    def test_bounded_cache_refreshes_both_hidden_and_restored_without_new_session(self):
        calls=[]
        visible=[True]
        def probe(title,checked):
            calls.append((title,checked))
            return dict(presentation_visible=visible[0],visibility_source='wayland_exact_window',
                        visibility_checked_at=checked)
        identity=self.reader.session_id
        with patch('office_reader.time.monotonic',side_effect=lambda:self.now), \
                patch('office_reader.exact_window_visibility',side_effect=probe):
            first=self.reader.metadata()
            self.now=10.2;visible[0]=False
            self.assertEqual(self.reader.metadata()['visibility_checked_at'],10.)
            self.now=10.6
            hidden=self.reader.metadata()
            self.assertIs(hidden['presentation_visible'],False)
            self.now=11.2;visible[0]=True
            restored=self.reader.metadata()
        self.assertIs(first['presentation_visible'],True)
        self.assertIs(restored['presentation_visible'],True)
        self.assertEqual(restored['visibility_checked_at'],11.2)
        self.assertEqual(len(calls),3)
        self.assertEqual(restored['session_id'],identity)

    def test_volatile_visibility_is_omitted_from_snapshot_identity_and_lazy_is_unknown(self):
        with patch('office_reader.exact_window_visibility') as probe:
            identity=self.reader.metadata(include_visibility=False)
            self.assertEqual(set(identity),{'session_id','source_path'})
            self.reader.doc=None
            lazy=self.reader.metadata()
        probe.assert_not_called()
        self.assertIs(lazy['presentation_visible'],False)
        self.assertEqual(lazy['visibility_source'],'unknown')

    def test_changed_caption_or_session_cannot_reuse_prior_positive_probe(self):
        with patch('office_reader.time.monotonic',return_value=10.), \
                patch('office_reader.exact_window_visibility',return_value=dict(
                    presentation_visible=False,visibility_source='unknown',visibility_checked_at=0.)) as probe:
            self.reader.metadata()
            self.reader.session_id='another-session'
            self.reader.metadata()
            self.reader.doc.getCurrentController.return_value.getFrame.return_value.getContainerWindow.return_value.getAccessibleContext.return_value.getAccessibleName.return_value='other title'
            self.reader.metadata()
        self.assertEqual(probe.call_count,3)


class BridgeVisibilityTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.identity=dict(session_id='same-session',source_path='/tmp/managed.pptx')
        self.office_calls=[];self.voice_calls=[]
        async def office(action,*args):
            self.office_calls.append(action)
            if action=='snapshot':return dict(self.identity)
            return Page(1,1,'Page text')
        async def voice(*args):
            self.voice_calls.append(args)
            return True
        self.bridge=ReaderBridge(office,voice,'Managed',lambda *args,**kwargs:None,clock=lambda:10.)
        self.bridge.document=dict(self.identity,presentation_visible=True,
            visibility_source='wayland_exact_window',visibility_checked_at=10.)
        await self.bridge.refresh()

    async def test_get_state_uses_cached_stamp_and_visibility_does_not_publish_new_page(self):
        old=self.bridge.state()
        calls=list(self.office_calls)
        self.assertEqual(self.bridge.state()['visibility_checked_at'],10.)
        self.assertEqual(self.office_calls,calls)
        self.bridge.document.update(presentation_visible=False,visibility_checked_at=10.6)
        new=await self.bridge.refresh()
        self.assertEqual(new['version'],old['version'])
        self.assertEqual(len(self.voice_calls),1)
        self.assertIs(new['presentation_visible'],False)
        self.assertEqual(new['visibility_checked_at'],10.6)

    async def test_snapshot_preserves_cache_only_for_same_identity(self):
        await self.bridge.snapshot()
        self.assertIs(self.bridge.state()['presentation_visible'],True)
        self.identity['session_id']='new-session'
        await self.bridge.snapshot()
        self.assertIs(self.bridge.state()['presentation_visible'],False)
        self.assertEqual(self.bridge.state()['visibility_source'],'unknown')

    async def test_closed_state_never_advertises_cached_visible(self):
        await self.bridge.clear()
        state=self.bridge.state()
        self.assertIs(state['active'],False)
        self.assertIs(state['presentation_visible'],False)
        self.assertEqual(state['visibility_checked_at'],0.)


if __name__=='__main__':unittest.main()

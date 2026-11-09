"""Native Impress transaction boundaries, independent of a running UI."""
import hashlib
import itertools
from pathlib import Path
import tempfile
import types
import unittest
from unittest.mock import Mock, patch

from office_reader import OfficeReader
from reader_core import Page


class Shape:
    def __init__(self, text, notes=False):
        self.text, self.notes = text, notes
        self.getText = Mock()

    def getString(self):
        return self.text

    def setString(self, text):
        self.text = text

    def getShapeType(self):
        return 'com.sun.star.presentation.NotesShape' if self.notes else 'com.sun.star.drawing.TextShape'


class NotesPage:
    def __init__(self, text):
        self.body = Shape(text, notes=True)

    def getCount(self):
        return 1

    def getByIndex(self, index):
        assert index == 0
        return self.body


class Slide:
    def __init__(self, name, text):
        self.Name = name
        self.body = Shape(text)
        self.notes = NotesPage('用户原有备注。')

    def getCount(self):
        return 1

    def getByIndex(self, index):
        assert index == 0
        return self.body

    def getNotesPage(self):
        return self.notes


class Pages:
    def __init__(self, slides):
        self.slides = slides

    def getCount(self):
        return len(self.slides)

    def getByIndex(self, index):
        return self.slides[index]


class Frame:
    def __init__(self):
        self.window = Mock()

    def activate(self):
        pass

    def getContainerWindow(self):
        return self.window


class Document:
    def __init__(self, path, slides):
        self.path, self.slides = path, slides
        self.store = Mock(side_effect=self._store)
        self.controller = Mock()
        self.controller.getFrame.return_value = Frame()
        self.controller.getCurrentPage.return_value = slides[0]

    def isReadonly(self):
        return False

    def getCurrentController(self):
        return self.controller

    def _store(self):
        # Only emulate the disk boundary. OOXML parsing has separate read-only
        # tests; no second presentation writer is embedded in these tests.
        self.path.write_bytes(b'native-Impress-save-completed')


class NativeSpeakerNotesTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'presentation.pptx'
        self.path.write_bytes(b'file-as-initially-opened')
        self.reader = OfficeReader(self.path, Path(self.tmp.name), lambda *args, **kwargs: None)
        self.slides = [Slide('pageA', '同一段页面正文。'),
                       Slide('pageB', '同一段页面正文。'),
                       Slide('pageC', '保持当前选中的第三页。')]
        self.doc = Document(self.path, self.slides)
        self.reader.doc = self.doc
        self.reader.pages = Pages(self.slides)
        self.reader.names = [slide.Name for slide in self.slides]
        self.reader.text = [slide.body.text for slide in self.slides]
        self.reader.presentation = Mock()
        self.reader.presentation.isRunning.return_value = False
        self.reader.digest = hashlib.sha256(self.path.read_bytes()).hexdigest()

    def save(self, snapshot):
        # The real save method must request native store and verify its result.
        with patch('office_reader.read_notes', side_effect=lambda path, number:
                   self.slides[number - 1].notes.body.getString()), \
                patch.object(self.reader, 'show_speaker_notes', return_value=dict(
                    notes_visible=False, display_mode='normal',
                    notes_pane_supported=False, normal_view_visible=True)), \
                patch.object(self.reader, '_prepare_notes_edit'), \
                patch.object(self.reader, '_store_without_backup', side_effect=self.doc.store):
            return self.reader.save_speaker_notes(snapshot['session_id'], snapshot['page'],
                snapshot['text_sha256'], '本页演讲备注说明当前页面的主题与具体内容。')

    def test_user_ctrl_s_before_snapshot_does_not_make_save_stale(self):
        # A normal UI save happens after bridge open, before this new request.
        # Its transaction baseline must be the snapshot, not bridge startup.
        self.path.write_bytes(b'user-edited-and-saved-with-Ctrl-S')
        snapshot = self.reader.snapshot()
        result = self.save(snapshot)
        self.assertTrue(result['ok'])
        self.assertTrue(result['verified'])
        self.assertFalse(result['notes_visible'])
        self.assertEqual(result['display_mode'], 'normal')
        self.assertFalse(result['notes_pane_supported'])
        self.assertTrue(result['normal_view_visible'])
        self.doc.store.assert_called_once_with()
        self.assertTrue(result['notes'].startswith('用户原有备注。'))

    def test_reordered_equal_text_slides_are_not_the_same_save_target(self):
        snapshot = self.reader.snapshot()
        original = self.slides[0]
        other = self.slides[1]
        # Stay on C, so page-count/current-name checks alone cannot detect A/B
        # being swapped. Both A and B also have the exact same text hash.
        self.doc.controller.getCurrentPage.return_value = self.slides[2]
        self.reader.current()
        self.slides[:2] = [other, original]
        with self.assertRaises(ValueError):
            self.save(snapshot)
        self.doc.store.assert_not_called()
        self.assertEqual(original.notes.body.text, '用户原有备注。')
        self.assertEqual(other.notes.body.text, '用户原有备注。')

    def assert_backup_disabled_during_store(self, fail=False):
        # Changes take effect only after commitChanges, like the remote UNO
        # configuration node. Both the observed store flag and final profile
        # preference matter; checking setter call names alone is insufficient.
        state = {'pending': True, 'committed': True, 'commits': []}
        def set_property(name, value):
            self.assertEqual(name, 'CreateBackup')
            state['pending'] = value
        def commit():
            state['committed'] = state['pending']
            state['commits'].append(state['committed'])
        options = types.SimpleNamespace(
            getPropertyValue=Mock(return_value=True),
            setPropertyValue=Mock(side_effect=set_property),
            commitChanges=Mock(side_effect=commit))
        def access(service, arguments):
            self.assertEqual(service, 'com.sun.star.configuration.ConfigurationUpdateAccess')
            self.assertEqual(arguments[0].Name, 'nodepath')
            self.assertEqual(arguments[0].Value, '/org.openoffice.Office.Common/Save/Document')
            return options
        provider = types.SimpleNamespace(createInstanceWithArguments=Mock(side_effect=access))
        manager = types.SimpleNamespace(createInstanceWithContext=Mock(return_value=provider))
        self.reader.remote_context = types.SimpleNamespace(ServiceManager=manager)
        uno = types.SimpleNamespace(createUnoStruct=lambda kind: types.SimpleNamespace())
        observed = []
        def store():
            observed.append(state['committed'])
            if fail:
                raise OSError('native store failed')
        self.doc.store.side_effect = store
        with patch.dict('sys.modules', {'uno': uno}):
            if fail:
                with self.assertRaisesRegex(OSError, 'native store failed'):
                    self.reader._store_without_backup()
            else:
                self.reader._store_without_backup()
        self.doc.store.assert_called_once_with()
        self.assertEqual(observed, [False])
        self.assertEqual(state['commits'], [False, True])
        self.assertTrue(state['committed'])
        manager.createInstanceWithContext.assert_called_once_with(
            'com.sun.star.configuration.ConfigurationProvider', self.reader.remote_context)

    def test_native_store_disables_backup_and_restores_profile_on_success(self):
        self.assert_backup_disabled_during_store()

    def test_native_store_exception_still_restores_original_backup_setting(self):
        self.assert_backup_disabled_during_store(fail=True)

    def show_notes_with_page(self, page, *, pane_supported=False,
                             pane_initial=False, pane_opens=True, window_visible=True):
        dispatcher = Mock()
        context = types.SimpleNamespace(ServiceManager=types.SimpleNamespace(
            createInstanceWithContext=lambda *args: dispatcher))
        uno = types.SimpleNamespace(getComponentContext=lambda: context,
            createUnoStruct=lambda kind: types.SimpleNamespace())
        # A normal dispatch return is not proof that the pane was opened;
        # only a confirmed boolean status is evidence of visibility.
        pane_state = {'visible': pane_initial}
        pane_dispatch = Mock() if pane_supported else None
        pane_url = types.SimpleNamespace(Complete='.uno:BottomPaneImpress')
        def dispatch(frame, command, target, flags, arguments):
            if command == '.uno:BottomPaneImpress':
                properties = {item.Name: item.Value for item in arguments}
                self.assertIs(properties.get('BottomPaneImpress'), True)
                if pane_opens:
                    pane_state['visible'] = True
        dispatcher.executeDispatch.side_effect = dispatch
        window = self.doc.controller.getFrame().getContainerWindow()
        window.isVisible.return_value = window_visible
        ticks = itertools.count(0, .4)
        with patch.dict('sys.modules', {'uno': uno}), \
                patch('office_reader.time.monotonic', side_effect=lambda: next(ticks)), \
                patch('office_reader.time.sleep'), \
                patch.object(self.reader, 'current', return_value=page), \
                patch.object(self.reader, '_notes_pane_dispatch', return_value=(pane_dispatch, pane_url)), \
                patch.object(self.reader, '_notes_pane_visible', side_effect=lambda *args: pane_state['visible']):
            result = self.reader.show_speaker_notes(1)
        commands = [call.args[1] for call in dispatcher.executeDispatch.call_args_list]
        self.assertIn('.uno:NormalMultiPaneGUI', commands)
        self.assertNotIn('.uno:NotesMode', commands)
        self.doc.controller.setCurrentPage.assert_called_with(self.slides[0])
        return result, dispatcher, pane_dispatch

    def test_legacy_version_preserves_large_normal_view_without_claiming_notes_pane(self):
        # The 24.2 fallback saves notes while retaining the large normal slide.
        # It must not replace that layout with a small-slide NotesMode page.
        result, _, pane_dispatch = self.show_notes_with_page(Page(1, 3, '正文', mode='document'))
        self.assertIsNone(pane_dispatch)
        self.assertFalse(result['notes_visible'])
        self.assertEqual(result['display_mode'], 'normal')
        self.assertFalse(result['notes_pane_supported'])
        self.assertTrue(result['normal_view_visible'])

    def test_modern_notes_pane_is_opened_and_confirmed_in_normal_view(self):
        result, dispatcher, _ = self.show_notes_with_page(
            Page(1, 3, '正文', mode='document'), pane_supported=True)
        self.assertEqual(sum(call.args[1] == '.uno:BottomPaneImpress'
                             for call in dispatcher.executeDispatch.call_args_list), 1)
        self.assertTrue(result['notes_visible'])
        self.assertEqual(result['display_mode'], 'normal')
        self.assertTrue(result['notes_pane_supported'])
        self.assertTrue(result['normal_view_visible'])

    def test_already_visible_modern_pane_is_not_toggled_closed(self):
        result, dispatcher, _ = self.show_notes_with_page(
            Page(1, 3, '正文', mode='document'), pane_supported=True, pane_initial=True)
        self.assertFalse(any(call.args[1] == '.uno:BottomPaneImpress'
                             for call in dispatcher.executeDispatch.call_args_list))
        self.assertTrue(result['notes_visible'])

    def test_ineffective_pane_dispatch_does_not_report_visible(self):
        with self.assertRaises(RuntimeError):
            self.show_notes_with_page(Page(1, 3, '正文', mode='document'),
                                      pane_supported=True, pane_opens=False)

    def test_wrong_slide_cannot_report_normal_view_visible(self):
        for supported in (False, True):
            with self.subTest(pane_supported=supported), self.assertRaises(RuntimeError):
                self.show_notes_with_page(Page(2, 3, '正文', mode='document'),
                                          pane_supported=supported)

    def test_hidden_native_window_cannot_report_visible(self):
        with self.assertRaises(RuntimeError):
            self.show_notes_with_page(Page(1, 3, '正文', mode='document'), window_visible=False)

    def test_unchanged_notes_mode_cannot_report_large_normal_view_visible(self):
        with self.assertRaises(RuntimeError):
            self.show_notes_with_page(Page(1, 3, '正文', mode='notes'))


if __name__ == '__main__':
    unittest.main()

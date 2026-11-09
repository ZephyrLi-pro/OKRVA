import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from office_reader import OfficeReader,recent_source


class LazyStartTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.file=self.root/'user-presentation.pptx'
        self.file.write_bytes(b'file-exists-no-need-to-open-it')
        self.state=self.root/'state.json'
        self.state.write_text(json.dumps(dict(session_id='a'*32,source_path=str(self.file))))
        self.state.chmod(0o600)

    def test_idle_controller_needs_no_file_and_never_starts_an_application(self):
        with patch('office_reader.subprocess.Popen') as start:
            reader=OfficeReader(None,self.root,lambda *a,**k:None)
            self.assertIsNone(reader.current())
            self.assertEqual(reader.metadata()['source_path'],'')
        start.assert_not_called()

    def test_recent_file_is_context_only_without_opening_the_file(self):
        with patch('office_reader.subprocess.Popen') as start:
            saved=recent_source(self.state)
            reader=OfficeReader(saved,self.root,lambda *a,**k:None)
            self.assertIsNone(reader.current())
            self.assertEqual(reader.metadata()['source_path'],str(self.file))
        start.assert_not_called()

    def test_untrusted_or_missing_history_is_not_restored(self):
        self.state.chmod(0o644)
        self.assertIsNone(recent_source(self.state))
        self.state.chmod(0o600)
        self.state.write_text(json.dumps(dict(session_id='invalid',source_path=str(self.file))))
        self.assertIsNone(recent_source(self.state))
        self.state.write_text(json.dumps(dict(session_id='a'*32,source_path=str(self.file))))
        self.file.unlink()
        self.assertIsNone(recent_source(self.state))

    def test_symlinked_history_cannot_supply_document_context(self):
        other=self.root/'another.json';other.write_bytes(self.state.read_bytes());other.chmod(0o600)
        self.state.unlink();self.state.symlink_to(other)
        self.assertIsNone(recent_source(self.state))


if __name__=='__main__':unittest.main()

from pathlib import Path
import tempfile
import unittest
import zipfile

from speaker_notes import read_notes


P = 'http://schemas.openxmlformats.org/presentationml/2006/main'
A = 'http://schemas.openxmlformats.org/drawingml/2006/main'
R = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
REL = 'http://schemas.openxmlformats.org/package/2006/relationships'


def notes_xml(body, include_body=True):
    # A notes page also contains slide images, dates, page numbers and footers.
    # Only the body placeholder holds the presenter's actual speaker notes.
    shapes = ''.join(
        f'<p:sp><p:nvSpPr><p:nvPr><p:ph type="{kind}"/></p:nvPr></p:nvSpPr>'
        f'<p:txBody><a:p><a:r><a:t>{text}</a:t></a:r></a:p></p:txBody></p:sp>'
        for kind, text in [('sldImg', '页面图片标题'), ('ftr', '机密页脚'), ('sldNum', '42'), ('dt', '2026-11-06')])
    if include_body:
        shapes += ('<p:sp><p:nvSpPr><p:nvPr><p:ph type="body"/></p:nvPr></p:nvSpPr>'
                   '<p:txBody>' + body + '</p:txBody></p:sp>')
    return f'<p:notes xmlns:p="{P}" xmlns:a="{A}"><p:cSld><p:spTree>{shapes}</p:spTree></p:cSld></p:notes>'


class SpeakerNotesTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'native-saved.pptx'

    def make(self, with_notes=True, body=None, include_body=True, overrides=None):
        # Logical page 1 deliberately maps to slide2.xml and notesSlide7.xml.
        # A filename sort or numeric notes filename guess would read the wrong page.
        parts = {
            'ppt/presentation.xml': f'<p:presentation xmlns:p="{P}" xmlns:r="{R}"><p:sldIdLst><p:sldId id="256" r:id="rId2"/><p:sldId id="257" r:id="rId1"/></p:sldIdLst></p:presentation>',
            'ppt/_rels/presentation.xml.rels': f'<Relationships xmlns="{REL}"><Relationship Id="rId1" Type="{R}/slide" Target="slides/slide1.xml"/><Relationship Id="rId2" Type="{R}/slide" Target="slides/slide2.xml"/></Relationships>',
            'ppt/slides/slide1.xml': f'<p:sld xmlns:p="{P}"/>',
            'ppt/slides/slide2.xml': f'<p:sld xmlns:p="{P}"/>',
        }
        if with_notes:
            parts['ppt/slides/_rels/slide2.xml.rels'] = f'<Relationships xmlns="{REL}"><Relationship Id="rId7" Type="{R}/notesSlide" Target="../notesSlides/notesSlide7.xml"/></Relationships>'
            parts['ppt/notesSlides/notesSlide7.xml'] = notes_xml(
                body or '<a:p><a:r><a:t>实际第一页备注。</a:t></a:r></a:p>', include_body)
        parts.update(overrides or {})
        with zipfile.ZipFile(self.path, 'w') as package:
            for name, data in parts.items():
                package.writestr(name, data)

    def test_follows_presentation_slide_order_and_relationships(self):
        self.make()
        self.assertEqual(read_notes(self.path, 1), '实际第一页备注。')
        self.assertEqual(read_notes(self.path, 2), '')

    def test_reads_only_note_body_not_footer_date_slide_image_or_number(self):
        self.make()
        self.assertEqual(read_notes(self.path, 1), '实际第一页备注。')
        original = self.path.read_bytes()
        read_notes(self.path, 1)
        self.assertEqual(self.path.read_bytes(), original)
        self.assertEqual([p.name for p in self.path.parent.iterdir()], ['native-saved.pptx'])

    def test_preserves_runs_soft_breaks_and_paragraph_breaks(self):
        self.make(body='<a:p><a:r><a:t>第一</a:t></a:r><a:r><a:t>句。</a:t></a:r>'
                       '<a:br/><a:r><a:t>换行说明。</a:t></a:r></a:p>'
                       '<a:p><a:r><a:t>第二段。</a:t></a:r></a:p>')
        self.assertEqual(read_notes(self.path, 1), '第一句。\n换行说明。\n第二段。')

    def test_absent_notes_or_body_returns_empty(self):
        self.make(with_notes=False)
        self.assertEqual(read_notes(self.path, 1), '')
        self.make(include_body=False)
        self.assertEqual(read_notes(self.path, 1), '')
        self.make(overrides={'ppt/slides/_rels/slide2.xml.rels': f'<Relationships xmlns="{REL}"/>'})
        self.assertEqual(read_notes(self.path, 1), '')

    def test_out_of_range_and_invalid_page_numbers_are_rejected(self):
        self.make()
        for page in (0, -1, 3, True, 1.0, '1'):
            with self.subTest(page=page), self.assertRaises(ValueError):
                read_notes(self.path, page)

    def test_external_and_package_escaping_notes_targets_are_rejected(self):
        for target, mode in [('https://example.org/notes.xml', 'External'),
                             ('file:///tmp/notes.xml', 'External'),
                             ('../../../outside.xml', ''), ('/etc/passwd', ''),
                             ('https://example.org/notes.xml', ''), ('..\\notes.xml', '')]:
            with self.subTest(target=target):
                external = ' TargetMode="' + mode + '"' if mode else ''
                rels = (f'<Relationships xmlns="{REL}"><Relationship Id="rId7" Type="{R}/notesSlide"'
                        f' Target="{target}"{external}/></Relationships>')
                self.make(overrides={'ppt/slides/_rels/slide2.xml.rels': rels})
                with self.assertRaises(ValueError):
                    read_notes(self.path, 1)

    def test_external_or_package_escaping_slide_relationship_is_rejected(self):
        for target, mode in [('https://example.org/slide.xml', 'External'), ('../../slide.xml', '')]:
            with self.subTest(target=target):
                external = ' TargetMode="' + mode + '"' if mode else ''
                rels = (f'<Relationships xmlns="{REL}"><Relationship Id="rId2" Type="{R}/slide"'
                        f' Target="{target}"{external}/></Relationships>')
                self.make(overrides={'ppt/_rels/presentation.xml.rels': rels})
                with self.assertRaises(ValueError):
                    read_notes(self.path, 1)


if __name__ == '__main__':
    unittest.main()

"""Read saved PPTX speaker-note text for native LibreOffice save verification.

This module never modifies a ZIP member or presentation file. LibreOffice
Impress owns editing and saving; package reads only verify its actual output.
"""
from __future__ import annotations

import posixpath
import xml.etree.ElementTree as ET
import zipfile

P = 'http://schemas.openxmlformats.org/presentationml/2006/main'
A = 'http://schemas.openxmlformats.org/drawingml/2006/main'
R = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
REL = 'http://schemas.openxmlformats.org/package/2006/relationships'
NS = {'p': P, 'a': A, 'r': R}
MARKER = 'AI演讲备注'


def _xml(package, name):
    try:
        info = package.getinfo(name)
    except KeyError as error:
        raise ValueError('PPT package part is missing') from error
    if info.file_size > 8 * 1024 * 1024:
        raise ValueError('PPT XML part is too large')
    return ET.fromstring(package.read(info))


def _target(base, name):
    if not isinstance(name, str) or not name or any(c in name for c in ('\\', ':', '\x00', '?', '#')):
        raise ValueError('Invalid internal PPT relationship')
    path = posixpath.normpath(name.lstrip('/') if name.startswith('/')
                              else posixpath.join(posixpath.dirname(base), name))
    if not path.startswith('ppt/'):
        raise ValueError('PPT relationship leaves presentation package')
    return path


def _internal(relationship):
    if relationship.get('TargetMode', 'Internal') != 'Internal':
        raise ValueError('External PPT relationships are not supported')
    return relationship.get('Target')


def _slide_part(package, number):
    if type(number) is not int or number < 1:
        raise ValueError('Invalid slide number')
    presentation = _xml(package, 'ppt/presentation.xml')
    slides = presentation.findall('p:sldIdLst/p:sldId', NS)
    if number > len(slides):
        raise ValueError('Slide number is outside the presentation')
    ref = slides[number - 1].get('{' + R + '}id')
    rels = _xml(package, 'ppt/_rels/presentation.xml.rels')
    matches = [rel for rel in rels.findall('{' + REL + '}Relationship') if rel.get('Id') == ref]
    if len(matches) != 1 or matches[0].get('Type') != R + '/slide':
        raise ValueError('Slide relationship is unavailable')
    return _target('ppt/presentation.xml', _internal(matches[0]))


def _notes_part(package, slide):
    rel_name = posixpath.join(posixpath.dirname(slide), '_rels', posixpath.basename(slide) + '.rels')
    if rel_name not in package.namelist():
        return None
    rels = _xml(package, rel_name)
    matches = [rel for rel in rels.findall('{' + REL + '}Relationship') if rel.get('Type') == R + '/notesSlide']
    if len(matches) > 1:
        raise ValueError('Ambiguous speaker notes relationship')
    return _target(slide, _internal(matches[0])) if matches else None


def _body_text(root):
    shapes = [shape for shape in root.findall('p:cSld/p:spTree/p:sp', NS)
              if (shape.find('p:nvSpPr/p:nvPr/p:ph', NS) is not None
                  and shape.find('p:nvSpPr/p:nvPr/p:ph', NS).get('type') == 'body')]
    if len(shapes) > 1:
        raise ValueError('Ambiguous notes body placeholders')
    if not shapes:
        return ''
    return '\n'.join(
        ''.join((node.text or '') if node.tag == '{' + A + '}t' else '\n'
                for node in paragraph.iter() if node.tag in ('{' + A + '}t', '{' + A + '}br'))
        for paragraph in shapes[0].findall('p:txBody/a:p', NS)).strip()


def read_notes(path, number):
    with zipfile.ZipFile(path, 'r') as package:
        names = package.namelist()
        if len(names) != len(set(names)):
            raise ValueError('PPT package contains duplicate parts')
        part = _notes_part(package, _slide_part(package, number))
        return _body_text(_xml(package, part)) if part else ''

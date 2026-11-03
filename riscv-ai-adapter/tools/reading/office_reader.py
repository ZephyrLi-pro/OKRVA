"""Read-only Impress session owned by this bridge, through a private UNO pipe."""
import hashlib
import subprocess
import time
import uuid
import re
from pathlib import Path

from reader_core import Page


def page_heading(text):
    # Page-number text boxes can precede the actual title in PPT shape order.
    return next((line.strip() for line in text.splitlines()
                 if line.strip() and not re.fullmatch(r'\d+(\s*[/／]\s*\d+)?',line.strip())), '')


def shape_text(shape):
    values = []
    if hasattr(shape,'getString'):
        text = shape.getString().strip()
        if text:
            values.append(text)
    if hasattr(shape,'supportsService') and shape.supportsService('com.sun.star.drawing.TableShape'):
        table = shape.getPropertyValue('Model')
        for row in range(table.getRows().getCount()):
            cells = [table.getCellByPosition(col,row).getString().strip()
                     for col in range(table.getColumns().getCount())]
            if any(cells):
                values.append(' / '.join(cells))
    if hasattr(shape,'getCount'):
        for index in range(shape.getCount()):
            values.extend(shape_text(shape.getByIndex(index)))
    return values


class OfficeReader:
    def __init__(self, path, profile, emit, start_page=1):
        self.path = Path(path).resolve(strict=True)
        self.profile = Path(profile).resolve()
        self.emit = emit
        self.start_page = start_page
        self.process = self.doc = self.presentation = None
        self.pages = None
        self.closed = False
        self.names = []
        self.text = []
        self.digest = None
        self._show_controller = None
        self._show_pages = ()

    def open(self):
        import uno
        if self.path.suffix.lower() not in ('.pptx','.ppt','.odp'):
            raise ValueError('Reading presentation must be PPT/PPTX/ODP')
        self.digest = hashlib.sha256(self.path.read_bytes()).hexdigest()
        self.profile.mkdir(parents=True,exist_ok=True)
        pipe = 'rvai_reader_'+uuid.uuid4().hex
        self.process = subprocess.Popen(['/usr/bin/libreoffice',
            '-env:UserInstallation='+self.profile.as_uri(),'--impress',
            '--norestore','--nodefault','--nofirststartwizard','--nologo',
            '--accept=pipe,name='+pipe+';urp;StarOffice.ServiceManager'])
        context = uno.getComponentContext()
        resolver = context.ServiceManager.createInstanceWithContext(
            'com.sun.star.bridge.UnoUrlResolver',context)
        remote = None
        deadline = time.monotonic()+40
        while time.monotonic()<deadline:
            try:
                remote = resolver.resolve('uno:pipe,name='+pipe+';urp;StarOffice.ComponentContext')
                break
            except Exception:
                time.sleep(.2)
        if remote is None:
            raise RuntimeError('Private LibreOffice session did not start')
        desktop = remote.ServiceManager.createInstanceWithContext('com.sun.star.frame.Desktop',remote)
        def prop(name,value):
            p=uno.createUnoStruct('com.sun.star.beans.PropertyValue')
            p.Name,p.Value=name,value
            return p
        self.emit('presentation_loading',file=self.path.name)
        self.doc = desktop.loadComponentFromURL(self.path.as_uri(),'_blank',0,(
            prop('ReadOnly',True), prop('UpdateDocMode',0),
            prop('MacroExecutionMode',uno.getConstantByName('com.sun.star.document.MacroExecMode.NEVER_EXECUTE'))))
        if self.doc is None or not self.doc.isReadonly():
            raise RuntimeError('Could not open presentation read-only')
        self.pages = self.doc.getDrawPages()
        for index in range(self.pages.getCount()):
            page = self.pages.getByIndex(index)
            self.names.append(page.Name)
            self.text.append('\n'.join(shape_text(page)))
        if not 1<=self.start_page<=len(self.text):
            raise ValueError('Initial page is outside the presentation')
        self.presentation = self.doc.getPresentation()
        self.presentation.IsFullScreen = False
        self.presentation.IsAutomatic = False
        self.presentation.IsEndless = False
        self.presentation.StartWithNavigator = False
        self.presentation.IsAlwaysOnTop = False
        self.presentation.start()
        deadline=time.monotonic()+10
        while not self.presentation.isRunning() and time.monotonic()<deadline:
            time.sleep(.1)
        if self.presentation.isRunning():
            self.presentation.getController().gotoSlideIndex(self.start_page-1)
        else:
            self.doc.getCurrentController().setCurrentPage(self.pages.getByIndex(self.start_page-1))
        self.emit('presentation_ready',pages=len(self.text),readonly=True,source_sha256=self.digest)
        return self.current()

    def _cache_slideshow(self, controller):
        count = controller.getSlideCount()
        if controller == self._show_controller and len(self._show_pages) == count:
            return
        snapshots = []
        for index in range(count):
            selected = controller.getSlideByIndex(index)
            name = selected.Name
            text = (self.text[self.names.index(name)] if name in self.names
                    else '\n'.join(shape_text(selected)))
            snapshots.append(Page(index+1, count, text, 'slideshow', page_heading(text)))
        self._show_controller = controller
        self._show_pages = tuple(snapshots)

    def current(self):
        if self.closed or self.doc is None:
            return None
        try:
            if self.presentation.isRunning():
                controller = self.presentation.getController()
                self._cache_slideshow(controller)
                index = controller.getCurrentSlideIndex()
                if not 0 <= index < len(self._show_pages):
                    raise RuntimeError('Invalid current slideshow index')
                return self._show_pages[index]
            self._show_controller = None
            self._show_pages = ()
            selected = self.doc.getCurrentController().getCurrentPage()
            name = selected.Name
            index = self.names.index(name)
            return Page(index+1, len(self.text), self.text[index], 'document', page_heading(self.text[index]))
        except Exception as error:
            self.emit('presentation_unavailable', error=str(error))
            self.closed = True
            return None

    def _goto_from(self, number, page):
        if page is None:
            return None
        if not 1 <= number <= page.total:
            raise ValueError('Page number out of range')
        if number == page.number:
            return page
        if self.presentation.isRunning():
            self.presentation.getController().gotoSlideIndex(number-1)
        else:
            self.doc.getCurrentController().setCurrentPage(self.pages.getByIndex(number-1))
        deadline = time.monotonic()+12
        while time.monotonic() < deadline:
            actual = self.current()
            if actual is None or actual.number == number:
                return actual
            time.sleep(.02)
        raise RuntimeError('Presentation did not confirm requested page')

    def goto(self, number):
        return self._goto_from(number, self.current())

    def navigate(self, direction):
        if direction not in ('left', 'right'):
            raise ValueError('Invalid reading direction')
        page = self.current()
        if page is None:
            return None
        step = 1 if direction == 'right' else -1
        return self._goto_from(max(1, min(page.total, page.number+step)), page)

    def close(self):
        if self.doc is not None and not self.closed:
            try:
                if self.presentation.isRunning():
                    self.presentation.end()
                self.doc.setModified(False)
                self.doc.close(True)
            except Exception:
                pass
        self.closed=True
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            try:self.process.wait(timeout=4)
            except subprocess.TimeoutExpired:self.process.kill();self.process.wait()
        if self.digest is not None:
            assert hashlib.sha256(self.path.read_bytes()).hexdigest()==self.digest, 'Source presentation changed'

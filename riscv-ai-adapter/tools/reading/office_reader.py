"""Control the user's normal editable LibreOffice Impress through local UNO."""
import hashlib
import os
import subprocess
import time
import uuid
import re
import json
import stat
from pathlib import Path

from reader_core import Page
from speaker_notes import MARKER, read_notes


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


def runtime_settings():
    launcher = Path(os.environ.get('RVAI_LIBREOFFICE_LAUNCHER',
        str(Path.home()/'.local/bin/libreoffice-24.8'))).expanduser()
    pipe = os.environ.get('RVAI_IMPRESS_PIPE', 'rvai_trial_248_'+str(os.getuid()))
    if not launcher.is_absolute():
        raise ValueError('LibreOffice launcher must be an absolute path')
    if not re.fullmatch(r'[A-Za-z0-9_-]+', pipe):
        raise ValueError('Invalid local LibreOffice UNO pipe name')
    return str(launcher), pipe


def activate_frame(frame):
    frame.activate()
    window = frame.getContainerWindow()
    if hasattr(window, 'IsMinimized'):
        window.IsMinimized = False
    window.setVisible(True)
    if hasattr(window, 'toFront'):
        window.toFront()
    # GTK/Wayland can report IsMinimized=False while the compositor still
    # hides the surface. Restore only this observed native Impress window and
    # confirm its compositor state before acknowledging an explicit open.
    command = '/usr/bin/wlrctl'
    if os.environ.get('WAYLAND_DISPLAY') and Path(command).is_file():
        title = window.getAccessibleContext().getAccessibleName()
        if not isinstance(title, str) or not title:
            raise RuntimeError('无法确定当前PPT窗口标题，尚未确认窗口已恢复。')
        match = ['app_id:libreoffice-impress', 'title:'+title]
        result = subprocess.run([command, 'toplevel', 'focus', *match],
            text=True, capture_output=True, timeout=3)
        if result.returncode:
            raise RuntimeError('尚未恢复当前PPT窗口：'+result.stderr.strip()[:200])
        deadline = time.monotonic()+3
        while time.monotonic()<deadline:
            result = subprocess.run([command, 'toplevel', 'find', *match, 'state:unminimized'],
                text=True, capture_output=True, timeout=2)
            if result.returncode==0 and not result.stderr.strip():
                return window
            if result.returncode not in (0, 1) or result.stderr.strip():
                break
            time.sleep(.05)
        raise RuntimeError('当前PPT在Wayland中的窗口恢复状态尚未确认。')
    return window


def unknown_visibility():
    return dict(presentation_visible=False, visibility_source='unknown', visibility_checked_at=0.)


def exact_window_visibility(title, checked_at):
    """Read compositor state without activating any application window."""
    unknown = unknown_visibility()
    command = '/usr/bin/wlrctl'
    if (not os.environ.get('WAYLAND_DISPLAY') or not Path(command).is_file()
            or not isinstance(title, str) or not title or any(c in title for c in '\r\n\0')):
        return unknown
    match = ['app_id:libreoffice-impress', 'title:'+title]
    try:
        matches = subprocess.run([command, 'toplevel', 'list', *match],
            text=True, capture_output=True, timeout=.5)
        # A title is not an object handle: duplicate captions are ambiguous.
        # Check the returned full name too, rather than trusting filter syntax.
        if (matches.returncode != 0 or matches.stderr.strip()
                or matches.stdout.splitlines() != ['libreoffice-impress: '+title]):
            return unknown
        status = subprocess.run([command, 'toplevel', 'find', *match, 'state:unminimized'],
            text=True, capture_output=True, timeout=.5)
        if status.returncode not in (0, 1) or status.stderr.strip():
            return unknown
        return dict(presentation_visible=status.returncode==0,
            visibility_source='wayland_exact_window', visibility_checked_at=checked_at)
    except (OSError, subprocess.TimeoutExpired):
        return unknown


def recent_source(state_path):
    """Restore this user's last real document as context without opening it."""
    try:
        descriptor=os.open(state_path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
        with os.fdopen(descriptor,encoding='utf-8') as stream:
            info=os.fstat(stream.fileno())
            if (not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid()
                    or info.st_mode & 0o077 or info.st_size>65536):
                return None
            value=json.loads(stream.read(65537))
        if (not isinstance(value,dict) or not isinstance(value.get('session_id'),str)
                or not re.fullmatch(r'[0-9a-f]{32}',value['session_id'])
                or not isinstance(value.get('source_path'),str)):
            return None
        source=Path(value['source_path'])
        if (not source.is_absolute() or source.suffix.lower() not in ('.ppt','.pptx','.odp')
                or source.resolve(strict=True)!=source or not source.is_file()):
            return None
        return source
    except (OSError,ValueError,TypeError):
        return None


class OfficeReader:
    def __init__(self, path, profile, emit, start_page=1):
        self.path = Path(path).resolve(strict=True) if path is not None else None
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
        self.session_id = uuid.uuid4().hex
        self._snapshots = {}
        self._notes_view = False
        self.desktop = None
        self.remote_context = None
        self.runtime_version = None
        self._visibility_cache = unknown_visibility()
        self._visibility_key = None
        self._visibility_cached_at = None

    def open(self):
        import uno
        if self.path is None:
            raise ValueError('请先选择要打开的PPT。')
        if self.path.suffix.lower() not in ('.pptx','.ppt','.odp'):
            raise ValueError('Reading presentation must be PPT/PPTX/ODP')
        self.digest = hashlib.sha256(self.path.read_bytes()).hexdigest()
        launcher, pipe = runtime_settings()
        context = uno.getComponentContext()
        resolver = context.ServiceManager.createInstanceWithContext(
            'com.sun.star.bridge.UnoUrlResolver',context)
        remote = None
        try:
            remote = resolver.resolve('uno:pipe,name='+pipe+';urp;StarOffice.ComponentContext')
        except Exception:
            # The selected native application owns its persistent profile.
            # Never silently fall back to the distribution's older office.
            self.process = subprocess.Popen([launcher,
                '--norestore','--nodefault','--nofirststartwizard','--nologo',
                '--accept=pipe,name='+pipe+';urp;StarOffice.ServiceManager'],
                stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,
                start_new_session=True)
        deadline = time.monotonic()+90
        while time.monotonic()<deadline:
            if remote is not None:
                break
            try:
                remote = resolver.resolve('uno:pipe,name='+pipe+';urp;StarOffice.ComponentContext')
                break
            except Exception:
                time.sleep(.2)
        if remote is None:
            raise RuntimeError('LibreOffice 24.8未能建立本地控制连接；请检查正式启动入口和UNO管道。')
        provider = remote.ServiceManager.createInstanceWithContext(
            'com.sun.star.configuration.ConfigurationProvider',remote)
        node=uno.createUnoStruct('com.sun.star.beans.PropertyValue')
        node.Name,node.Value='nodepath','/org.openoffice.Setup/Product'
        product=provider.createInstanceWithArguments(
            'com.sun.star.configuration.ConfigurationAccess',(node,))
        self.runtime_version=str(product.getPropertyValue('ooSetupVersionAboutBox'))
        match=re.match(r'^(\d+)\.(\d+)',self.runtime_version)
        if match is None or tuple(map(int,match.groups()))<(24,8):
            raise RuntimeError('PPT服务要求LibreOffice 24.8或更新版本，当前连接版本：'+self.runtime_version)
        self.desktop = remote.ServiceManager.createInstanceWithContext('com.sun.star.frame.Desktop',remote)
        self.remote_context = remote
        def prop(name,value):
            p=uno.createUnoStruct('com.sun.star.beans.PropertyValue')
            p.Name,p.Value=name,value
            return p
        self.emit('presentation_loading',file=self.path.name)
        self.doc = self.desktop.loadComponentFromURL(self.path.as_uri(),'_default',0,(
            prop('ReadOnly',False), prop('UpdateDocMode',0),
            prop('MacroExecutionMode',uno.getConstantByName('com.sun.star.document.MacroExecMode.NEVER_EXECUTE'))))
        if self.doc is None or self.doc.isReadonly():
            raise RuntimeError('PPT未能以可编辑方式打开，请检查文件权限或文件锁。')
        self.pages = self.doc.getDrawPages()
        for index in range(self.pages.getCount()):
            page = self.pages.getByIndex(index)
            self.names.append(page.Name)
            self.text.append('\n'.join(shape_text(page)))
        if not 1<=self.start_page<=len(self.text):
            raise ValueError('Initial page is outside the presentation')
        self.presentation = self.doc.getPresentation()
        # Open in ordinary Impress editing view. A user-started F5 slideshow
        # is detected and controlled without changing its normal settings.
        if self.presentation.isRunning():
            controller=self.presentation.getController()
            self._cache_slideshow(controller)
            controller.gotoSlideIndex(self.start_page-1)
        else:
            self.doc.getCurrentController().setCurrentPage(self.pages.getByIndex(self.start_page-1))
        frame=self.doc.getCurrentController().getFrame()
        activate_frame(frame)
        self.emit('presentation_ready',pages=len(self.text),readonly=False,source_sha256=self.digest,
                  office_version=self.runtime_version,uno_pipe=pipe)
        return self.current()

    def _normal_page(self, selected):
        # In Notes view getCurrentPage returns the notes page, not the slide.
        indexes=list(range(self.pages.getCount()))
        try:
            candidate=int(selected.Number)-1
            if candidate in indexes:
                indexes.remove(candidate)
                indexes.insert(0,candidate)
        except (AttributeError,ValueError,TypeError):
            pass
        for index in indexes:
            slide = self.pages.getByIndex(index)
            if selected == slide:
                return index, slide, False
            if selected == slide.getNotesPage():
                return index, slide, True
        raise RuntimeError('当前视图不是可识别的幻灯片或备注页。')

    def _follow_native_document(self):
        if self.desktop is None:
            return True
        selected = self.desktop.getCurrentComponent()
        if selected is None or not selected.supportsService('com.sun.star.presentation.PresentationDocument'):
            return False
        if selected != self.doc or selected.getURL() != self.path.as_uri():
            import uno
            url = selected.getURL()
            if not url.startswith('file:'):
                return False
            self.doc = selected
            self.path = Path(uno.fileUrlToSystemPath(url)).resolve(strict=True)
            self.digest = hashlib.sha256(self.path.read_bytes()).hexdigest()
            self.pages = selected.getDrawPages()
            self.names = [self.pages.getByIndex(i).Name for i in range(self.pages.getCount())]
            self.text = ['\n'.join(shape_text(self.pages.getByIndex(i))) for i in range(self.pages.getCount())]
            self.presentation = selected.getPresentation()
            self._show_controller, self._show_pages = None, ()
            self.session_id = uuid.uuid4().hex
            self._snapshots.clear()
        elif self.pages.getCount()!=len(self.names):
            self.names = [self.pages.getByIndex(i).Name for i in range(self.pages.getCount())]
            self.text = ['\n'.join(shape_text(self.pages.getByIndex(i))) for i in range(self.pages.getCount())]
            self.session_id = uuid.uuid4().hex
            self._snapshots.clear()
            self._show_controller,self._show_pages=None,()
        return True

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
            if not self._follow_native_document():
                return None
            controller=self._show_controller
            # The active controller exposes its own liveness and slide index.
            # Re-entering XPresentation for every poll can wait for the UI's
            # rendering lock for seconds after a slide change.
            if controller is not None and not controller.isRunning():
                controller=None
                self._show_controller=None
                self._show_pages=()
            if controller is None and self.presentation.isRunning():
                controller=self.presentation.getController()
            if controller is not None:
                self._cache_slideshow(controller)
                index = controller.getCurrentSlideIndex()
                if not 0 <= index < len(self._show_pages):
                    raise RuntimeError('Invalid current slideshow index')
                return self._show_pages[index]
            self._show_controller = None
            self._show_pages = ()
            selected = self.doc.getCurrentController().getCurrentPage()
            index, slide, notes_view = self._normal_page(selected)
            if self.names[index] != slide.Name:
                self.names = [self.pages.getByIndex(i).Name for i in range(self.pages.getCount())]
                self.text = ['\n'.join(shape_text(self.pages.getByIndex(i))) for i in range(self.pages.getCount())]
                self.session_id = uuid.uuid4().hex
                self._snapshots.clear()
            self._notes_view = notes_view
            self.text[index] = '\n'.join(shape_text(slide))
            return Page(index+1, len(self.text), self.text[index], 'notes' if self._notes_view else 'document', page_heading(self.text[index]))
        except Exception as error:
            self.emit('presentation_unavailable', error=str(error))
            return None

    def _goto_from(self, number, page):
        if page is None:
            return None
        if not 1 <= number <= page.total:
            raise ValueError('Page number out of range')
        if number == page.number:
            return page
        if page.mode=='slideshow':
            controller=self._show_controller
            if controller is None or not controller.isRunning():
                raise RuntimeError('Slideshow controller is no longer running')
            controller.gotoSlideIndex(number-1)
        else:
            selected = self.pages.getByIndex(number-1)
            if page.mode == 'notes':
                selected = selected.getNotesPage()
            self.doc.getCurrentController().setCurrentPage(selected)
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

    def metadata(self, *, include_visibility=True):
        result = dict(session_id=self.session_id, source_path=str(self.path) if self.path is not None else '')
        if not include_visibility:
            return result
        if self.closed or self.doc is None:
            self._visibility_key = None
            self._visibility_cache = unknown_visibility()
            return dict(result, **self._visibility_cache)
        # metadata() runs on the existing serialized UNO worker. GetState only
        # reads ReaderBridge's cached copy, never spawns wlrctl or calls UNO.
        started = time.monotonic()
        try:
            window = self.doc.getCurrentController().getFrame().getContainerWindow()
            title = window.getAccessibleContext().getAccessibleName()
        except Exception:
            self._visibility_key = None
            self._visibility_cache = unknown_visibility()
            return dict(result, **self._visibility_cache)
        key = (self.session_id, result['source_path'], title)
        if (key != self._visibility_key or self._visibility_cached_at is None
                or not 0 <= started-self._visibility_cached_at < .5):
            self._visibility_key = key
            self._visibility_cached_at = started
            # Commit results only after a completed probe, retaining its START
            # time so an overlapping pre-minimize query cannot authorize resume.
            self._visibility_cache = exact_window_visibility(title, started)
        return dict(result, **self._visibility_cache)

    def snapshot(self):
        page = self.current()
        if page is None:
            raise RuntimeError('PPT阅读窗口已关闭，请先打开PPT。')
        if self._show_controller is not None:
            selected = self._show_controller.getSlideByIndex(page.number-1)
        else:
            selected = self.doc.getCurrentController().getCurrentPage()
        original_index, slide, _ = self._normal_page(selected)
        text = '\n'.join(shape_text(slide)).strip()
        if not text:
            raise ValueError('当前页没有可提取的原生文字，不能据此生成演讲备注。')
        if len(text) > 12000:
            raise ValueError('当前页正文超过12000字，请先拆分页面再生成演讲备注。')
        digest = hashlib.sha256(text.encode()).hexdigest()
        self._snapshots[(page.number, digest)] = dict(
            number=original_index+1, slide=slide,
            source_digest=hashlib.sha256(self.path.read_bytes()).hexdigest())
        if len(self._snapshots) > 128:
            self._snapshots.pop(next(iter(self._snapshots)))
        notes = self._notes_text(slide)
        return dict(ok=True, **self.metadata(include_visibility=False), page=page.number, total=page.total,
                    title=self.path.stem, page_title=page_heading(text), text=text,
                    text_sha256=digest, notes=notes)

    def open_presentation(self, path):
        selected = Path(path).expanduser().resolve(strict=True)
        if not selected.is_file() or selected.suffix.lower() not in ('.pptx','.ppt','.odp'):
            raise ValueError('请选择PPT、PPTX或ODP文件。')
        if selected.stat().st_size > 512*1024*1024:
            raise ValueError('PPT文件不能超过512MB。')
        try:
            connected = not self.closed and self._follow_native_document()
        except Exception:
            connected = False
        if (connected and selected == self.path
                and hashlib.sha256(selected.read_bytes()).hexdigest() == self.digest):
            # Reuse the already loaded deck, especially the 189MB demo.
            if self.presentation.isRunning():
                self.presentation.end()
            self._show_controller, self._show_pages = None, ()
            self._notes_view = False
            self.doc.getCurrentController().setCurrentPage(self.pages.getByIndex(0))
            activate_frame(self.doc.getCurrentController().getFrame())
            self.session_id = uuid.uuid4().hex
            self._snapshots.clear()
            return self.current()
        profile, emit = self.profile, self.emit
        self.close()
        self.__init__(selected, profile, emit, start_page=1)
        return self.open()

    def save_speaker_notes(self, session_id, number, text_sha256, notes):
        current = self.current()
        if session_id != self.session_id or self.closed:
            raise ValueError('PPT已切换或关闭，本次演讲备注没有保存。')
        if type(number) is not int or (number, text_sha256) not in self._snapshots:
            raise ValueError('没有匹配的当前页快照，请重新生成演讲备注。')
        snapshot = self._snapshots[(number, text_sha256)]
        original_number = snapshot['number']
        if self.pages.getByIndex(original_number-1) != snapshot['slide']:
            raise ValueError('幻灯片顺序已更改，请重新生成演讲备注。')
        original_text = '\n'.join(shape_text(snapshot['slide'])).strip()
        if hashlib.sha256(original_text.encode()).hexdigest() != text_sha256:
            raise ValueError('页面正文已更改，请重新生成演讲备注。')
        if current is None:
            raise RuntimeError('PPT阅读窗口已关闭，本次演讲备注没有保存。')
        if self.path.suffix.lower() != '.pptx':
            raise ValueError('请先用LibreOffice将演示文稿另存为PPTX后再添加演讲备注。')
        if not isinstance(notes,str) or not notes.strip() or len(notes)>6000:
            raise ValueError('演讲备注内容为空或过长。')
        if self.doc.isReadonly():
            raise ValueError('当前PPT是只读的，请在LibreOffice中启用编辑后再保存备注。')
        if hashlib.sha256(self.path.read_bytes()).hexdigest()!=snapshot['source_digest']:
            raise ValueError('PPT磁盘文件已更改，请重新打开后再生成备注。')
        notes_page = self.pages.getByIndex(original_number-1).getNotesPage()
        notes_shapes = [s for s in (notes_page.getByIndex(i) for i in range(notes_page.getCount()))
            if s.getShapeType()=='com.sun.star.presentation.NotesShape' and hasattr(s,'getString')]
        if len(notes_shapes)!=1:
            raise RuntimeError('未找到可编辑的演讲备注区，本次未保存。')
        self._prepare_notes_edit()
        before = notes_shapes[0].getString().strip()
        addition = MARKER+'\n'+notes.strip()
        saved = before if before.endswith(addition) else '\n\n'.join(t for t in (before,addition) if t)
        notes_shapes[0].setString(saved)
        # Apply to actual text runs, including Chinese. Shape defaults alone
        # leave explicit template formatting on existing paragraphs untouched.
        cursor = notes_shapes[0].getText().createTextCursor()
        cursor.gotoStart(False)
        cursor.gotoEnd(True)
        for name in ('CharHeight', 'CharHeightAsian', 'CharHeightComplex'):
            cursor.setPropertyValue(name, 12.0)
        # Standard Save includes other edits too. Suppress native automatic
        # backup for this save only, then restore the user's existing choice.
        self._store_without_backup()
        self.digest = hashlib.sha256(self.path.read_bytes()).hexdigest()
        disk = read_notes(self.path, original_number)
        loaded = '\n'.join(s.getString().strip() for s in
            (notes_page.getByIndex(i) for i in range(notes_page.getCount()))
            if s.getShapeType()=='com.sun.star.presentation.NotesShape' and hasattr(s,'getString'))
        normalize = lambda text: '\n'.join(line.strip() for line in text.replace('\r','').split('\n')).strip()
        if normalize(saved) != normalize(disk) or normalize(saved) != normalize(loaded):
            raise RuntimeError('PPT已写入，但可见备注内容核验失败，请重新打开PPT。')
        display=self.show_speaker_notes(original_number)
        self.emit('speaker_notes_saved', page=number, slide_number=original_number,
                  notes_chars=len(saved), notes_sha256=hashlib.sha256(saved.encode()).hexdigest())
        return dict(ok=True, page=number, path=str(self.path), notes=saved,
                    notes_sha256=hashlib.sha256(saved.encode()).hexdigest(), verified=True, **display)

    def _notes_text(self, slide):
        page = slide.getNotesPage()
        return '\n'.join(shape.getString().strip() for shape in
            (page.getByIndex(i) for i in range(page.getCount()))
            if shape.getShapeType()=='com.sun.star.presentation.NotesShape' and hasattr(shape,'getString'))

    def _prepare_notes_edit(self):
        """Commit and release the bottom editor before changing its model."""
        import uno
        if self.presentation.isRunning():
            self.presentation.end()
            self._show_controller, self._show_pages = None, ()
        controller = self.doc.getCurrentController()
        framework = controller.getConfigurationController()
        resources = framework.getCurrentConfiguration().getResources(
            None, '', uno.Enum('com.sun.star.drawing.framework.AnchorBindingMode', 'INDIRECT'))
        bottom = None
        for resource in resources:
            if resource.getResourceURL() == 'private:resource/pane/CenterPane':
                framework.getResource(resource).getWindow().setFocus()
            elif resource.getResourceURL() == 'private:resource/pane/BottomImpressPane':
                bottom = resource
        if bottom is None:
            return
        frame = controller.getFrame()
        context = uno.getComponentContext()
        dispatcher = context.ServiceManager.createInstanceWithContext('com.sun.star.frame.DispatchHelper', context)
        disabled = uno.createUnoStruct('com.sun.star.beans.PropertyValue')
        disabled.Name, disabled.Value = 'BottomPaneImpress', False
        dispatcher.executeDispatch(frame, '.uno:BottomPaneImpress', '', 0, (disabled,))
        deadline = time.monotonic()+8
        while time.monotonic()<deadline:
            if not framework.getCurrentConfiguration().hasResource(bottom):
                return
            time.sleep(.05)
        raise RuntimeError('备注编辑区尚未结束编辑，本次未覆盖备注。')

    def _store_without_backup(self):
        if self.remote_context is None:
            raise RuntimeError('LibreOffice保存连接不可用，本次未保存。')
        import uno
        context=self.remote_context
        provider=context.ServiceManager.createInstanceWithContext(
            'com.sun.star.configuration.ConfigurationProvider',context)
        node=uno.createUnoStruct('com.sun.star.beans.PropertyValue')
        node.Name,node.Value='nodepath','/org.openoffice.Office.Common/Save/Document'
        options=provider.createInstanceWithArguments(
            'com.sun.star.configuration.ConfigurationUpdateAccess',(node,))
        original=bool(options.getPropertyValue('CreateBackup'))
        if original:
            options.setPropertyValue('CreateBackup',False)
            options.commitChanges()
        try:
            self.doc.store()
        finally:
            if original:
                options.setPropertyValue('CreateBackup',True)
                options.commitChanges()

    def show_speaker_notes(self, number):
        import uno
        if self.presentation.isRunning():
            self.presentation.end()
        self._show_controller, self._show_pages = None, ()
        controller = self.doc.getCurrentController()
        frame = controller.getFrame()
        context = uno.getComponentContext()
        dispatcher = context.ServiceManager.createInstanceWithContext('com.sun.star.frame.DispatchHelper', context)
        # NotesMode is a printable notes-page layout, which shrinks the slide.
        # Keep the ordinary large slide, using a bottom pane only if supported.
        dispatcher.executeDispatch(frame, '.uno:NormalMultiPaneGUI', '', 0, ())
        controller = self.doc.getCurrentController()
        controller.setCurrentPage(self.pages.getByIndex(number-1))
        frame=controller.getFrame()
        window = activate_frame(frame)
        self._notes_view = False
        pane,url=self._notes_pane_dispatch(frame)
        if pane is not None and not self._notes_pane_visible(pane,url):
            enabled=uno.createUnoStruct('com.sun.star.beans.PropertyValue')
            enabled.Name,enabled.Value='BottomPaneImpress',True
            dispatcher.executeDispatch(frame,'.uno:BottomPaneImpress','',0,(enabled,))
        deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            actual=self.current()
            visible = not hasattr(window,'isVisible') or window.isVisible()
            minimized = getattr(window,'IsMinimized',False)
            if actual is not None and actual.mode=='document' and actual.number==number and visible and not minimized:
                if pane is None:
                    return dict(notes_visible=False,display_mode='normal',
                                notes_pane_supported=False,normal_view_visible=True)
                if self._notes_pane_visible(pane,url):
                    return dict(notes_visible=True,display_mode='normal',
                                notes_pane_supported=True,normal_view_visible=True)
            time.sleep(.05)
        raise RuntimeError('PPT备注已保存，但普通视图或底部备注栏的显示尚未确认。')

    def _notes_pane_dispatch(self, frame):
        import uno
        url=uno.createUnoStruct('com.sun.star.util.URL')
        url.Complete='.uno:BottomPaneImpress'
        url.Protocol='.uno:'
        url.Path='BottomPaneImpress'
        return frame.queryDispatch(url,'',0),url

    def _notes_pane_visible(self, dispatch, url):
        import unohelper
        from com.sun.star.frame import XStatusListener
        class Status(unohelper.Base,XStatusListener):
            def __init__(self):self.visible=False
            def statusChanged(self,event):
                self.visible=event.IsEnabled and type(event.State) is bool and event.State
            def disposing(self,event):pass
        listener=Status()
        dispatch.addStatusListener(listener,url)
        dispatch.removeStatusListener(listener,url)
        return listener.visible

    def close(self):
        # The user's ordinary LibreOffice window and unsaved edits outlive
        # this background control bridge. Never close/kill/clear them here.
        self.closed=True
        self.doc = self.presentation = self.pages = self.desktop = None

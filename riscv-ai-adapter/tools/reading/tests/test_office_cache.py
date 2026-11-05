"""Protect slide identity across hidden/custom shows and controller changes."""
import types,unittest
from pathlib import Path
from office_reader import OfficeReader

class Controller:
    def __init__(self,names,index=0):
        self.names,self.index,self.jumps=names,index,[]
        self.running=True
    def isRunning(self):return self.running
    def getSlideCount(self):return len(self.names)
    def getSlideByIndex(self,index):return types.SimpleNamespace(Name=self.names[index])
    def getCurrentSlideIndex(self):return self.index
    def gotoSlideIndex(self,index):self.jumps.append(index);self.index=index

class Presentation:
    def __init__(self,controller):
        self._controller=controller;self.running_calls=0;self.controller_calls=0
    @property
    def controller(self):return self._controller
    @controller.setter
    def controller(self,value):
        self._controller.running=False;self._controller=value
    def isRunning(self):self.running_calls+=1;return True
    def getController(self):self.controller_calls+=1;return self.controller

class SlideshowIdentityTests(unittest.TestCase):
    def setUp(self):
        self.reader=OfficeReader(Path(__file__),Path(__file__).parent,lambda *a,**kw:None)
        self.reader.doc=object()
        self.reader.names=['pageA','pageB','pageC']
        self.reader.text=['Title A\nBody A','Title B\nBody B','Title C\nBody C']

    def start(self,names):
        self.controller=Controller(names)
        self.reader.presentation=Presentation(self.controller)

    def test_hidden_custom_order_uses_selected_slide_text(self):
        self.start(['pageC','pageA'])
        first=self.reader.current()
        self.assertEqual((first.number,first.total,first.text),(1,2,'Title C\nBody C'))
        self.controller.index=1
        second=self.reader.current()
        self.assertEqual((second.number,second.text),(2,'Title A\nBody A'))

    def test_restarted_show_same_count_rebuilds_identity(self):
        self.start(['pageC','pageA']);self.reader.current()
        self.reader.presentation.controller=Controller(['pageA','pageC'])
        self.assertEqual(self.reader.current().text,'Title A\nBody A')

    def test_changed_show_count_is_not_mapped_to_old_order(self):
        self.start(['pageC','pageA']);self.reader.current()
        self.controller.names=['pageB','pageC','pageA'];self.controller.index=0
        self.assertEqual((self.reader.current().total,self.reader.current().text),(3,'Title B\nBody B'))

    def test_boundary_swipe_does_not_restart_same_slide(self):
        self.start(['pageA','pageC'])
        self.assertEqual(self.reader.navigate('left').number,1)
        self.assertEqual(self.controller.jumps,[])
        self.assertEqual(self.reader.navigate('right').text,'Title C\nBody C')
        self.assertEqual(self.controller.jumps,[1])
        self.assertEqual(self.reader.navigate('right').number,2)
        self.assertEqual(self.controller.jumps,[1])

    def test_active_show_polling_and_navigation_avoid_presentation_render_lock(self):
        self.start(['pageA','pageC']);self.reader.current()
        for _ in range(5):self.reader.current()
        self.reader.navigate('right')
        self.assertEqual(self.reader.presentation.running_calls,1)
        self.assertEqual(self.reader.presentation.controller_calls,1)
        self.assertEqual(self.reader.current().text,'Title C\nBody C')

if __name__=='__main__':unittest.main()

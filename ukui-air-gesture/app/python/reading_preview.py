"""Optional calibration view. Closing this view leaves recognition running."""
import os
import subprocess
import time

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont


class ReadingPreview:
    WINDOW = '阅读手势 — 正式程序校准'

    def __init__(self):
        font_path = subprocess.run(['fc-match','-f','%{file}','sans:lang=zh-cn'],
            capture_output=True, text=True, check=True).stdout.strip()
        self.font = ImageFont.truetype(font_path, 22)
        self.small = ImageFont.truetype(font_path, 18)
        self.large = ImageFont.truetype(font_path, 30)
        self.connections = [(0,1),(1,2),(2,3),(3,4),(0,5),(5,6),(6,7),(7,8),
            (5,9),(9,10),(10,11),(11,12),(9,13),(13,14),(14,15),(15,16),
            (13,17),(0,17),(17,18),(18,19),(19,20)]
        self.last_sequence = 0
        self.first_frame = True
        self.last_panel = 0.
        self.header = self.footer = None
        cv2.namedWindow(self.WINDOW, cv2.WINDOW_NORMAL | cv2.WINDOW_GUI_NORMAL)
        cv2.resizeWindow(self.WINDOW, 640, 720)

    def update(self, latest, snapshot):
        if latest is not None and latest[0] != self.last_sequence:
            self.last_sequence = latest[0]
            image = cv2.flip(latest[2], 1)
            height, width = image.shape[:2]
            origin = snapshot.get('navigation_origin_x', .5)
            half = snapshot.get('navigation_home_half_width', .04)
            distance = snapshot.get('navigation_stroke_distance', .10)
            left, right = ((.4,.6) if snapshot.get('navigation_phase')=='await_center'
                           else (origin-half,origin+half))
            cv2.rectangle(image, (round(width*left),1), (round(width*right),height-2), (230,170,30),2)
            for x in (origin-distance,origin+distance):
                cv2.line(image, (round(width*x),0), (round(width*x),height-1), (80,200,110),1)
            landmarks = snapshot.get('landmarks')
            if landmarks and time.monotonic()-snapshot.get('frame_time',0)<.65:
                points = [(round((1-p.x)*width),round(p.y*height)) for p in landmarks]
                for a,b in self.connections:
                    cv2.line(image,points[a],points[b],(0,220,0),2)
                for point in points:
                    cv2.circle(image,point,4,(0,255,255),-1)
                if snapshot.get('palm_center_x') is not None:
                    center = (round(snapshot['palm_center_x']*width),
                              round(snapshot['palm_center_y']*width))
                    cv2.circle(image,center,10,(0,165,255),3)
            now = time.monotonic()
            if self.header is None or now-self.last_panel >= .25:
                header = Image.new('RGB',(width,90),'#13283b')
                draw = ImageDraw.Draw(header)
                counts = snapshot.get('reading_counts',{})
                draw.text((14,5),f'左挥：{counts.get("horizontal:left",0)} 次   右挥：{counts.get("horizontal:right",0)} 次',
                          font=self.large,fill='#86efac')
                draw.text((14,52),'掌心稍停确定起点，左右挥动后回起点',font=self.font,fill='#fde68a')
                footer = Image.new('RGB',(width,100),'#232323')
                draw = ImageDraw.Draw(footer)
                draw.text((12,5),snapshot.get('reading_reason','模型初始化中'),font=self.small,fill='white')
                draw.text((12,37),('阅读模式：右挥下一页，左挥上一页。' if os.environ.get('AIR_GESTURE_READING_TARGET')=='reader' else '正式模式：挥手会发送左右翻页按键；不保存画面。'),font=self.small,fill='#cbd5e1')
                draw.text((12,69),'Q / Esc 关闭校准窗口，手势服务继续运行。',font=self.small,fill='#cbd5e1')
                self.header = cv2.cvtColor(np.array(header),cv2.COLOR_RGB2BGR)
                self.footer = cv2.cvtColor(np.array(footer),cv2.COLOR_RGB2BGR)
                self.last_panel = now
            display = np.vstack([self.header,image,self.footer])
            cv2.imshow(self.WINDOW, display)
            if self.first_frame:
                cv2.resizeWindow(self.WINDOW,640,720)
                self.first_frame = False
        key = cv2.waitKey(1)&255
        return key not in (27,ord('q'),ord('Q')) and cv2.getWindowProperty(self.WINDOW,cv2.WND_PROP_VISIBLE)>=1

    def close(self):
        cv2.destroyAllWindows()

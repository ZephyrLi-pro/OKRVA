"""
Copyright (C) 2026, KylinSoft Co., Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""

import asyncio
import threading
from typing import Optional

from dbus_next import Message
from dbus_next.aio import MessageBus


class AirGestureDBusClient:
    SERVICE_NAME = "org.ukui.AirGesture"
    OBJECT_PATH = "/org/ukui/AirGesture"
    INTERFACE_NAME = "org.ukui.AirGesture"

    def __init__(self):
        self._lock = threading.Lock()
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._thread: Optional[threading.Thread] = None
        self._bus: Optional[MessageBus] = None

    # 确保事件循环和线程正在运行
    def _ensure_runtime(self):
        with self._lock:
            if self._loop is not None and self._thread is not None and self._thread.is_alive():
                return

            loop = asyncio.new_event_loop()
            ready = threading.Event()

            def runner():
                asyncio.set_event_loop(loop)
                ready.set()
                loop.run_forever()

            thread = threading.Thread(target=runner, name="air-gesture-dbus", daemon=True)
            thread.start()
            ready.wait()

            self._loop = loop
            self._thread = thread

    # 获取D-Bus连接
    async def _get_bus(self) -> Optional[MessageBus]:
        if self._bus is not None:
            return self._bus

        bus = MessageBus()
        await bus.connect()

        self._bus = bus
        return self._bus

    # 调用D-Bus方法
    async def _call_async(self, method_name: str, signature: str = "", body: Optional[list] = None) -> None:
        if body is None:
            body = []

        bus = await self._get_bus()
        if bus is None:
            print("Failed to connect to D-Bus")
            return

        message = Message(
            destination=self.SERVICE_NAME,
            path=self.OBJECT_PATH,
            interface=self.INTERFACE_NAME,
            member=method_name,
            signature=signature,
            body=body,
        )

        await bus.call(message)

    def _post(self, method_name: str, signature: str = "", body: Optional[list] = None) -> None:
        self._ensure_runtime()
        if self._loop is None:
            print("Failed to create event loop for D-Bus client")
            return

        # 在子线程的事件循环中调用异步方法
        asyncio.run_coroutine_threadsafe(
            self._call_async(method_name, signature, body),
            self._loop,
        )

    def show_icon(self, palm_direction) -> None:
        self._post("showIcon", signature="s", body=[palm_direction])

    def hide_icon(self) -> None:
        self._post("hideIcon")

    def wheel(self, direction) -> None:
        self._post("wheel", signature="s", body=[direction])

    def horizontal(self, direction) -> None:
        self._post("horizontal", signature="s", body=[direction])

    def ptrsc(self) -> None:
        self._post("ptrsc")

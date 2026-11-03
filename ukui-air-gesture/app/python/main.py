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

from gesture_recognition import recognize_gesture

def main():
    import os
    profile = os.environ.get("AIR_GESTURE_PROFILE", "legacy")
    if profile == "reading":
        from reading_runtime import run
        run()
    elif profile == "legacy":
        recognize_gesture()
    else:
        raise ValueError("Unknown AIR_GESTURE_PROFILE: " + profile)

if __name__ == "__main__":
    main()
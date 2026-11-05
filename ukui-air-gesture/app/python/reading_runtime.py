"""Optional reading profile for the native ukui-air-gesture service.

The legacy posture classifier is unchanged. No camera frames are recorded.
"""
import collections
import datetime
import json
import os
import signal
import threading
import time
from pathlib import Path

from latest_frame import LatestFrame
from reading_dispatch import ReadingDispatcher
from reading_dbus import ReadingDBusActions
from reading_swipe import palm_sample
from reading_feedback import ReadingFeedback


def run():
    import cv2
    # ORT owns the model's configured worker threads. Small camera transforms
    # do not need another eight-worker OpenCV pool competing with those workers.
    cv2.setNumThreads(1)
    from gesture_recognition import (
        HandTracker, configure_camera_capture, get_hand_landmarks_from_frame,
        resolve_model_path, PALM_DETECTION_SCORE_THRESHOLD)
    from opencv_ort_backend import ORTPalmDet, ORTHandPose
    from virtual_camera import (
        activate_shared_camera, find_real_camera_node, open_loopback_writer)

    stopped = threading.Event()
    mailbox = LatestFrame()
    lock = threading.Lock()
    log_lock = threading.Lock()
    shared = {'snapshot': {}, 'error': None}
    event_path = os.environ.get('AIR_GESTURE_EVENT_LOG')
    counts = collections.Counter()
    seed_path = os.environ.get('AIR_GESTURE_COUNTER_SEED')
    if seed_path:
        for key, value in json.loads(Path(seed_path).read_text()).get('reading', {}).items():
            if type(value) is int and value >= 0:
                counts[key] = value

    def emit(event, **data):
        row = dict(event=event, time=datetime.datetime.now().isoformat(timespec='milliseconds'), **data)
        line = json.dumps(row, ensure_ascii=False)
        with log_lock:
            print(line, flush=True)
            if event_path:
                with Path(event_path).open('a', encoding='utf-8') as stream:
                    stream.write(line+'\n')

    def stop(*_):
        stopped.set()
        mailbox.close()

    def fail(error):
        with lock:
            shared['error'] = str(error)
        stop()

    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, stop)

    activate_shared_camera()
    camera = find_real_camera_node()
    if not camera:
        raise RuntimeError('No physical camera available')

    def capture():
        cap = cv2.VideoCapture(camera, cv2.CAP_V4L2)
        writer = None
        try:
            if not cap.isOpened():
                raise RuntimeError('Camera busy or unavailable')
            width, height, fps = configure_camera_capture(cap)
            # Keep the calibrated live preview responsive independently of the
            # slower model rate. The legacy capture setting remains unchanged.
            cap.set(cv2.CAP_PROP_FPS, 30)
            fps = cap.get(cv2.CAP_PROP_FPS)
            emit('reading_capture_config', width=width, height=height, fps=fps)
            writer = open_loopback_writer(width, height, fps)
            while not stopped.is_set():
                ok, frame = cap.read()
                if not ok:
                    raise RuntimeError('Camera disconnected or capture failed')
                mailbox.publish(frame, time.monotonic())
                if writer is not None and not writer.write(frame):
                    writer.close()
                    writer = None
        except Exception as error:
            fail(error)
        finally:
            if writer is not None:
                writer.close()
            cap.release()

    def infer():
        trace_stream=None
        try:
            palm_threads = int(os.environ.get('AIR_GESTURE_PALM_THREADS', '4'))
            pose_threads = int(os.environ.get('AIR_GESTURE_POSE_THREADS', '4'))
            if palm_threads not in (1,2,4) or pose_threads not in (1,2,4):
                raise ValueError('Gesture threads must be 1, 2 or 4')
            tracker = HandTracker(
                ORTPalmDet(resolve_model_path('palm_detection_mediapipe_2023feb.onnx'),
                    score_threshold=PALM_DETECTION_SCORE_THRESHOLD,
                    nms_threshold=.3, intra_op_num_threads=palm_threads),
                ORTHandPose(resolve_model_path('handpose_estimation_mediapipe_2023feb.onnx'),
                    confidence_threshold=.4, intra_op_num_threads=pose_threads))
            target = os.environ.get('AIR_GESTURE_READING_TARGET','native')
            if target == 'reader':
                from reader_actions import ReaderDBusActions
                actions = ReaderDBusActions()
            elif target == 'native':
                actions = ReadingDBusActions()
            else:
                raise ValueError('Unknown reading action target: '+target)
            if target == 'reader':
                from queued_reading import QueuedReadingDispatcher
                def confirmed(event, **data):
                    if event == 'gesture_recognized':
                        with lock:
                            counts['horizontal:'+data['direction']] += 1
                    emit(event, **data)
                dispatcher = QueuedReadingDispatcher(actions, confirmed)
            else:
                dispatcher = ReadingDispatcher(actions.horizontal, emit)
            emit('model_thread_config', palm_threads=palm_threads, pose_threads=pose_threads,
                 opencv_threads=cv2.getNumThreads())
            trace_path=os.environ.get('AIR_GESTURE_TRACE_LANDMARKS')
            if trace_path:
                try:
                    fd=os.open(trace_path,os.O_WRONLY|os.O_CREAT|os.O_APPEND,0o600)
                    trace_stream=os.fdopen(fd,'a',encoding='utf8')
                    emit('landmark_trace_started',path=trace_path,records_camera_frames=False)
                except OSError as error:
                    emit('landmark_trace_error',error=str(error))
            sequence, last_log, last_key = 0, 0., None
            while not stopped.is_set():
                latest = mailbox.wait_after(sequence)
                if latest is None:
                    continue
                sequence, timestamp, frame = latest
                start = time.monotonic()
                landmarks, handedness, mode = get_hand_landmarks_from_frame(frame, tracker, 0)
                if stopped.is_set():
                    break
                age = time.monotonic()-timestamp
                aspect = frame.shape[0]/frame.shape[1]
                direction = dispatcher.process(landmarks, timestamp, aspect, age=age)
                if direction:
                    counts['horizontal:'+direction] += 1
                features = palm_sample(landmarks, aspect)
                with lock:
                    current_counts = dict(counts)
                snapshot = dict(hand_present=landmarks is not None,
                    handedness=handedness, tracker_mode=mode,
                    reading_reason=dispatcher.controller.reason,
                    palm_center_x=features[0] if features else None,
                    palm_center_y=features[1] if features else None,
                    frame_time=timestamp, inference_seconds=time.monotonic()-start,
                    source_age_seconds=age, reading_counts=current_counts)
                snapshot.update(palm_model_runs=tracker.palm_runs,
                                pose_model_runs=tracker.pose_runs,
                                palm_search_skips=tracker.search_skips)
                snapshot.update(dispatcher.controller.diagnostics(timestamp))
                if hasattr(dispatcher,'diagnostics'):
                    snapshot.update(dispatcher.diagnostics())
                    if snapshot.get('page_navigation_busy') and snapshot.get('requested_page'):
                        snapshot['reading_reason']+='；正在翻到第'+str(snapshot['requested_page'])+'页'
                if trace_stream is not None:
                    row=dict(event='count_sample',time=datetime.datetime.now().isoformat(timespec='milliseconds'),
                        frame_time=timestamp,aspect=aspect,source_age=age,
                        landmarks=[dict(x=p.x,y=p.y,z=p.z) for p in landmarks] if landmarks is not None else None,
                        feature=features,phase=dispatcher.controller.phase,message=dispatcher.controller.reason,
                        fixed_counts=dict(dispatcher.controller.accepted_counts))
                    try:
                        trace_stream.write(json.dumps(row,ensure_ascii=False)+'\n');trace_stream.flush()
                    except OSError as error:
                        trace_stream.close();trace_stream=None
                        emit('landmark_trace_error',error=str(error))
                with lock:
                    shared['snapshot'] = dict(snapshot, landmarks=landmarks)
                key = (snapshot['hand_present'], snapshot['reading_reason'])
                now = time.monotonic()
                if key != last_key or now-last_log >= 2:
                    emit('recognition_state', **snapshot)
                    last_key, last_log = key, now
        except Exception as error:
            fail(error)
        finally:
            if trace_stream is not None:trace_stream.close()
            if 'dispatcher' in locals() and hasattr(dispatcher, 'close'):
                dispatcher.close()

    feedback = ReadingFeedback(emit)
    preview = None
    if os.environ.get('AIR_GESTURE_READING_PREVIEW') == '1':
        from reading_preview import ReadingPreview
        preview = ReadingPreview()
    capture_thread = threading.Thread(target=capture, daemon=True, name='reading-capture')
    infer_thread = threading.Thread(target=infer, daemon=True, name='reading-inference')
    emit('preview_started', mode='reading', reading_algorithm_revision='navigation_v11_stationary_home_recovery',
         camera=camera, records_camera_frames=False, executes_desktop_actions=True)
    capture_thread.start()
    infer_thread.start()
    try:
        while not stopped.wait(.015 if preview else .1):
            with lock:
                snapshot = dict(shared['snapshot'])
            feedback.update(snapshot)
            if preview:
                latest = mailbox.wait_after(0, timeout=0)
                if not preview.update(latest, snapshot):
                    preview.close()
                    preview = None
                    emit('calibration_closed', recognition_continues=True)
        if shared['error']:
            raise RuntimeError(shared['error'])
    finally:
        stop()
        feedback.close()
        capture_thread.join(timeout=3)
        infer_thread.join(timeout=3)
        if preview:
            preview.close()
        emit('preview_finished', mode='reading', reading_counts=dict(counts), legacy_counts={})

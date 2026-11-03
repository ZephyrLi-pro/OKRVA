"""ONNX Runtime adapters for the OpenCV Zoo MediaPipe hand models."""

import numpy as np
import onnxruntime as ort

from mp_handpose import MPHandPose
from mp_palmdet import MPPalmDet


class _ORTNet:
    """Small subset of cv.dnn.Net used by the OpenCV Zoo model helpers."""

    def __init__(self, model_path, intra_op_num_threads=4):
        options = ort.SessionOptions()
        options.intra_op_num_threads = intra_op_num_threads
        options.inter_op_num_threads = 1
        options.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
        options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        options.add_session_config_entry("session.intra_op.allow_spinning", "0")
        options.add_session_config_entry("session.inter_op.allow_spinning", "0")
        self._session = ort.InferenceSession(
            model_path,
            sess_options=options,
            providers=["CPUExecutionProvider"],
        )
        self._input_name = self._session.get_inputs()[0].name
        self._output_names = [item.name for item in self._session.get_outputs()]
        self._input = None

    def setPreferableBackend(self, _backend_id):
        pass

    def setPreferableTarget(self, _target_id):
        pass

    def setInput(self, value):
        self._input = np.ascontiguousarray(value, dtype=np.float32)

    def getUnconnectedOutLayersNames(self):
        return self._output_names

    def forward(self, output_names=None):
        if self._input is None:
            raise RuntimeError("Model input has not been set")
        names = list(output_names) if output_names else self._output_names
        return self._session.run(names, {self._input_name: self._input})


class ORTPalmDet(MPPalmDet):
    def __init__(self, model_path, nms_threshold=0.3, score_threshold=0.5,
                 top_k=5000, intra_op_num_threads=4):
        self.model_path = model_path
        self.nms_threshold = nms_threshold
        self.score_threshold = score_threshold
        self.topK = top_k
        self.backend_id = 0
        self.target_id = 0
        self.input_size = np.array([192, 192])
        self.model = _ORTNet(model_path, intra_op_num_threads)
        self.anchors = self._load_anchors()


class ORTHandPose(MPHandPose):
    def __init__(self, model_path, confidence_threshold=0.5,
                 intra_op_num_threads=2):
        self.model_path = model_path
        self.conf_threshold = confidence_threshold
        self.backend_id = 0
        self.target_id = 0
        self.input_size = np.array([224, 224])
        self.PALM_LANDMARK_IDS = [0, 5, 9, 13, 17, 1, 2]
        self.PALM_LANDMARKS_INDEX_OF_PALM_BASE = 0
        self.PALM_LANDMARKS_INDEX_OF_MIDDLE_FINGER_BASE = 2
        self.PALM_BOX_PRE_SHIFT_VECTOR = [0, 0]
        self.PALM_BOX_PRE_ENLARGE_FACTOR = 4
        self.PALM_BOX_SHIFT_VECTOR = [0, -0.4]
        self.PALM_BOX_ENLARGE_FACTOR = 3
        self.HAND_BOX_SHIFT_VECTOR = [0, -0.1]
        self.HAND_BOX_ENLARGE_FACTOR = 1.65
        self.model = _ORTNet(model_path, intra_op_num_threads)

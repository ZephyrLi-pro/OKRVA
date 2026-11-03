"""Internal, grammar-constrained model plans; never parse ordinary chat as code.

This module has no executor, network client, or natural-language keyword matcher.
The model selects an operation and its arguments. Deterministic code validates
the contract and the caller-supplied tool catalog, not the wording of the user.
"""
from __future__ import annotations

import json
import copy
import hashlib
import threading
import time
import uuid
from dataclasses import dataclass, field

PROTOCOL_VERSION = 'structured-intent-v7-balanced'


OPERATIONS = {
    'sys__set_volume': ('设置扬声器播放音量百分比，不改变静音。',
                       {'percent': {'type': 'integer', 'minimum': 0, 'maximum': 100}}),
    'sys__adjust_volume': ('增减扬声器音量百分点，正数增加、负数减少；一点默认10个百分点。',
                          {'delta': {'type': 'integer', 'minimum': -100, 'maximum': 100}}),
    'sys__set_mute': ('扬声器静音true，解除静音false；不改变音量数值，不是麦克风。',
                     {'muted': {'type': 'boolean'}}),
    'sys__open_application': ('打开文件管理器file_manager或系统设置system_settings。',
                             {'application': {'type': 'string', 'enum': ['file_manager', 'system_settings']}}),
    'sys__get_volume': ('只读查询扬声器音量与静音状态。', {}),
    'sys__get_brightness': ('只读查询屏幕亮度，不是设置亮度。', {}),
    'sys__wifi_status': ('只读查询无线网络状态。', {}),
    'sys__wifi_scan': ('只读列出缓存的可见无线网络。', {}),
    'sys__bluetooth_status': ('只读查询蓝牙状态。', {}),
    'sys__get_capabilities': ('只读查询已接入的系统能力。', {}),
}

def object_schema(properties):
    return {'type': 'object', 'properties': properties, 'required': list(properties), 'additionalProperties': False}


# One bounded native planning call is easier for small models than resuming an
# open-ended chat after every effect. This function is NEVER an MCP executor.
ACTION_MAP = {
    'set_volume': ('sys__set_volume', 'percent'),
    'increase_volume': ('sys__adjust_volume', 'delta'),
    'decrease_volume': ('sys__adjust_volume', 'delta'),
    'mute_speaker': ('sys__set_mute', None),
    'unmute_speaker': ('sys__set_mute', None),
    'open_file_manager': ('sys__open_application', None),
    'open_settings': ('sys__open_application', None),
    'query_volume': ('sys__get_volume', None),
    'query_brightness': ('sys__get_brightness', None),
    'wifi_status': ('sys__wifi_status', None),
    'wifi_scan': ('sys__wifi_scan', None),
    'bluetooth_status': ('sys__bluetooth_status', None),
    'get_capabilities': ('sys__get_capabilities', None),
}
UNSUPPORTED_ACTIONS = ('mute_microphone', 'unmute_microphone', 'set_microphone_volume', 'unsupported')
PLAN_SYSTEM = '''分析user_request，仅输出JSON完整计划，不执行，不回答用户问题。
先判断intent：要求立即操作是operate；聊天、解释、引用、教学、禁止代操作是chat，actions=[]。
日志分析、文档问答、文件摘要用chat，交给原来的聊天/文件处理入口。
按用户顺序列出全部动作，最多3步，不增加动作，不包含被否定的动作。“不要A，只做B”只列B。
set_volume是绝对目标百分比；increase/decrease_volume是相对增减百分点，一点默认10。
外放/喇叭=speaker；话筒/麦克风=microphone，不能互换。未支持功能选unsupported。
value为0到100整数；非数值动作填null。'''

ACTION_DESCRIPTIONS = {
    'set_volume': '把播放音量设为value%，包括调到/降到某个百分比；绝对目标，不是增量',
    'increase_volume': '播放音量增加value个百分点，一点默认10',
    'decrease_volume': '播放音量减少value个百分点，一点默认10',
    'mute_speaker': '播放设备/扬声器静音，value=null',
    'unmute_speaker': '解除播放设备/扬声器静音，value=null',
    'open_file_manager': '打开文件管理器/文件浏览器，value=null',
    'open_settings': '打开系统设置/控制面板，value=null',
    'query_volume': '查询实际播放音量，value=null',
    'query_brightness': '只读查询屏幕亮度，value=null',
    'wifi_status': '只读查询无线网络状态，value=null',
    'wifi_scan': '只读查询附近无线网络，value=null',
    'bluetooth_status': '只读查询蓝牙状态，value=null',
    'get_capabilities': '只读查询当前系统工具能力，value=null',
    'mute_microphone': '录音设备/麦克风/话筒静音；尚未接入，value=null',
    'unmute_microphone': '解除录音设备/话筒静音；尚未接入，value=null',
    'set_microphone_volume': '设定录音设备/话筒音量；尚未接入，value为目标百分比',
    'unsupported': '请求了其他未支持的系统功能，value=null',
}


def request(model, messages, names):
    # The semantic vocabulary is not the execution allowlist. Keep labels for
    # disabled capabilities so the decoder need not substitute a nearby tool.
    operations = list(ACTION_MAP) + list(UNSUPPORTED_ACTIONS)
    definition = object_schema({'intent': {'type': 'string', 'enum': ['chat', 'operate']},
        'actions': {'type': 'array', 'minItems': 0, 'maxItems': 3,
        'items': object_schema({'operation': {'type': 'string', 'enum': operations},
            'value': {'type': ['integer', 'null'], 'minimum': 0, 'maximum': 100}})}})
    # Balanced demonstrations explain the protocol, not a phrase-routing table.
    # Never compare the user's wording with these examples.
    examples = [
        ('把播放音量设成六成。', [('set_volume', 60)]),
        ('把外放声音减小五个百分点，再启动文件浏览器。', [('decrease_volume', 5), ('open_file_manager', None)]),
        ('不要动播放音量，只启动系统设置。', [('open_settings', None)]),
        ('文档里说“打开设置”，这句话是什么意思？', []),
        ('如何把声音设为七成？先说明步骤，不要替我设置。', []),
        ('请关闭话筒，保持喇叭原样。', [('mute_microphone', None)]),
    ]
    # response_format constrains the decoder; it does not teach the model what
    # operation labels mean. Supply the capability catalog in the prompt too.
    catalog = '\n'.join(op + ': ' + ACTION_DESCRIPTIONS[op] for op in operations)
    history = [{'role': 'system', 'content': PLAN_SYSTEM + '\n格式：'
        '{"intent":"chat或operate","actions":[{"operation":"下列操作名","value":整数或null}]}。\n操作目录：\n' + catalog}]
    for index, (text, steps) in enumerate(examples):
        if any(op not in operations for op, _ in steps):
            continue
        history += [{'role': 'user', 'content': json.dumps({'user_request':text}, ensure_ascii=False)},
            {'role': 'assistant', 'content': json.dumps({'intent': 'operate' if steps else 'chat',
                'actions': [{'operation': op, 'value': val} for op, val in steps]}, ensure_ascii=False)}]
    # Wrap input as data for intent analysis rather than a direct instruction
    # to the assistant. This does not extract or match any action words.
    context = copy.deepcopy(messages)
    if context and context[-1].get('role') == 'user' and isinstance(context[-1].get('content'), str):
        context[-1]['content'] = json.dumps({'user_request': context[-1]['content']}, ensure_ascii=False)
    return {'model': model, 'messages': history + context,
        'response_format': {'type': 'json_schema', 'json_schema': {
            'name': 'private_system_plan', 'strict': True, 'schema': definition}},
        'stream': False, 'temperature': 0, 'seed': 42,
        'repeat_penalty': 1, 'max_tokens': 160, 'cache_prompt': True,
        'chat_template_kwargs': {'enable_thinking': False}}


def decode_response(result, names):
    """Only the bound private constrained response; never parse ordinary chat."""
    choice = result['choices'][0]
    if choice.get('finish_reason') != 'stop' or choice['message'].get('tool_calls'):
        raise ValueError('Private constrained response missing or incomplete')
    raw = json.loads(choice['message']['content'])
    if (not isinstance(raw, dict) or set(raw) != {'intent', 'actions'}
            or raw['intent'] not in ('chat', 'operate')
            or not isinstance(raw['actions'], list) or len(raw['actions']) > 3):
        raise ValueError('Invalid action list')
    if raw['intent'] == 'chat':
        if raw['actions']:raise ValueError('Non-operating request contains actions')
        return {'mode': 'delegate', 'steps': [], 'message': ''}
    if not raw['actions']:raise ValueError('Empty operating plan')
    steps = []
    unavailable = False
    for action in raw['actions']:
        if not isinstance(action, dict) or set(action) != {'operation', 'value'}:
            raise ValueError('Invalid action')
        operation, value = action['operation'], action['value']
        if not isinstance(operation, str) or operation not in (*ACTION_MAP, *UNSUPPORTED_ACTIONS):
            raise ValueError('Unknown operation')
        numeric = operation in ('set_volume', 'increase_volume', 'decrease_volume', 'set_microphone_volume')
        if numeric:
            if type(value) is not int or not 0 <= value <= 100:
                raise ValueError('Invalid percentage')
            if operation in ('increase_volume', 'decrease_volume') and value == 0:
                raise ValueError('Ambiguous zero adjustment')
        elif value is not None:
            raise ValueError('Unexpected argument')
        if operation in UNSUPPORTED_ACTIONS:
            unavailable = True
            continue
        name, key = ACTION_MAP[operation]
        if name not in names:
            unavailable = True
            continue
        if key:
            arguments = {key: -value if operation == 'decrease_volume' else value}
        elif operation in ('mute_speaker', 'unmute_speaker'):
            arguments = {'muted': operation == 'mute_speaker'}
        elif operation in ('open_file_manager', 'open_settings'):
            arguments = {'application': 'file_manager' if operation == 'open_file_manager' else 'system_settings'}
        else:
            arguments = {}
        steps.append({'name': name, 'arguments': arguments})
    # Validate the ENTIRE plan before dispatching its first side effect.
    if unavailable:
        return {'mode': 'clarify', 'steps': [], 'message': '该请求包含当前未接入的系统能力，本次没有执行任何操作。'}
    plan = {'mode': 'execute' if steps else 'delegate', 'steps': steps, 'message': ''}
    return validate(plan, names)


def validate(value, names):
    if not isinstance(value, dict) or set(value) != {'mode', 'steps', 'message'}:
        raise ValueError('Invalid plan envelope')
    mode, steps, message = (value[k] for k in ('mode', 'steps', 'message'))
    if mode not in ('execute', 'delegate', 'clarify') or not isinstance(message, str) or len(message) > 300:
        raise ValueError('Invalid plan mode/message')
    if not isinstance(steps, list) or len(steps) > 3:
        raise ValueError('Invalid plan size')
    if mode != 'execute':
        if steps or (mode == 'clarify' and not message.strip()):
            raise ValueError('Non-executable plan contains actions or no clarification')
        return value
    if not steps or message:
        raise ValueError('Executable plan requires bounded actions only')
    if len({json.dumps(s, sort_keys=True) for s in steps}) != len(steps):
        raise ValueError('Duplicate operations are not automatically repeated')
    for step in steps:
        if not isinstance(step, dict) or set(step) != {'name', 'arguments'}:
            raise ValueError('Invalid plan step')
        name, args = step['name'], step['arguments']
        if not isinstance(name, str) or name not in names or name not in OPERATIONS or not isinstance(args, dict):
            raise ValueError('Unpermitted operation')
        rules = OPERATIONS[name][1]
        if set(args) != set(rules):
            raise ValueError('Missing/extra arguments')
        for key, rule in rules.items():
            actual = args[key]
            kind = {'integer': int, 'boolean': bool, 'string': str}[rule['type']]
            if type(actual) is not kind:
                raise ValueError('Wrong argument type')
            if ('minimum' in rule and actual < rule['minimum'] or 'maximum' in rule and actual > rule['maximum']
                    or 'enum' in rule and actual not in rule['enum']):
                raise ValueError('Argument out of range')
    return value


STOP = '模型计划或工具结果未确认，已停止后续操作；没有自动重试。'


def permitted_catalog(value, allowed):
    """Whitelist capabilities/schema constraints, never match user sentences."""
    result = {}
    for tool in value.get('tools', []):
        if not isinstance(tool, dict) or tool.get('type') != 'function':
            continue
        function = tool.get('function', {})
        name = function.get('name')
        if name not in allowed or name not in OPERATIONS:
            continue
        if name in result:
            raise ValueError('Duplicate tool schema')
        params = function.get('parameters', {})
        keys = set(OPERATIONS[name][1])
        if (params.get('type') != 'object' or set(params.get('properties', {})) != keys
                or set(params.get('required', [])) != keys
                or set(params) - {'type', 'properties', 'required', 'additionalProperties', 'description', 'title'}):
            continue
        for key, rule in params['properties'].items():
            if (not isinstance(rule, dict) or rule.get('type') != OPERATIONS[name][1][key]['type']
                    or set(rule) - {'type', 'minimum', 'maximum', 'enum', 'description', 'title', 'default'}):
                break
        else:
            result[name] = copy.deepcopy(params)
    return result


def validate_catalog(plan, catalog):
    validate(plan, catalog)
    for step in plan['steps']:
        for key, value in step['arguments'].items():
            rule = catalog[step['name']]['properties'][key]
            for bound, test in (('minimum', lambda n: value < n), ('maximum', lambda n: value > n)):
                if bound in rule and (type(rule[bound]) not in (int, float) or test(rule[bound])):
                    raise ValueError('Caller schema bound violated')
            if 'enum' in rule and (not isinstance(rule['enum'], list) or not any(
                    type(item) is type(value) and item == value for item in rule['enum'])):
                raise ValueError('Caller schema enum violated')


@dataclass
class PlanState:
    binding: str
    steps: list
    expires: float
    issued: set = field(default_factory=set)
    failed: bool = False


class SemanticPlanner:
    """Model plans become native calls; KylinBot alone authorizes/executes them.

    Model JSON is accepted ONLY from the private constrained inference response,
    never from user/assistant prose. Opaque issued call IDs bind continuations to
    the original conversation and catalog. Restart/expiry/tampering fail closed.
    Only in-memory plans are retained; no private conversation is logged/saved.
    """
    def __init__(self, clock=time.monotonic, ttl=600, capacity=64, latest_only=False):
        self.clock, self.ttl, self.capacity = clock, ttl, capacity
        self.lock = threading.RLock()
        self.states = {}
        self.latest_only = latest_only

    def handle(self, body, model, allowed, infer, cancelled=lambda: False):
        try:
            value = json.loads(body)
            if not isinstance(value, dict) or value.get('tool_choice') not in (None, 'auto'):
                return None
            messages = value.get('messages')
            if not isinstance(messages, list) or not all(isinstance(m, dict) for m in messages):
                return None
            starts = [i for i, m in enumerate(messages) if m.get('role') == 'user']
            if not starts:
                return None
            start = starts[-1]
            prefix, tail = messages[:start + 1], messages[start + 1:]
            catalog = permitted_catalog(value, allowed)
            binding = hashlib.sha256(json.dumps([model, catalog, [m for m in prefix if m.get('role') != 'system']],
                                     sort_keys=True, ensure_ascii=False).encode()).hexdigest()
            if tail:
                # Non-planner tools/ordinary chats keep their original model path.
                ids = [c.get('id', '') for m in tail for c in m.get('tool_calls', []) if isinstance(c, dict)]
                own = [c for c in ids if isinstance(c, str) and c.startswith('call_rvai_')]
                return self.resume(binding, tail, own[0], catalog) if own else None
            if not catalog or not isinstance(prefix[-1].get('content'), str):
                return None
            # Grounded inference uses current evidence only. Keep binding above
            # over the ORIGINAL history, so replay/tampering checks do not weaken.
            context = [copy.deepcopy(prefix[-1])] if self.latest_only else [copy.deepcopy(m) for m in prefix if m.get('role') != 'system']
            if len(json.dumps(context, ensure_ascii=False)) > 6000:
                return None
            # This strips frontend framing only, NOT action words/intent rules.
            from .backends import _system_command_text
            text = _system_command_text(context[-1]['content'])
            if text:
                context[-1]['content'] = text
            if cancelled():
                raise ConnectionResetError('Planner client cancelled')
            result = infer(request(model, context, list(catalog)))
            if cancelled():
                raise ConnectionResetError('Planner client cancelled')
            plan = decode_response(result, catalog)
            validate_catalog(plan, catalog)
            if plan['mode'] == 'delegate':
                return None
            if plan['mode'] == 'clarify':
                return {'content': '尚未执行操作：' + plan['message'], 'route': 'model-system-plan'}
            with self.lock:
                self.states = {k: s for k, s in self.states.items() if s.expires > self.clock()}
                if len(self.states) >= self.capacity:
                    raise ValueError('Planner capacity reached')
                nonce = uuid.uuid4().hex
                state = PlanState(binding, copy.deepcopy(plan['steps']), self.clock() + self.ttl)
                self.states[nonce] = state
                return self.dispatch(nonce, state, 0)
        except (ValueError, TypeError, KeyError, IndexError, AttributeError):
            return {'content': STOP, 'route': 'model-system-plan'}

    def dispatch(self, nonce, state, index):
        if index in state.issued or state.failed:
            return {'content': STOP, 'route': 'model-system-plan'}
        state.issued.add(index)
        return dict(state.steps[index], id=f'call_rvai_{nonce}_{index}', route='model-system-plan')

    def resume(self, binding, tail, identifier, catalog):
        with self.lock:
            state = None
            try:
                nonce = identifier.split('_')[2]
                state = self.states[nonce]
                if state.failed or state.binding != binding or state.expires <= self.clock() or len(tail) % 2 or len(tail) > 2 * len(state.steps):
                    raise ValueError('Expired or changed plan context')
                validate_catalog({'mode': 'execute', 'steps': state.steps, 'message': ''}, catalog)
                summaries = []
                for i in range(len(tail) // 2):
                    assistant, result = tail[2*i:2*i+2]
                    calls = assistant.get('tool_calls')
                    call_id = f'call_rvai_{nonce}_{i}'
                    if (i not in state.issued or assistant.get('role') != 'assistant' or result.get('role') != 'tool'
                            or not isinstance(calls, list) or len(calls) != 1 or calls[0].get('id') != call_id
                            or result.get('tool_call_id') != call_id or calls[0]['function']['name'] != state.steps[i]['name']):
                        raise ValueError('Native chain/ID/order mismatch')
                    actual = calls[0]['function']['arguments']
                    actual = json.loads(actual) if isinstance(actual, str) else actual
                    if json.dumps(actual, sort_keys=True) != json.dumps(state.steps[i]['arguments'], sort_keys=True):
                        raise ValueError('Arguments changed')
                    output = json.loads(result['content'])
                    blocks = output.get('content')
                    if (output.get('isError') is not False or not isinstance(blocks, list) or not blocks
                            or not all(isinstance(b, dict) and b.get('type') == 'text' and isinstance(b.get('text'), str)
                                       and b['text'].strip() for b in blocks)):
                        raise ValueError('Tool failed or unconfirmed')
                    text = '\n'.join(b['text'].strip() for b in blocks)
                    if len(text) > 2000:
                        raise ValueError('Oversized tool result')
                    summaries.append(text)
                index = len(tail) // 2
                if index == len(state.steps):
                    content='\n'.join(summaries)
                    if self.latest_only and all(step['name'] not in (
                            'sys__set_volume','sys__adjust_volume','sys__set_mute','sys__open_application') for step in state.steps):
                        content='仅查询，未修改任何设置。\n'+content
                    return {'content': content, 'route': 'model-system-plan'}
                return self.dispatch(nonce, state, index)
            except (ValueError, TypeError, KeyError, IndexError, AttributeError):
                if state is not None and state.binding == binding:
                    state.failed = True
                return {'content': STOP, 'route': 'model-system-plan'}

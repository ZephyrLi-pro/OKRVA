"""Ground model-selected arguments in user evidence, never route commands.

The model chooses every operation. This module normalizes measurements and
validates source spans, units and domain object identity. It has no executor.
"""
from __future__ import annotations
import copy
import json
import re

from .semantic_planner import (ACTION_DESCRIPTIONS, ACTION_MAP, UNSUPPORTED_ACTIONS,
    SemanticPlanner, object_schema, permitted_catalog)

PROTOCOL_VERSION = 'evidence-units-v9-query-objects'
_DIGITS = dict(zip('零〇一二两三四五六七八九', (0,0,1,2,2,3,4,5,6,7,8,9)))
_ATOM = r'(?:-?\d+(?:\.\d+)?|负?[零〇一二两三四五六七八九十百]+(?:点[零〇一二两三四五六七八九]+)?)'
_BOUNDARY = '，,。；;！？!?\n'
_CAPTURE = ('麦克风', '话筒', '录音', '录音输入', '输入音量', '采集')
_PLAYBACK = ('扬声器', '喇叭', '外放', '播放', '输出音量')
_APPS = {'open_file_manager': ('文件管理器','文件浏览器','文件管理','文件浏览'),
         'open_settings': ('系统设置','设置界面','控制面板','设置')}
_NEGATION = ('不','别','禁止','无需')
_INFORMATION = ('如何','怎么','什么意思','是什么意思','解释','方法','日志里','文档里','示例')
_SEQUENCE = ('然后','接着','随后','再')
_QUERY_OBJECTS = {
    'query_brightness': ('亮度','背光'),
    'wifi_status': ('wifi','wi-fi','无线','网络'),
    'wifi_scan': ('wifi','wi-fi','无线','网络'),
    'bluetooth_status': ('蓝牙','bluetooth'),
    'get_capabilities': ('能力','功能','工具','支持','能做什么'),
}


class GroundingError(ValueError):
    pass


def number(text):
    if text.startswith('负'):return -number(text[1:])
    if re.fullmatch(r'-?\d+(?:\.\d+)?', text):
        value=float(text)
        return int(value) if value.is_integer() else value
    if '点' in text:
        a,b=text.split('点',1)
        return float(str(number(a))+'.'+''.join(str(_DIGITS[x]) for x in b))
    if '百' in text:
        a,b=text.split('百',1)
        return (number(a) if a else 1)*100+(number(b.lstrip('零〇')) if b.lstrip('零〇') else 0)
    if '十' in text:
        a,b=text.split('十',1)
        return (number(a) if a else 1)*10+(number(b) if b else 0)
    return int(''.join(str(_DIGITS[x]) for x in text))


def measurements(text):
    """Only measurement/entity normalization; no action verbs are inspected."""
    found=[]
    patterns=[
        (rf'百分之({_ATOM})','percent'),
        (rf'({_ATOM})\s*[%％]','percent'),
        (rf'({_ATOM})\s*(?:个百分点|百分点|个点)','points'),
        (rf'({_ATOM})成(半)?','tenths'),
        (r'一半','half'),
        (r'一点(?![零〇一二两三四五六七八九])','vague'),
        (_ATOM,'bare'),
    ]
    for pattern,unit in patterns:
        for match in re.finditer(pattern,text):
            if any(match.start()<item['end'] and match.end()>item['start'] for item in found):
                continue
            value=10 if unit=='vague' else 50 if unit=='half' else number(match.group(1) if match.lastindex else match.group())
            if unit=='tenths':value=value*10+(5 if match.group(2) else 0)
            found.append(dict(start=match.start(),end=match.end(),text=match.group(),
                value=value,unit='percent' if unit in ('tenths','half') else unit))
    return [dict(item,id='q'+str(i)) for i,item in enumerate(sorted(found,key=lambda x:x['start']))]


def segments(text):
    result=[]
    for part in re.finditer(r'[^，,。；;！？!?\n]+',text):
        value=part.group();offset=part.start()
        # Sequence connectives segment tasks; they never select an operation.
        # Preserve a negation's scope if punctuation has not separated it.
        cuts=[0]
        if not any(term in value for term in _NEGATION):
            cuts += [m.start() for m in re.finditer(r'然后|接着|随后',value) if m.start()>0]
        cuts.append(len(value))
        for a,b in zip(cuts,cuts[1:]):
            chunk=value[a:b];left=len(chunk)-len(chunk.lstrip());right=len(chunk.rstrip())
            if chunk.strip():result.append((offset+a+left,offset+a+right))
    return result


def clause(text,start,end):
    for left,right in segments(text):
        if start>=left and end<=right:return left,right
    raise GroundingError('source必须对应单个原文片段，不要把多个步骤混为一个来源。')


def source_map(question):
    return {'s'+str(i):question[a:b] for i,(a,b) in enumerate(segments(question))}


def model_view(question,items):
    """Do not expose numeric VALUES to a model that only selects operations.

    Canonical values stay in trusted code; a model references q0/s0 symbols.
    Equal command structure with different valid values has an equal request.
    The original user/history sent to KylinBot is NEVER rewritten.
    """
    def mask(start,end):
        value=question[start:end]
        for item in reversed(items):
            if item['start']>=start and item['end']<=end:
                a=item['start']-start;b=item['end']-start
                value=value[:a]+'<'+item['id']+':'+item['unit']+'>'+value[b:]
        return value
    return {'user_request':mask(0,len(question)),
        'sources':[{'id':'s'+str(i),'text':mask(a,b)} for i,(a,b) in enumerate(segments(question))],
        'measurements':[{'id':item['id'],'unit':item['unit']} for item in items]}


def _source(question,source):
    if not isinstance(source,str) or not source.strip() or len(source)>600 or question.count(source)!=1:
        raise GroundingError('source必须是用户原文中唯一的原样片段，不要改写或省略对象。')
    start=question.index(source);end=start+len(source)
    left,right=clause(question,start,end);scope=question[left:right]
    if any(term in scope for term in _NEGATION):
        raise GroundingError('该动作的来源子句含否定要求，不能执行；只保留明确要求执行的子句。')
    if any(term in scope for term in _INFORMATION):
        raise GroundingError('来源片段是方法/说明/日志语境，不能作为设备操作授权。')
    # A quoted/logged instruction is evidence, not automatic authorization.
    for opening,closing in (('“','”'),('「','」'),('"','"')):
        position=0
        while position<len(question):
            pos=question.find(opening,position)
            if pos<0:break
            finish=question.find(closing,pos+1)
            if finish<0:break
            if start<finish and end>pos+1:
                raise GroundingError('不能把引号内的示例或日志直接视作操作授权。')
            position=finish+1
    return start,end,left,right


def _target_relation(question,start,item):
    """Check the proposed target's assignment relation, not an action route.

    A percentage following 'to/as' is a target, while a relative proportion
    is not a percentage-point delta. Ambiguous relations are refused.
    """
    prefix=question[start:item['start']].strip()
    if prefix and prefix[-1] in '到为成至':return True
    reduced=prefix
    for noun in sorted((*_CAPTURE,*_PLAYBACK,'音量','声音'),key=len,reverse=True):
        reduced=reduced.replace(noun,'')
    if reduced.rstrip().endswith(('设置','设定','设')):return True
    # An explicit slider-style argument such as 音量50%, not a state claim
    # such as 当前音量50% or a relative expression such as 提高20%.
    return not reduced.strip() and bool(prefix)


def _quantity(ref,items,start,end,operation,left=None,right=None,question=''):
    scoped=[item for item in items if item['start']>=start and item['end']<=end]
    relative=operation in ('increase_volume','decrease_volume')
    if ref is None:
        if not relative:raise GroundingError('绝对音量必须引用原文中的数值。')
        scope_items=[item for item in items if item['start']>=(left if left is not None else start)
            and item['end']<=(right if right is not None else end)]
        if any(item['unit']!='vague' for item in items):
            raise GroundingError('原文有明确数值，不能省略它并套用默认增量。')
        return 10
    item=next((item for item in items if item['id']==ref),None)
    if item is None or item not in scoped:raise GroundingError('quantity必须位于本动作的source片段内。')
    if type(item['value']) is not int or not 0<=item['value']<=100:
        raise GroundingError('当前工具仅支持0到100的整数百分比；不能舍入或猜测。')
    if relative and item['unit'] not in ('points','vague'):
        raise GroundingError('增减工具需要points百分点，原文单位是'+item['unit']+'。请重新判断原文：绝对目标可用set_volume；相对比例变化暂不支持，须用unsupported澄清，不能把比例换成百分点或绝对设定。')
    if not relative and item['unit'] not in ('percent','bare'):
        raise GroundingError('设定工具需要百分比目标，不能把百分点变化当成目标。')
    if operation=='set_volume' and not _target_relation(question,start,item):
        raise GroundingError('百分比的目标赋值关系未确认，不能将相对比例或背景状态改成绝对设定；请澄清。')
    return item['value']


def grounded_actions(result,question,items,enforce_coverage=True,source_ids_only=False):
    choice=result['choices'][0]
    if choice.get('finish_reason')!='stop' or choice['message'].get('tool_calls'):
        raise GroundingError('规划输出不完整。')
    value=json.loads(choice['message']['content'])
    if not isinstance(value,dict) or set(value)!={'intent','actions'} or value['intent'] not in ('chat','operate'):
        raise GroundingError('计划格式错误。')
    actions=value['actions']
    if not isinstance(actions,list) or len(actions)>3:
        raise GroundingError('动作列表错误。')
    if value['intent']=='chat' and actions:
        raise GroundingError('intent=chat不能带动作；明确要求操作才为operate，教学/解释必须chat且actions为空。')
    if value['intent']=='operate' and not actions:raise GroundingError('操作计划不能为空。')
    output=[];resolved=[];sources=source_map(question);seen=set();last_start=-1
    for original in actions:
        action=copy.deepcopy(original)
        if not isinstance(action,dict) or set(action)!={'operation','source','quantity'}:
            raise GroundingError('每步需要operation、source、quantity。')
        if source_ids_only and action['source'] not in sources:
            raise GroundingError('source必须引用sources中的id。')
        if isinstance(action['source'],str) and action['source'] in sources:
            action['source']=sources[action['source']]
        resolved.append(action)
        op=action['operation'];ref=action['quantity']
        if not isinstance(op,str) or op not in (*ACTION_MAP,*UNSUPPORTED_ACTIONS):
            raise GroundingError('未知操作名。')
        start,end,left,right=_source(question,action['source']);scope=question[left:right]
        identity=(op,action['source'],ref)
        if identity in seen:raise GroundingError('不能重复同一个来源的同一个动作来凑步骤数。')
        if start<last_start:raise GroundingError('步骤必须遵循原文来源的顺序，不能颠倒执行。')
        seen.add(identity);last_start=start
        audio=op in ('set_volume','increase_volume','decrease_volume','mute_speaker','unmute_speaker','query_volume')
        capture=any(term in scope for term in _CAPTURE)
        playback=any(term in scope for term in _PLAYBACK)
        inherited_audio=(any(term in question for term in (*_PLAYBACK,'音量','声音'))
            and not any(term in question for term in _CAPTURE)
            and not any(term in scope for term in ('亮度','屏幕','温度','麦克风','话筒','WiFi','蓝牙',
                *_APPS['open_file_manager'],*_APPS['open_settings'])))
        if audio and not (capture or playback or '音量' in scope or '声音' in scope or inherited_audio):
            raise GroundingError('来源子句没有明确的音频对象，不能猜测为扬声器操作。')
        if op in ('mute_speaker','unmute_speaker') and not any(term in scope for term in ('静音','无声','有声','关闭声音','关掉声音','声音关掉','声音关闭')):
            raise GroundingError('来源子句没有静音状态证据，不能擅自切换静音状态。')
        source_playback=any(term in action['source'] for term in _PLAYBACK)
        source_capture=any(term in action['source'] for term in _CAPTURE)
        if audio and capture and not (source_playback and not source_capture):
            raise GroundingError('source指向录音/麦克风，不能调用扬声器播放工具；若请求不支持请用对应microphone标签或unsupported。')
        if op in _QUERY_OBJECTS and not any(term in scope.casefold() for term in _QUERY_OBJECTS[op]):
            raise GroundingError('查询对象与source不一致，不能把其他设备的状态当作本次查询结果。')
        if 'microphone' in op and not capture:
            raise GroundingError('source未明确指向录音设备，不能换成麦克风。')
        if op in _APPS:
            if not any(alias in scope for alias in _APPS[op]):raise GroundingError('应用对象与source不一致。')
            ambiguous_settings=op=='open_settings' and not any(alias in action['source'] for alias in _APPS[op][:-1])
            if ambiguous_settings and ('音量' in action['source'] or action['source'].strip()=='设置') and ('音量' in scope or capture or playback):
                raise GroundingError('应用的source混入了音量指令，请引用其独立的原文片段。')
        numeric=op in ('set_volume','increase_volume','decrease_volume','set_microphone_volume')
        amount=_quantity(ref,items,start,end,op,left,right,question) if numeric else None
        if not numeric and ref is not None:raise GroundingError('非数值操作的quantity必须为null。')
        output.append({'operation':op,'value':amount})
    if enforce_coverage and actions and not any(action['operation'] in UNSUPPORTED_ACTIONS for action in actions):
        choices=action_schema(question,items)
        numeric_ops={'set_volume','increase_volume','decrease_volume'}
        required_refs={ref for choice in choices if choice['properties']['operation']['const'] in numeric_ops
            for ref in choice['properties']['quantity']['enum'] if ref is not None}
        used_refs={action['quantity'] for action in resolved}
        required_sources={sources[choice['properties']['source']['const']] for choice in choices
            if sources[choice['properties']['source']['const']].startswith(_SEQUENCE)}
        used_sources={action['source'] for action in resolved}
        missing_refs=required_refs-used_refs;missing_sources=required_sources-used_sources
        if missing_refs or missing_sources:
            missing=sorted(missing_refs)+[key for key,text in sources.items() if text in missing_sources]
            raise GroundingError('计划未覆盖以下参数或后续片段：'+json.dumps(missing,ensure_ascii=False,separators=(',',':'))
                +'。核对是否用户要求的剩余步骤；不得执行遗漏任务前面的半个计划。不是操作要求或无法确定时请unsupported澄清，不要擅自增加动作。')
    return {'intent':value['intent'],'actions':output}


SYSTEM='''只分析user_request，输出JSON，不执行。
普通聊天、教学、文件检索与摘要intent=chat且actions=[]，交给原智能体。
解释报错或阅读资料用analyze。实际系统操作、查询设备状态用operate；按原文顺序列全动作，最多3步。
每步operation选目录，source引用sources的s编号，quantity引用measurements的q编号；非数值动作填null。
数值由代码绑定，模型不计算。percent是绝对目标，points是增减百分点；一点默认10。
不执行否定、引用或教学中的动作。录音/麦克风不能替换为扬声器；未支持能力用对应microphone或unsupported。
只输出规定JSON，不复述问题。操作目录：
'''


def action_schema(question,items):
    """Compile VALID argument domains, not a selected action or execution.

    Every candidate must satisfy the same validator used after inference.
    No full user sentence is mapped to an action. Assignment markers validate
    a proposed argument relation only. The model still chooses intent,
    operations, ordering and whether a task is unsupported.
    """
    choices=[]
    sources=source_map(question)
    for op in (*ACTION_MAP,*UNSUPPORTED_ACTIONS):
        for identifier,source in sources.items():
            refs=[]
            for ref in [item['id'] for item in items]+[None]:
                candidate={'intent':'operate','actions':[{'operation':op,'source':source,'quantity':ref}]}
                result={'choices':[{'finish_reason':'stop','message':{'content':json.dumps(candidate,ensure_ascii=False,separators=(',',':'))}}]}
                try:grounded_actions(result,question,items,enforce_coverage=False)
                except (ValueError,TypeError,KeyError,IndexError):continue
                refs.append(ref)
            if refs:
                choices.append(object_schema({'operation':{'const':op},'source':{'const':identifier},'quantity':{'enum':refs}}))
    return choices


def request(model,question,items,feedback=None,previous=None,document_tasks=False,report_cleanup=False):
    ops=list(ACTION_MAP)+list(UNSUPPORTED_ACTIONS)
    choices=action_schema(question,items)
    branches=[object_schema({'intent':{'const':'chat'},'actions':{'type':'array','maxItems':0,'items':{}}})]
    if choices:
        # A model may stop after the first clause even when a repair message
        # names the omission. Require space for every explicit parameter/source
        # in the grammar as well. This chooses NO operation: post-validation
        # still checks coverage and refuses duplicates or unsupported plans.
        sources=source_map(question)
        required_sources={choice['properties']['source']['const'] for choice in choices
            if sources[choice['properties']['source']['const']].startswith(_SEQUENCE)
            or (choice['properties']['operation']['const'] in ('set_volume','increase_volume','decrease_volume')
                and any(ref is not None for ref in choice['properties']['quantity']['enum']))}
        minimum=min(3,max(1,len(required_sources)))
        branches.append(object_schema({'intent':{'const':'operate'},'actions':{'type':'array','minItems':minimum,'maxItems':3,'items':{'anyOf':choices}}}))
    branches.append(object_schema({'intent':{'const':'analyze'},'actions':{'type':'array','maxItems':0,'items':{}}}))
    from .document_workflow import task_schema
    document_definition=task_schema(question,items) if document_tasks else None
    if document_definition:branches.append(document_definition)
    from .report_cleanup import task_schema as cleanup_schema
    cleanup_definition=cleanup_schema(question) if report_cleanup else None
    if cleanup_definition and not document_definition:
        from .report_cleanup import request_plan,filenames
        view=model_view(question,items);view['explicit_report_names']=len(filenames(question))
        return request_plan(model,view,cleanup_definition,feedback,previous)
    if cleanup_definition:branches.append(cleanup_definition)
    definition={'anyOf':branches}
    instructions=SYSTEM
    if document_definition:
        instructions=instructions.replace('普通聊天、教学、文件检索与摘要intent=chat且actions=[]，交给原智能体。',
            '普通聊天、教学intent=chat且actions=[]。当前已接入最近文档摘要能力，明确要求查找最近修改的N份文档、分别摘要并保存桌面时，intent=documents，不是未接入能力，不要用unsupported或chat。')
        instructions+='\n文档任务格式：{"intent":"documents","directory":"documents","count":"数量引用q0","destination":"desktop","order":"modified_desc"}。count引用原文份数ID，不生成数字。KylinBot随后逐步调用真实文档工具。\n'
    if cleanup_definition:
        instructions+='\n桌面已有摘要的查看或清理才用intent=report_cleanup，selection=preview/all/names。读取源文档并形成新摘要、保存到桌面用intent=documents。不要把输出位置当作待清理的输入文件。当前两个能力同时可用，请按本轮完整请求选择；不增加删除操作。\n'
    # The grammar already restricts these operation labels. Explain exactly
    # that same domain; argument validation still chooses no operation.
    visible_ops={choice['properties']['operation']['const'] for choice in choices}
    messages=[{'role':'system','content':instructions+'\n'.join(
        op+': '+ACTION_DESCRIPTIONS[op] for op in ops if op in visible_ops)}]
    examples=[
        ('把外放设为六成。',[('set_volume','把外放设为六成','q0')]),
        ('把声音减小五个百分点，然后打开设置。',[('decrease_volume','把声音减小五个百分点','q0'),('open_settings','然后打开设置',None)]),
        ('如何把音量设成七成？先讲方法，不要代操作。',[]),
        ('把录音输入音量设为45%。',[('set_microphone_volume','把录音输入音量设为45%','q0')]),
        ('不要更改扬声器，只启动系统设置。',[('open_settings','只启动系统设置',None)]),
        ('现在外放声音是多少？',[('query_volume','现在外放声音是多少',None)]),
    ]
    # A documents-only grammar has no audio/application operation. Keep its
    # non-execution counterexample, without teaching unrelated system actions.
    mandatory=(2,) if document_definition and visible_ops <= {'unsupported'} else (0,1,2)
    examples=[entry for index,entry in enumerate(examples)
        if index in mandatory or any(op in visible_ops for op,_,_ in entry[1])]
    for text,actions in examples:
        reverse_sources={value:key for key,value in source_map(text).items()}
        messages.extend([{'role':'user','content':json.dumps(model_view(text,measurements(text)),ensure_ascii=False,separators=(',',':'))},
            {'role':'assistant','content':json.dumps({'intent':'operate' if actions else 'chat','actions':[
                {'operation':op,'source':reverse_sources[source],'quantity':ref} for op,source,ref in actions]},ensure_ascii=False,separators=(',',':'))}])
    example='帮我分析刚才报错的原因。'
    messages.extend([{'role':'user','content':json.dumps(model_view(example,measurements(example)),ensure_ascii=False,separators=(',',':'))},
        {'role':'assistant','content':'{"intent":"analyze","actions":[]}'}])
    if document_definition:
        example='将资料目录最新修改的两篇资料分别概括，存到桌面。'
        messages.extend([{'role':'user','content':json.dumps(model_view(example,measurements(example)),ensure_ascii=False,separators=(',',':'))},
            {'role':'assistant','content':json.dumps({'intent':'documents','directory':'documents','count':'q0','destination':'desktop','order':'modified_desc'})}])
    view=model_view(question,items)
    messages.append({'role':'user','content':json.dumps(view,ensure_ascii=False,separators=(',',':'))})
    if feedback:
        if previous is not None:messages.append({'role':'assistant','content':previous})
        messages.append({'role':'user','content':json.dumps(dict(view,validation_error=feedback,
            instruction='上次计划尚未执行。修正完整计划；无法确定就unsupported澄清，不得伪造来源或添加未要求的动作。'),ensure_ascii=False,separators=(',',':'))})
    return dict(model=model,messages=messages,response_format={'type':'json_schema','json_schema':{
        'name':'evidence_system_plan','strict':True,'schema':definition}},stream=False,temperature=0,seed=42,
        max_tokens=260,repeat_penalty=1,cache_prompt=True,chat_template_kwargs={'enable_thinking':False})


class GroundedPlanner:
    """Reuse native sequential state machine; validate all arguments first."""
    def __init__(self):
        self.base=SemanticPlanner(latest_only=True)
        from .document_workflow import DocumentWorkflow
        self.documents=DocumentWorkflow()
        from .report_cleanup import ReportCleanup
        self.reports=ReportCleanup()

    def handle(self,body,model,allowed,infer,cancelled=lambda:False,document_tasks=False):
        resumed=self.reports.resume(body,model,allowed,enabled=document_tasks,cancelled=cancelled)
        if resumed is not None:return resumed
        resumed=self.documents.resume(body,model,allowed,infer,cancelled,enabled=document_tasks)
        if resumed is not None:return resumed
        decline=[];selected=[];cleanup_selected=[];analysis_selected=[]
        try:
            value=json.loads(body)
            last=next(m for m in reversed(value.get('messages',[])) if m.get('role')=='user')
            from .backends import _system_command_text
            question=_system_command_text(last.get('content'))
            if not question:return None
            from .reference_context import parse_reference
            reference=parse_reference(last.get('content'))
            if reference is not None:
                # The same constrained model plan selects analysis vs operation.
                # Reference data never enters its authority/evidence domain.
                question=reference['question']
                if (value.get('messages',[])[-1].get('role')=='user'
                        and value.get('tool_choice') in (None,'auto')
                        and not permitted_catalog(value,allowed)
                        and not (document_tasks and self.documents.available(value,allowed))):
                    return {'read_only_analysis':True,'route':'model-read-only-analysis'}
            items=measurements(question)
        except (ValueError,TypeError,KeyError,StopIteration,AttributeError):return None
        enabled=document_tasks and value.get('tool_choice') in (None,'auto') and self.documents.available(value,allowed)
        cleanup_enabled=document_tasks and value.get('tool_choice') in (None,'auto') and self.reports.available(value,allowed)
        def checked_infer(_unused):
            feedback=None;previous=None
            for _ in range(2):
                if cancelled():raise ConnectionResetError('Planner cancelled')
                result=infer(request(model,question,items,feedback,previous,document_tasks=enabled,report_cleanup=cleanup_enabled))
                try:
                    from .document_workflow import decode_plan
                    raw=json.loads(result['choices'][0]['message']['content'])
                    if raw.get('intent')=='analyze':
                        if raw != {'intent':'analyze','actions':[]} or result['choices'][0].get('finish_reason')!='stop' or result['choices'][0]['message'].get('tool_calls'):
                            raise ValueError('Invalid read-only analysis plan')
                        analysis_selected.append(True)
                        return {'choices':[{'finish_reason':'stop','message':{'content':'{"intent":"chat","actions":[]}'}}]}
                    if raw.get('intent')=='report_cleanup':
                        from .report_cleanup import decode_plan as decode_cleanup
                        if not cleanup_enabled:raise ValueError('Report cleanup tools unavailable')
                        cleanup_selected.append(decode_cleanup(result,question))
                        return {'choices':[{'finish_reason':'stop','message':{'content':'{"intent":"chat","actions":[]}'}}]}
                    if raw.get('intent')=='documents':
                        if not enabled:raise ValueError('Document tools unavailable')
                        selected.append(decode_plan(result,question,items))
                        return {'choices':[{'finish_reason':'stop','message':{'content':'{"intent":"chat","actions":[]}'}}]}
                    plan=grounded_actions(result,question,items,source_ids_only=True)
                    if reference is not None and plan['intent']=='chat':
                        analysis_selected.append(True)
                    return {'choices':[{'finish_reason':'stop','message':{'content':json.dumps(plan,ensure_ascii=False,separators=(',',':'))}}]}
                except (ValueError,TypeError,KeyError,IndexError) as error:
                    feedback=str(error)
                    try:previous=result['choices'][0]['message']['content']
                    except (TypeError,KeyError,IndexError):previous=None
            decline.append('尚未执行操作：参数来源、单位或设备对象未通过校验，请明确目标设备及数值。')
            plan={'intent':'operate','actions':[{'operation':'unsupported','value':None}]}
            return {'choices':[{'finish_reason':'stop','message':{'content':json.dumps(plan)}}]}
        from .document_workflow import task_schema
        from .report_cleanup import task_schema as cleanup_schema
        if enabled and task_schema(question,items) or cleanup_enabled and cleanup_schema(question):
            cached=checked_infer(None)
            if cleanup_selected:return self.reports.begin(body,model,allowed,*cleanup_selected[0])
            if selected:return self.documents.begin(body,model,allowed,selected[0])
            if cleanup_enabled and cleanup_schema(question):
                return {'content':'尚未执行本轮摘要查看或清理。请明确是列出桌面摘要，还是将指定摘要移到回收站。',
                    'route':'model-report-cleanup'}
            answer=self.base.handle(body,model,allowed,lambda _body:cached,cancelled)
        else:
            answer=self.base.handle(body,model,allowed,checked_infer,cancelled)
        if selected:return self.documents.begin(body,model,allowed,selected[0])
        if cleanup_selected:return self.reports.begin(body,model,allowed,*cleanup_selected[0])
        if analysis_selected and answer is None:return {'read_only_analysis':True,'route':'model-read-only-analysis'}
        if decline and answer and 'content' in answer:answer=dict(answer,content=decline[0])
        return answer

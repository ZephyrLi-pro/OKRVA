import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from riscv_ai_adapter.presentation_workflow import NAMES, decode_plan, task_schema
from riscv_ai_adapter.grounding import GroundedPlanner, measurements, model_view, request


NOTES = '本页介绍智能温室的自动监测与控制。系统每10秒采集温度和湿度，温度超限时会启动风扇，断网期间还可以保存最近24小时的数据。'


def response(value):
    return {'choices': [{'finish_reason': 'stop', 'message': {'content': json.dumps(value, ensure_ascii=False)}}]}


def notes_response(text=NOTES, finish='stop'):
    return {'choices': [{'finish_reason': finish, 'message': {'content': text}}]}


def payload(question):
    properties = [{'query': {'type': 'string', 'maxLength': 160}}, {},
                  {'snapshot_id': {'type': 'string'}, 'notes': {'type': 'string', 'maxLength': 1500}}]
    tools = [{'type': 'function', 'function': {'name': name, 'parameters': {
        'type': 'object', 'properties': p, 'required': list(p), 'additionalProperties': False}}}
        for name, p in zip(NAMES, properties)]
    return {'messages': [{'role': 'user', 'content': question}], 'tools': tools}


def append(value, call, data, error=False):
    value['messages'] += [
        {'role': 'assistant', 'tool_calls': [{'id': call['id'], 'type': 'function', 'function': {
            'name': call['name'], 'arguments': json.dumps(call['arguments'], ensure_ascii=False)}}]},
        {'role': 'tool', 'tool_call_id': call['id'], 'content': json.dumps({
            'isError': error, 'content': [{'type': 'text', 'text': json.dumps(data, ensure_ascii=False)}]})}]


def snapshot():
    text = '每10秒采集温湿度；温度超限时启动风扇；断网时可保存最近24小时的数据。'
    return dict(ok=True, snapshot_id='a' * 32, session_id='reader-session-1',
                source_path='/home/openkylin/文档/温室.pptx', page=7, total=12,
                title='温室项目', page_title='智能温室控制', text=text,
                text_sha256=hashlib.sha256(text.encode()).hexdigest(), notes='已有备注')


class PresentationWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.question = '根据当前页生成演讲备注并保存。'
        self.value = payload(self.question)
        self.planner = GroundedPlanner()
        self.allowed = NAMES
        self.inference = []
        self.plan = {'action': 'notes', 'query': ''}

    def infer(self, body):
        self.inference.append(body)
        return response(dict(intent='presentation', **self.plan))

    def handle(self, **kwargs):
        return self.planner.handle(json.dumps(self.value, ensure_ascii=False).encode(), 'planner',
            self.allowed, self.infer, document_tasks=True,
            infer_presentation=kwargs.pop('infer_presentation', notes_response_adapter), **kwargs)

    def saved_call(self):
        call = self.handle()
        self.assertEqual(call['name'], NAMES[1])
        self.slide = snapshot()
        append(self.value, call, self.slide)
        call = self.handle()
        self.assertEqual(call['name'], NAMES[2])
        self.assertEqual(call['arguments'], {'snapshot_id': self.slide['snapshot_id'], 'notes': NOTES})
        return call

    def save_receipt(self, call):
        text = self.slide['notes'] + '\n\nAI演讲备注\n' + call['arguments']['notes']
        return dict(ok=True, snapshot_id=self.slide['snapshot_id'], page=self.slide['page'],
                    path=self.slide['source_path'], notes=text,
                    notes_sha256=hashlib.sha256(text.encode()).hexdigest(), verified=True, notes_visible=True,
                    display_mode='normal', notes_pane_supported=True, normal_view_visible=True)

    def test_native_get_generate_save_receipt_required(self):
        call = self.saved_call()
        append(self.value, call, self.save_receipt(call))
        answer = self.handle()
        self.assertIn('第7页', answer['content'])
        self.assertIn('已保存第7页演讲备注', answer['content'])
        self.assertIn('已在底部备注栏显示', answer['content'])
        self.assertTrue(answer['content'].endswith('文件已保存。'))
        self.assertNotIn('备份', answer['content'])
        self.assertNotIn('name', self.handle())

    def test_verified_save_without_supported_bottom_pane_is_success(self):
        call = self.saved_call()
        receipt = self.save_receipt(call)
        receipt.update(notes_visible=False, notes_pane_supported=False)
        append(self.value, call, receipt)
        answer = self.handle()
        self.assertEqual(answer['content'],
            '已保存第7页演讲备注；当前LibreOffice版本不支持底部备注栏，已保留普通视图。文件已保存。')
        self.assertNotIn('name', answer)
        self.assertNotIn('name', self.handle())

    def test_missing_or_inconsistent_display_receipts_cannot_claim_success(self):
        bad_values = [
            {'display_mode': 'notes'}, {'display_mode': None},
            {'normal_view_visible': False}, {'normal_view_visible': 1},
            {'notes_pane_supported': False}, {'notes_pane_supported': 'true'},
            {'notes_visible': False}, {'notes_visible': 1},
        ]
        for changes in bad_values:
            with self.subTest(changes=changes):
                self.setUp()
                call = self.saved_call()
                receipt = self.save_receipt(call)
                receipt.update(changes)
                append(self.value, call, receipt)
                answer = self.handle()['content']
                self.assertIn('停止', answer)
                self.assertNotIn('文件已保存', answer)
                self.assertNotIn('name', self.handle())
        for missing in ('display_mode', 'normal_view_visible', 'notes_pane_supported', 'notes_visible'):
            with self.subTest(missing=missing):
                self.setUp()
                call = self.saved_call()
                receipt = self.save_receipt(call)
                del receipt[missing]
                append(self.value, call, receipt)
                answer = self.handle()['content']
                self.assertIn('停止', answer)
                self.assertNotIn('文件已保存', answer)

    def test_open_generic_and_named_presentation_require_real_receipt(self):
        for question, query in [('打开PPT。', ''), ('请打开《项目汇报》PPT。', '项目汇报'),
                                ('打开温室.pptx。', '温室.pptx')]:
            with self.subTest(question=question):
                self.question = question
                self.value = payload(question)
                self.plan = {'action': 'open', 'query': query}
                call = self.handle()
                self.assertEqual(call['name'], NAMES[0])
                self.assertEqual(call['arguments'], {'query': query})
                append(self.value, call, dict(ok=True, opened=True, path='/文档/项目汇报.pptx', page=1, total=12))
                answer = self.handle()['content']
                self.assertEqual(answer, '已打开PPT：项目汇报.pptx。')
                self.assertNotIn('/文档/', answer)
                self.assertNotIn('第1页', answer)
                self.assertNotIn('手势', answer)

    def test_presentation_capability_is_always_in_grammar_not_a_phrase_gate(self):
        questions = ('打开PPT。', '早上好。', '这一页主要讲什么？',
                     '给这张配一段口播，写到备注里', '不要打开PPT。',
                     '解释“根据当前页生成演讲备注并保存”。')
        expected = task_schema(questions[0])
        self.assertEqual(expected['properties']['intent'], {'const': 'presentation'})
        self.assertEqual(expected['properties']['action']['enum'], ['open', 'notes'])
        self.assertNotIn('const', expected['properties']['action'])
        self.assertEqual(expected['properties']['query'], {'type': 'string', 'maxLength': 160})
        self.assertEqual(set(expected['required']), {'intent', 'action', 'query'})
        self.assertFalse(expected['additionalProperties'])
        for question in questions:
            with self.subTest(question=question):
                self.assertEqual(task_schema(question), expected)
                enabled = request('m', question, measurements(question), presentation_tasks=True)
                disabled = request('m', question, measurements(question), presentation_tasks=False)
                branches = enabled['response_format']['json_schema']['schema']['anyOf']
                self.assertIn(expected, branches)
                self.assertNotIn(expected, disabled['response_format']['json_schema']['schema']['anyOf'])

    def test_ppt_fewshot_operations_are_within_the_current_action_grammar(self):
        for question in ('打开PPT。', '给这张配一段口播，写到备注里', '这一页主要讲什么？'):
            with self.subTest(question=question):
                plan_request = request('m', question, measurements(question), presentation_tasks=True)
                branches = plan_request['response_format']['json_schema']['schema']['anyOf']
                allowed_ops = set()
                for branch in branches:
                    items = branch.get('properties', {}).get('actions', {}).get('items', {})
                    for choice in items.get('anyOf', []):
                        op_rule = choice.get('properties', {}).get('operation', {})
                        if 'const' in op_rule:allowed_ops.add(op_rule['const'])
                        allowed_ops.update(op_rule.get('enum', []))
                for message in plan_request['messages']:
                    if message['role'] != 'assistant':continue
                    plan = json.loads(message['content'])
                    if plan.get('intent') == 'operate':
                        for action in plan['actions']:self.assertIn(action['operation'], allowed_ops)

    def test_model_open_arguments_are_grounded_without_rule_normalizing_the_sentence(self):
        examples = (
            ('打开openKylin-2026这个PPT。', 'openKylin-2026'),
            ('打开openKylin-2026这个.pptx。', 'openKylin-2026这个.pptx'),
            ('打开“openKylin-2026那个.pptx”这个PPT。', 'openKylin-2026那个.pptx'),
            ('把《项目汇报》拿出来放映。', '项目汇报'),
            ('放一下《桌面：项目》', '桌面：项目'),
            ('打开《刚才那个》PPT。', '刚才那个'),
            ('打开刚才那个.pptx。', '刚才那个.pptx'),
            ('打开最近：项目.pptx。', '最近：项目.pptx'))
        for question, query in examples:
            with self.subTest(question=question):
                plan = {'action': 'open', 'query': query}
                self.assertEqual(decode_plan(response(dict(intent='presentation', **plan)), question), plan)

    def test_model_open_query_can_bind_masked_number_but_never_invent_a_name(self):
        question = '打开openKylin-2026这个PPT。'
        value = payload(question)
        observed = []
        def infer(plan_request):
            observed.append(plan_request)
            self.assertEqual(json.loads(plan_request['messages'][-1]['content'])['user_request'],
                             '打开openKylin<q0:bare>这个PPT。')
            return response({'intent': 'presentation', 'action': 'open', 'query': 'openKylin<q0:bare>'})
        result = GroundedPlanner().handle(json.dumps(value, ensure_ascii=False).encode(),
            'planner', NAMES, infer, document_tasks=True)
        self.assertEqual(result['name'], NAMES[0])
        self.assertEqual(result['arguments'], {'query': 'openKylin-2026'})
        self.assertEqual(value['messages'][0]['content'], question)
        self.assertEqual(len(observed), 1)
        for query in ('虚构名称', 'openKylin<q99:bare>', 'openKylin<q0:percent>'):
            with self.subTest(query=query), self.assertRaises(ValueError):
                decode_plan(response({'intent': 'presentation', 'action': 'open', 'query': query}), question)

    def test_model_selects_generic_or_recent_open_without_inventing_paths(self):
        for question, query in (('打开PPT。', ''), ('打开刚才那个PPT。', '最近:'),
                                ('继续放刚才那份吧。', '最近:'),
                                ('打开刚才那个PPT，不用列候选', '最近:')):
            with self.subTest(question=question):
                value = payload(question)
                observed = []
                def infer(plan_request):
                    observed.append(plan_request)
                    self.assertEqual(json.loads(plan_request['messages'][-1]['content'])['user_request'], question)
                    self.assertIn(task_schema(question),
                        plan_request['response_format']['json_schema']['schema']['anyOf'])
                    return response({'intent': 'presentation', 'action': 'open', 'query': query})
                result = GroundedPlanner().handle(json.dumps(value, ensure_ascii=False).encode(),
                    'planner', NAMES, infer, document_tasks=True)
                self.assertEqual(result['name'], NAMES[0])
                self.assertEqual(result['arguments'], {'query': query})
                self.assertEqual(value['messages'][0]['content'], question)
                self.assertEqual(len(observed), 1)

    def test_no_recent_receipt_has_fixed_message_without_discovery_or_retry(self):
        receipt = dict(ok=False, opened=False, reason='no_recent', message='最近记录不存在')
        for question, query in (('打开PPT。', ''), ('打开刚才那个PPT。', '最近:')):
            with self.subTest(question=question):
                self.setUp()
                self.question, self.value = question, payload(question)
                self.plan = {'action': 'open', 'query': query}
                call = self.handle()
                append(self.value, call, receipt)
                result = self.handle()
                self.assertEqual(result['content'], '没有可用的最近打开记录，请先打开一次PPT。')
                self.assertNotIn('候选', result['content'])
                self.assertNotIn('已打开', result['content'])
                self.assertNotIn('name', result)
                self.assertNotIn('name', self.handle())
                self.assertEqual(len(self.inference), 1)

    def test_no_recent_reply_requires_verified_receipt_and_matching_request(self):
        valid = dict(ok=False, opened=False, reason='no_recent', message='没有历史')
        invalid = [dict(valid, ok=True), dict(valid, opened=True), dict(valid, reason='not_found'),
                   dict(valid, message=''), dict(valid, message=1), dict(valid, message='a' * 301),
                   dict(ok=False, opened=False, needs_clarification=True, reason='ambiguous',
                        candidates=[{'name': '甲.pptx', 'scope': 'desktop'}], match_count=2)]
        for missing in ('ok', 'opened', 'reason', 'message'):
            receipt = copy.deepcopy(valid)
            del receipt[missing]
            invalid.append(receipt)
        for question, query, receipt in [('打开PPT。', '', r) for r in invalid] + [('打开项目汇报PPT。', '项目汇报', valid)]:
            with self.subTest(question=question, receipt=receipt):
                self.setUp()
                self.question, self.value = question, payload(question)
                self.plan = {'action': 'open', 'query': query}
                call = self.handle()
                append(self.value, call, receipt)
                result = self.handle()
                self.assertIn('停止', result['content'])
                self.assertNotIn('已打开', result['content'])
                self.assertNotIn('候选', result['content'])
                self.assertNotIn('name', result)

    def test_model_chat_analysis_and_unsupported_remain_without_ppt_side_effects(self):
        for plan in ({'intent': 'chat', 'actions': []}, {'intent': 'analyze', 'actions': []},
                     {'intent': 'operate', 'actions': [
                         {'operation': 'unsupported', 'source': 's0', 'quantity': None}]}):
            with self.subTest(plan=plan):
                value = payload('打开PPT。')
                observed = []
                def infer(plan_request):
                    observed.append(plan_request)
                    self.assertIn(task_schema('打开PPT。'),
                        plan_request['response_format']['json_schema']['schema']['anyOf'])
                    return response(plan)
                result = GroundedPlanner().handle(json.dumps(value).encode(),
                    'planner', NAMES, infer, document_tasks=True)
                if plan['intent'] == 'chat':self.assertIsNone(result)
                elif plan['intent'] == 'analyze':self.assertTrue(result['read_only_analysis'])
                else:self.assertNotIn('name', result or {})
                self.assertEqual(len(observed), 1)

    def test_pure_explanation_quotes_and_explicit_effect_denial_are_post_plan_vetoes(self):
        cases = (
            ('不要打开PPT', 'open'), ('如何打开PPT？', 'open'),
            ('解释“打开PPT”是什么意思', 'open'), ('“打开PPT”', 'open'),
            ('不用根据当前页生成演讲备注并保存', 'notes'),
            ('怎么根据当前页生成演讲备注并保存？', 'notes'),
            ('解释“根据当前页生成演讲备注并保存”', 'notes'),
            ('只给我讲稿文本，不要写入或保存', 'notes'),
            ('打开PPT然后返回桌面', 'open'),
            ('根据当前PPT页生成备注并保存，然后返回桌面。', 'notes'))
        for text, action in cases:
            with self.subTest(text=text):
                self.assertIsNotNone(task_schema(text))
                plan = {'intent': 'presentation', 'action': action, 'query': ''}
                with self.assertRaises(ValueError):decode_plan(response(plan), text)
                observed = []
                def infer(plan_request):
                    observed.append(plan_request)
                    self.assertIn(task_schema(text),
                        plan_request['response_format']['json_schema']['schema']['anyOf'])
                    return response(plan)
                result = GroundedPlanner().handle(json.dumps(payload(text), ensure_ascii=False).encode(),
                    'planner', NAMES, infer, document_tasks=True)
                self.assertNotIn('name', result or {})
                self.assertGreaterEqual(len(observed), 1)

    def test_bad_model_arguments_are_rejected_after_semantic_selection(self):
        question = '打开《项目汇报》PPT。'
        valid = {'intent': 'presentation', 'action': 'open', 'query': '项目汇报'}
        bad = [dict(valid, action='delete'), dict(valid, query=True), dict(valid, query=None),
               dict(valid, query='编造的文件'), dict(valid, query='a' * 161),
               dict(valid, action='notes', query='项目汇报'), dict(valid, path='/home/a.pptx')]
        for missing in ('intent', 'action', 'query'):
            plan = dict(valid);del plan[missing];bad.append(plan)
        for plan in bad:
            with self.subTest(plan=plan), self.assertRaises(ValueError):decode_plan(response(plan), question)
        for query in ('/home/a.pptx', '../项目.pptx', 'C:\\项目.pptx', '项目:汇报',
                      '项目\\汇报', '项目\x00汇报', '项目\n汇报'):
            with self.subTest(query=query), self.assertRaises(ValueError):
                decode_plan(response(dict(valid, query=query)), '打开《' + query + '》PPT。')
        incomplete = response(valid);incomplete['choices'][0]['finish_reason'] = 'length'
        native = response(valid);native['choices'][0]['message']['tool_calls'] = [{'id': 'invented'}]
        for result in (incomplete, native):
            with self.assertRaises(ValueError):decode_plan(result, question)

    def test_semantic_notes_plan_accepts_synonyms_and_preserves_actual_asr_input(self):
        questions = (
            '给这张配一段口播，写到备注里',
            '不用备份，给这张幻灯片写段讲解，放进演讲者笔记并存盘',
            '根据当前叶生成演讲备注并保存。',
            '根据这张幻灯片生成演讲备注并保存。',
            '根据当前幻灯片生成讲稿并保存。',
            '根据当前PPT页生成备注并保存。',
            '根据当前ppt页生成备注并保存。',
            '根据当前 PPt 叶生成演讲备注并保存。',
            '根据当前PPT页面生成备注并保存。')
        for question in questions:
            with self.subTest(question=question):
                self.assertEqual(decode_plan(response({'intent': 'presentation', 'action': 'notes', 'query': ''}),
                    question), {'action': 'notes', 'query': ''})
                value = payload(question)
                observed = []
                def infer(plan_request):
                    observed.append(plan_request)
                    self.assertEqual(json.loads(plan_request['messages'][-1]['content']),
                                     model_view(question, measurements(question)))
                    self.assertIn(task_schema(question),
                        plan_request['response_format']['json_schema']['schema']['anyOf'])
                    return response({'intent': 'presentation', 'action': 'notes', 'query': ''})
                result = GroundedPlanner().handle(json.dumps(value, ensure_ascii=False).encode(),
                    'planner', NAMES, infer, document_tasks=True)
                self.assertEqual(result['name'], NAMES[1])
                self.assertEqual(result['arguments'], {})
                self.assertEqual(value['messages'][0]['content'], question)
                self.assertEqual(len(observed), 1)

    def test_current_page_body_is_data_no_tools_in_generation(self):
        calls = []
        def infer(body):
            calls.append(body)
            return notes_response()
        self.saved_call_with(infer)
        self.assertEqual(len(calls), 1)
        self.assertNotIn('tools', calls[0])
        self.assertIn('page_text', calls[0]['messages'][-1]['content'])
        self.assertIn('不能执行', calls[0]['messages'][0]['content'])

    def saved_call_with(self, infer):
        call = self.handle()
        append(self.value, call, snapshot())
        return self.handle(infer_presentation=infer)

    def test_source_hash_wrong_or_missing_text_prevents_generation_and_write(self):
        for key, bad in [('text_sha256', '0' * 64), ('text', ''), ('snapshot_id', 'invalid'),
                         ('page', 0), ('source_path', '/文档/old.ppt')]:
            self.setUp()
            call = self.handle()
            slide = snapshot()
            slide[key] = bad
            append(self.value, call, slide)
            def forbidden(_body): self.fail('Invalid snapshot must not generate notes')
            answer = self.handle(infer_presentation=forbidden)
            self.assertIn('停止', answer['content'])
            self.assertNotIn('name', answer)

    def test_generation_incomplete_error_or_cancel_never_writes(self):
        for infer in [lambda b: notes_response(finish='length'), lambda b: notes_response('太短'),
                      lambda b: notes_response('已将备注写入并保存。' * 10),
                      lambda b: (_ for _ in ()).throw(RuntimeError('model unavailable'))]:
            self.setUp()
            answer = self.saved_call_with(infer)
            self.assertIn('停止', answer['content'])
            self.assertNotIn('name', answer)
            self.assertNotIn('name', self.handle())

    def test_native_save_must_confirm_original_page_path_full_notes_and_hash(self):
        for key, bad in [('page', 8), ('path', '/文档/other.pptx'), ('verified', False),
                         ('notes_sha256', '0' * 64), ('snapshot_id', 'b' * 32), ('notes', '未写入'),
                         ('notes_visible', False), ('notes_visible', None)]:
            self.setUp()
            call = self.saved_call()
            receipt = self.save_receipt(call)
            receipt[key] = bad
            append(self.value, call, receipt)
            answer = self.handle()['content']
            self.assertIn('停止', answer)
            self.assertNotIn('文件已保存', answer)
            self.assertNotIn('name', self.handle())

    def test_plain_executor_timeout_after_save_reports_unknown_state_without_retry(self):
        call = self.saved_call()
        append(self.value, call, {})
        # Native MCP transport failures arrive from the executor as text, not
        # an isError MCP object; the underlying UNO store may still complete.
        self.value['messages'][-1]['content'] = 'Error: MCP server `ppt` timed out during save_speaker_notes'
        answer = self.handle()
        self.assertIn('保存状态待核实', answer['content'])
        self.assertNotIn('文件已保存', answer['content'])
        self.assertIn('没有自动重复保存', answer['content'])
        self.assertNotIn('name', answer)
        self.assertEqual(self.handle(), answer)

    def test_changed_user_catalog_prior_result_and_unknown_restart_fail_closed(self):
        for mode in ('user', 'catalog', 'result', 'restart'):
            self.setUp()
            call = self.saved_call()
            receipt = self.save_receipt(call)
            if mode == 'user': self.value['messages'][0]['content'] = '生成别的页'
            elif mode == 'catalog': self.value['tools'][0]['function']['parameters']['properties']['query']['maxLength'] = 12
            elif mode == 'result': self.value['messages'][2]['content'] = '{}'
            else: self.planner = GroundedPlanner()
            append(self.value, call, receipt)
            self.assertIn('停止', self.handle()['content'])

    def test_gateway_may_prune_only_leading_old_history(self):
        history = [{'role': 'user', 'content': '旧问题'}, {'role': 'assistant', 'content': '旧回答'}]
        self.value['messages'] = copy.deepcopy(history) + self.value['messages']
        call = self.handle()
        append(self.value, call, snapshot())
        self.value['messages'] = self.value['messages'][2:]
        call = self.handle()
        self.assertEqual(call['name'], NAMES[2])

    def test_disabled_tools_cannot_be_used_or_resumed(self):
        self.value['tool_choice'] = 'none'
        self.assertIsNone(self.handle())
        self.setUp()
        call = self.saved_call()
        append(self.value, call, self.save_receipt(call))
        self.value['tool_choice'] = 'none'
        self.assertIn('停止', self.handle()['content'])

    def test_ordinary_page_summary_still_offers_capability_but_model_selects_analysis(self):
        text = '这一页主要讲什么？'
        enabled = request('m', text, measurements(text), presentation_tasks=True)
        self.assertIn(task_schema(text), enabled['response_format']['json_schema']['schema']['anyOf'])
        result = GroundedPlanner().handle(json.dumps(payload(text), ensure_ascii=False).encode(),
            'planner', NAMES, lambda b: response({'intent': 'analyze', 'actions': []}), document_tasks=True)
        self.assertTrue(result['read_only_analysis'])
        self.assertNotIn('name', result)


def notes_response_adapter(body):
    return notes_response()


if __name__ == '__main__':
    unittest.main()

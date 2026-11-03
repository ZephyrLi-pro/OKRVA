import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src'))
from riscv_ai_adapter.grounding import GroundedPlanner,request,measurements
from riscv_ai_adapter.report_cleanup import NAMES,task_schema,decode_plan
from riscv_ai_adapter.semantic_planner import OPERATIONS
from riscv_ai_adapter.backends import rewrite_chat_request
from test_document_workflow import payload,append,response

class CleanupTests(unittest.TestCase):
    def setUp(self):
        self.question='删除桌面的几个摘要文件。';self.mode='preview';self.planner=GroundedPlanner()
        self.value=payload(self.question)
        props=[{}, {'selection_id':{'type':'string'},'report_ids':{'type':'array','minItems':1,'maxItems':20,'items':{'type':'string'}}}]
        self.value['tools'] += [{'type':'function','function':{'name':n,'parameters':{'type':'object','properties':p,'required':list(p)}}} for n,p in zip(NAMES,props)]
        self.allowed=tuple(OPERATIONS)+NAMES
        self.rows=[{'report_id':str(i)*32,'name':f'文档摘要-20261026-120000-{i:08x}.md'} for i in (1,2)]
    def handle(self):
        return self.planner.handle(json.dumps(self.value,ensure_ascii=False).encode(),'model',self.allowed,
            lambda b:response({'intent':'report_cleanup','selection':self.mode}),document_tasks=True)
    def discover(self):
        self.value['messages'][-1]['content']=self.question
        call=self.handle();self.assertEqual(call.get('name'),NAMES[0],call)
        append(self.value,call,{'selection_id':'a'*32,'reports':self.rows})
        return self.handle()
    def test_ambiguous_several_lists_only_and_never_guesses_files(self):
        result=self.discover()
        self.assertIn('尚未删除',result['content']);self.assertNotIn('name',result)
        for row in self.rows:self.assertIn(row['name'],result['content'])
    def test_explicit_all_uses_actual_ids_and_verified_native_results(self):
        self.question='把桌面所有摘要都移入回收站。';self.mode='all'
        call=self.discover();self.assertEqual(call['name'],NAMES[1]);self.assertEqual(call['arguments']['report_ids'],[r['report_id'] for r in self.rows])
        append(self.value,call,{'selection_id':'a'*32,'verified':True,'requested':2,'count':2,'moved':self.rows,'message':'真实工具结果'})
        self.assertIn('2份摘要移入回收站',self.handle()['content'])
        self.assertIn('停止',self.handle()['content'])
    def test_names_bind_exact_source_instead_of_model_invented_paths(self):
        self.question='删除'+self.rows[1]['name']+'。';self.mode='names'
        call=self.discover();self.assertEqual(call['arguments']['report_ids'],[self.rows[1]['report_id']])
    def test_source_named_reports_and_identifier_words_do_not_become_commands(self):
        for source in ('b_network','会议纪要','网络_音量_不要删除'):
            with self.subTest(source=source):
                self.setUp();self.rows[1]['name']=f'文档摘要-{source}-20261026-120000-00000002.md'
                self.question='删除'+self.rows[1]['name']+'。';self.mode='names'
                call=self.discover();self.assertEqual(call['arguments']['report_ids'],[self.rows[1]['report_id']])
    def test_partial_or_unknown_named_selection_does_not_move_anything(self):
        self.question='删除'+self.rows[1]['name']+'和文档摘要-20261026-120000-deadbeef.md。';self.mode='names'
        result=self.discover();self.assertIn('没有移动',result['content']);self.assertNotIn('name',result)
    def test_partial_result_is_not_reported_as_full_success(self):
        self.question='删除桌面全部摘要文件。';self.mode='all';call=self.discover()
        append(self.value,call,{'selection_id':'a'*32,'verified':False,'requested':2,'count':1,'moved':self.rows[:1],'message':'全部成功（不可信文字）'})
        result=self.handle()['content'];self.assertIn('1份',result);self.assertIn('未确认',result);self.assertNotIn('全部成功',result)
    def test_plan_cannot_turn_ambiguous_words_into_all_or_delete_path(self):
        with self.assertRaises(ValueError):decode_plan(response({'intent':'report_cleanup','selection':'all'}),self.question)
        for text in ('如何删除桌面的摘要？','不要删除桌面全部摘要。','永久删除桌面摘要。','清空回收站中的摘要。',
                     '删除桌面全部摘要但保留第一份。','删除/home/openkylin/文档摘要-20261026-120000-00000001.md。',
                     '生成三份摘要然后删除桌面全部摘要。','删除桌面摘要，然后打开系统设置。'):
            self.assertIsNone(task_schema(text),text)
    def test_model_can_choose_chat_without_native_actions(self):
        result=self.planner.handle(json.dumps(self.value).encode(),'model',self.allowed,
            lambda b:response({'intent':'chat','actions':[]}),document_tasks=True)
        self.assertNotIn('name',result)
        self.assertIn('尚未执行本轮',result['content'])
    def test_changed_history_or_tool_result_never_issues_move(self):
        self.question='删除桌面全部摘要文件。';self.mode='all'
        self.value['messages'][0]['content']=self.question;call=self.handle()
        append(self.value,call,{'selection_id':'a'*32,'reports':self.rows})
        self.value['messages'][0]['content']='删除其他文件。'
        self.assertNotIn('name',self.handle())
    def test_actual_leading_history_pruning_is_allowed(self):
        self.question='删除桌面全部摘要文件。';self.mode='all'
        self.value['messages']=[{'role':'user','content':'旧问题'},{'role':'assistant','content':'旧回答'}, {'role':'user','content':self.question}]
        call=self.handle();append(self.value,call,{'selection_id':'a'*32,'reports':self.rows})
        self.value['messages']=self.value['messages'][2:]
        self.assertEqual(self.handle().get('name'),NAMES[1])
    def test_disabled_or_restart_does_not_resume_pending_cleanup(self):
        self.question='删除桌面全部摘要文件。';self.mode='all';call=self.discover()
        append(self.value,call,{'selection_id':'a'*32,'verified':True,'requested':2,'count':2,'moved':self.rows,'message':'ok'})
        self.planner=GroundedPlanner();self.assertIn('停止',self.handle()['content'])
    def test_existing_system_prompt_and_generic_write_policy_preserved(self):
        q='音量调节成30%。';m=measurements(q)
        self.assertEqual(request('model',q,m),request('model',q,m,report_cleanup=True))
        value=json.loads(rewrite_chat_request(json.dumps(self.value).encode(),'model',self.allowed,
            system_prompt_override='你是小K。',semantic_only=True))
        self.assertNotIn(NAMES[1],[t['function']['name'] for t in value['tools']])

if __name__=='__main__':unittest.main()

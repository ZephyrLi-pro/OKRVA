"""Cross-capability regressions: all summary AND cleanup tools enabled."""
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src'))
from riscv_ai_adapter.grounding import GroundedPlanner,request,measurements
from riscv_ai_adapter.document_workflow import NAMES as DOCS
from riscv_ai_adapter.report_cleanup import NAMES as REPORTS,task_schema as cleanup_schema
from riscv_ai_adapter.semantic_planner import OPERATIONS
from test_document_workflow import payload,response,append,infer,save_result

def all_tools(question):
    value=payload(question)
    props=[{}, {'selection_id':{'type':'string'},'report_ids':{'type':'array','minItems':1,'maxItems':20,'items':{'type':'string'}}}]
    value['tools'] += [{'type':'function','function':{'name':name,'parameters':{'type':'object','properties':p,'required':list(p)}}} for name,p in zip(REPORTS,props)]
    return value

class SummaryRoutingTests(unittest.TestCase):
    def test_create_wording_is_not_replaced_by_cleanup_only_prompt(self):
        for verb in ('形成','生成','整理成','概括为'):
            with self.subTest(verb=verb):
                q=f'查找文档目录里最近修改的三份文件，分别{verb}摘要，并把结果保存到桌面。'
                definition=request('model',q,measurements(q),document_tasks=True,report_cleanup=True)
                self.assertEqual(definition['response_format']['json_schema']['name'],'evidence_system_plan')
                self.assertEqual(definition,request('model',q,measurements(q),document_tasks=True,report_cleanup=False))
                self.assertIsNone(cleanup_schema(q))
                value=all_tools(q);planner=GroundedPlanner();seen=[]
                def model(body):seen.append(body);return infer(body)
                result=planner.handle(json.dumps(value,ensure_ascii=False).encode(),'model',tuple(OPERATIONS)+DOCS+REPORTS,model,document_tasks=True)
                self.assertEqual(result.get('name'),DOCS[0],result)
                self.assertEqual(result['arguments']['count'],3)
                self.assertNotEqual(seen[0]['response_format']['json_schema']['name'],'report_cleanup_plan')
    def test_overlapping_view_wording_keeps_both_choices_for_the_model(self):
        q='查看文档目录里最近修改的三份文件，分别形成摘要，并把结果保存到桌面。'
        self.assertIsNone(cleanup_schema(q))
        result=request('model',q,measurements(q),document_tasks=True,report_cleanup=True)
        self.assertEqual(result['response_format']['json_schema']['name'],'evidence_system_plan')
        # If a future scope change reintroduces overlap, the shortcut may not
        # erase the document branch. Both choices must remain model-visible.
        definition={'type':'object','properties':{'intent':{'const':'report_cleanup'},'selection':{'enum':['preview']}},'required':['intent','selection'],'additionalProperties':False}
        with patch('riscv_ai_adapter.report_cleanup.task_schema',return_value=definition):
            result=request('model',q,measurements(q),document_tasks=True,report_cleanup=True)
            intents={b['properties']['intent'].get('const') for b in result['response_format']['json_schema']['schema']['anyOf']}
            self.assertIn('documents',intents);self.assertIn('report_cleanup',intents)
            value=all_tools(q);planner=GroundedPlanner()
            result=planner.handle(json.dumps(value).encode(),'model',tuple(OPERATIONS)+DOCS+REPORTS,infer,document_tasks=True)
            self.assertEqual(result.get('name'),DOCS[0])
    def test_report_listing_and_cleanup_keep_their_own_scope(self):
        for question,selection in [('列出桌面的摘要文件。','preview'),('删除桌面的几个摘要文件。','preview'),
            ('查看桌面上有哪些摘要。','preview'),('删除桌面所有的摘要文件。','all')]:
            with self.subTest(question=question):
                definition=request('model',question,measurements(question),document_tasks=True,report_cleanup=True)
                self.assertEqual(definition['response_format']['json_schema']['name'],'report_cleanup_plan')
                value=all_tools(question);planner=GroundedPlanner()
                answer=planner.handle(json.dumps(value).encode(),'model',tuple(OPERATIONS)+DOCS+REPORTS,
                    lambda b:response({'intent':'report_cleanup','selection':selection}),document_tasks=True)
                self.assertEqual(answer.get('name'),REPORTS[0])
    def test_new_summary_task_after_cleanup_turn_uses_current_request(self):
        for word in ('形成','生成'):
            q=f'查找文档目录里最近修改的三份文件，分别{word}摘要，并把结果保存到桌面。'
            value=all_tools('删除桌面全部摘要文件。');planner=GroundedPlanner();allowed=tuple(OPERATIONS)+DOCS+REPORTS
            call=planner.handle(json.dumps(value).encode(),'model',allowed,lambda b:response({'intent':'report_cleanup','selection':'all'}),document_tasks=True)
            append(value,call,{'selection_id':'a'*32,'reports':[]})
            final=planner.handle(json.dumps(value).encode(),'model',allowed,infer,document_tasks=True)
            value['messages'] += [{'role':'assistant','content':final['content']},{'role':'user','content':q}]
            call=planner.handle(json.dumps(value).encode(),'model',allowed,infer,document_tasks=True)
            self.assertEqual(call.get('name'),DOCS[0],call)
    def test_full_document_native_chain_with_all_tools_never_calls_cleanup(self):
        q='查找文档目录里最近修改的三份文件，分别形成摘要，并把结果保存到桌面。'
        value=all_tools(q);planner=GroundedPlanner();allowed=tuple(OPERATIONS)+DOCS+REPORTS;calls=[]
        def step():
            answer=planner.handle(json.dumps(value).encode(),'model',allowed,infer,document_tasks=True)
            if answer.get('name'):calls.append(answer['name'])
            return answer
        call=step();rows=[{'document_id':str(i)*32,'name':str(i)+'.txt','modified_ns':100-i} for i in (1,2,3)]
        append(value,call,{'batch_id':'b'*32,'documents':rows});call=step()
        for row in rows:
            append(value,call,dict(row,batch_id='b'*32,text='这是原文事实。'))
            call=step()
        self.assertEqual(calls,[DOCS[0]]+[DOCS[1]]*3+[DOCS[2]])
        append(value,call,save_result(rows,'b'*32))
        self.assertIn('3个独立摘要文件',step()['content'])
    def test_declined_report_view_never_delegates_stale_success_prose(self):
        value=all_tools('查看桌面上有哪些摘要文件。')
        value['messages']=[{'role':'user','content':'以前的问题'},{'role':'assistant','content':'已生成3份文档摘要，旧结果。'}]+value['messages']
        planner=GroundedPlanner()
        answer=planner.handle(json.dumps(value).encode(),'model',tuple(OPERATIONS)+DOCS+REPORTS,
            lambda b:response({'intent':'chat','actions':[]}),document_tasks=True)
        self.assertNotIn('name',answer);self.assertIn('尚未执行本轮',answer['content']);self.assertNotIn('已生成',answer['content'])

if __name__=='__main__':unittest.main()

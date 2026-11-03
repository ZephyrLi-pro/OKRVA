import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src'))
from riscv_ai_adapter.document_workflow import DocumentWorkflow,NAMES,task_schema,decode_plan,select_points
from riscv_ai_adapter.grounding import GroundedPlanner,measurements,request
from riscv_ai_adapter.semantic_planner import OPERATIONS


def response(value):return {'choices':[{'finish_reason':'stop','message':{'content':json.dumps(value,ensure_ascii=False)}}]}


def save_result(rows,batch_id='a'*32,verified=True):
    return {'layout':'one_file_per_document','batch_id':batch_id,'verified':verified,'count':len(rows),'expected_count':len(rows),
        'reports':[{'document_id':r['document_id'],'source_name':r['name'],'path':f'/Desktop/report-{i}.md',
            'sha256':'a'*64,'bytes':100,'verified':True} for i,r in enumerate(rows)],'message':'原生保存回执'}


def payload(question):
    props=[{'directory':{'type':'string'},'count':{'type':'integer'},'order':{'type':'string'}},
           {'batch_id':{'type':'string'},'document_id':{'type':'string'}},
           {'batch_id':{'type':'string'},'entries':{'type':'array','items':{'type':'object','properties':{
               'document_id':{'type':'string'},'points':{'type':'array','items':{'type':'string'}}},'required':['document_id','points']}}}]
    tools=[{'type':'function','function':{'name':name,'parameters':{'type':'object','properties':p,'required':list(p)}}} for name,p in zip(NAMES,props)]
    tools += [{'type':'function','function':{'name':name,'parameters':{'type':'object','properties':p,'required':list(p)}}} for name,(_,p) in OPERATIONS.items()]
    return {'messages':[{'role':'user','content':question}],'tools':tools}


def append(value,call,data,error=False):
    value['messages'] += [{'role':'assistant','tool_calls':[{'id':call['id'],'type':'function','function':{
        'name':call['name'],'arguments':json.dumps(call['arguments'],ensure_ascii=False)}}]},
        {'role':'tool','tool_call_id':call['id'],'content':json.dumps({'isError':error,'content':[{'type':'text','text':json.dumps(data,ensure_ascii=False)}]})}]


def infer(body):
    if body['response_format']['json_schema']['name']=='document_key_sentences':
        value=json.loads(body['messages'][-1]['content'])
        return response({'selected':[row['id'] for row in value['segments'][:value['select_count']]]})
    return response({'intent':'documents','directory':'documents','count':'q0','destination':'desktop','order':'modified_desc'})


class DocumentWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.question='查找文档目录里最近修改的三份文件，分别生成摘要，并把结果保存到桌面。'
        self.value=payload(self.question);self.planner=GroundedPlanner();self.allowed=tuple(OPERATIONS)+NAMES
    def handle(self,**kw):return self.planner.handle(json.dumps(self.value,ensure_ascii=False).encode(),'model',self.allowed,infer,document_tasks=True,**kw)
    def discovery(self):
        call=self.handle();self.assertEqual(call['name'],NAMES[0]);self.assertEqual(call['arguments']['count'],3)
        self.rows=[{'document_id':str(i)*32,'name':str(i)+'.txt','modified_ns':100-i} for i in (1,2,3)]
        append(self.value,call,{'batch_id':'a'*32,'documents':self.rows})
        return self.handle()
    def test_real_native_steps_required_before_verified_save(self):
        call=self.discovery()
        for row in self.rows:
            self.assertEqual(call['name'],NAMES[1]);self.assertEqual(call['arguments']['document_id'],row['document_id'])
            append(self.value,call,dict(row,batch_id='a'*32,text='功耗2瓦。电池8小时。不要直接用水清洗。'))
            call=self.handle()
        self.assertEqual(call['name'],NAMES[2]);self.assertEqual(len(call['arguments']['entries']),3)
        for entry in call['arguments']['entries']:self.assertEqual(entry['points'],['功耗2瓦。','电池8小时。','不要直接用水清洗。'])
        append(self.value,call,save_result(self.rows))
        self.assertIn('3个独立摘要文件',self.handle()['content'])
        self.assertIn('停止',self.handle()['content']) # No replay/duplicate save.
    def test_failure_changed_source_or_prior_result_stops(self):
        call=self.discovery();append(self.value,call,{},error=True)
        self.assertIn('停止',self.handle()['content'])
        self.setUp();call=self.discovery()
        self.value['messages'][2]['content']='tampered'
        append(self.value,call,dict(self.rows[0],batch_id='a'*32,text='正文。'))
        self.assertIn('停止',self.handle()['content'])
    def test_caller_cannot_change_count_ids_or_history(self):
        call=self.discovery();self.value['messages'][0]['content']='读另一份文件'
        append(self.value,call,dict(self.rows[0],batch_id='a'*32,text='正文。'))
        self.assertIn('停止',self.handle()['content'])
    def test_gateway_can_trim_only_older_history_during_active_document_turn(self):
        history=[{'role':'user','content':'你好'},{'role':'assistant','content':'你好。'},
                 {'role':'user','content':'你是谁'},{'role':'assistant','content':'我是小K。'}]
        self.value['messages']=history+self.value['messages']
        call=self.discovery()
        for index,row in enumerate(self.rows):
            append(self.value,call,dict(row,batch_id='a'*32,text='第一条事实。第二条事实。第三条事实。'))
            if index==2:
                # The real Gateway has max_history_messages=12 and drops old
                # messages here, while the current user/native chain remains.
                self.value['messages']=self.value['messages'][2:]
            call=self.handle()
        self.assertEqual(call.get('name'),NAMES[2],call)
        append(self.value,call,save_result(self.rows))
        self.assertIn('3个独立摘要文件',self.handle()['content'])
    def test_history_trimming_does_not_allow_mutation_insertion_reordering_or_restore(self):
        for mode in ('change_retained','insert','reorder','restore','change_user','change_catalog','change_model'):
            with self.subTest(mode=mode):
                self.setUp()
                history=[{'role':'user','content':'先前的问题'},{'role':'assistant','content':'先前的回答'},
                         {'role':'user','content':'另一个问题'},{'role':'assistant','content':'另一个回答'}]
                self.value['messages']=copy.deepcopy(history)+self.value['messages']
                call=self.discovery()
                # A verified native read continues after real leading pruning.
                self.value['messages']=self.value['messages'][2:]
                append(self.value,call,dict(self.rows[0],batch_id='a'*32,text='第一份事实。'))
                call=self.handle();self.assertEqual(call.get('name'),NAMES[1])
                append(self.value,call,dict(self.rows[1],batch_id='a'*32,text='第二份事实。'))
                if mode=='change_retained':self.value['messages'][0]['content']='修改过的旧问题'
                elif mode=='insert':self.value['messages'].insert(0,{'role':'assistant','content':'额外内容'})
                elif mode=='reorder':self.value['messages'][:2]=reversed(self.value['messages'][:2])
                elif mode=='restore':self.value['messages']=copy.deepcopy(history[:2])+self.value['messages']
                elif mode=='change_user':self.value['messages'][2]['content']='不同的当前指令'
                elif mode=='change_catalog':self.value['tools'][0]['function']['parameters']['properties']['count']['maximum']=2
                if mode=='change_model':
                    answer=self.planner.handle(json.dumps(self.value).encode(),'other-model',self.allowed,infer,document_tasks=True)
                else:answer=self.handle()
                self.assertIn('停止',answer['content']);self.assertNotIn('name',answer)
    def test_trimming_does_not_allow_dropping_active_native_results(self):
        call=self.discovery();append(self.value,call,dict(self.rows[0],batch_id='a'*32,text='真实正文。'))
        self.value['messages'][1:3]=[]
        self.assertIn('停止',self.handle()['content'])
    def test_failure_reports_stage_without_repeating_save(self):
        call=self.discovery();append(self.value,call,{},error=True)
        answer=self.handle()
        self.assertIn('第1份文档读取',answer['content'])
        self.assertEqual(self.handle(),answer)
        self.assertNotIn('name',answer)
    def test_bounded_gateway_history_retains_full_five_document_transaction(self):
        self.question='查找文档目录里最近修改的五份文件，分别生成摘要，并把结果保存到桌面。'
        self.value=payload(self.question)
        history=[{'role':role,'content':f'旧轮次{i}'} for i in range(8) for role in ('user','assistant')]
        self.value['messages']=history+self.value['messages']
        call=self.handle();self.assertEqual(call['arguments']['count'],5)
        rows=[{'document_id':str(i)*32,'name':str(i)+'.txt','modified_ns':100-i} for i in range(1,6)]
        def advance(data):
            append(self.value,call,data)
            self.value['messages']=self.value['messages'][-16:]
            return self.handle()
        call=advance({'batch_id':'a'*32,'documents':rows})
        for row in rows:
            self.assertEqual(call.get('name'),NAMES[1],call)
            call=advance(dict(row,batch_id='a'*32,text='一条原文事实。'))
        self.assertEqual(call.get('name'),NAMES[2],call)
        call=advance(save_result(rows))
        self.assertIn('5个独立摘要文件',call['content'])
    def saved_call(self):
        call=self.discovery()
        for row in self.rows:
            append(self.value,call,dict(row,batch_id='a'*32,text='本文件的原文事实。'))
            call=self.handle()
        self.assertEqual(call['name'],NAMES[2]);return call
    def test_requires_distinct_per_source_report_receipts(self):
        for bad in ('legacy','missing','duplicate_path','wrong_source','wrong_batch','bad_hash'):
            with self.subTest(bad=bad):
                self.setUp();call=self.saved_call();value=save_result(self.rows)
                if bad=='legacy':value={'verified':True,'count':3,'path':'/Desktop/all.md','sha256':'a'*64,'message':'3份已保存'}
                elif bad=='missing':value['reports'].pop();value['count']=2
                elif bad=='duplicate_path':value['reports'][1]['path']=value['reports'][0]['path']
                elif bad=='wrong_source':value['reports'][0]['source_name']='another.txt'
                elif bad=='wrong_batch':value['batch_id']='b'*32
                elif bad=='bad_hash':value['reports'][0]['sha256']='wrong'
                append(self.value,call,value);self.assertIn('停止',self.handle()['content'])
    def test_partial_receipts_report_actual_count_and_never_repeat_save(self):
        call=self.saved_call();value=save_result(self.rows)
        value.update(verified=False,count=1,reports=value['reports'][:1])
        append(self.value,call,value);answer=self.handle()
        self.assertIn('已确认保存1个',answer['content']);self.assertIn('未全部完成',answer['content'])
        self.assertNotIn('name',self.handle())
    def test_restart_unknown_state_fails_closed(self):
        call=self.discovery();self.planner=GroundedPlanner()
        append(self.value,call,dict(self.rows[0],batch_id='a'*32,text='正文。'))
        self.assertIn('停止',self.handle()['content'])
    def test_sort_count_and_read_identity_are_checked(self):
        call=self.handle();append(self.value,call,{'batch_id':'a'*32,'documents':[]})
        self.assertIn('停止',self.handle()['content'])
        self.setUp();call=self.discovery();append(self.value,call,dict(self.rows[0],batch_id='a'*32,text='正文。',modified_ns=999))
        self.assertIn('停止',self.handle()['content'])
    def test_model_selects_task_not_a_whole_sentence_trigger(self):
        def chat(_body):return response({'intent':'chat','actions':[]})
        answer=self.planner.handle(json.dumps(self.value).encode(),'model',self.allowed,chat,document_tasks=True)
        self.assertIsNone(answer)
    def test_identical_new_requests_receive_distinct_read_only_batches(self):
        first=self.handle();second=self.handle()
        self.assertEqual(first['name'],NAMES[0]);self.assertEqual(second['name'],NAMES[0])
        self.assertNotEqual(first['id'],second['id'])
    def test_document_workflow_does_not_require_system_control_tools(self):
        self.value['tools']=self.value['tools'][:3]
        self.allowed=NAMES
        self.assertEqual(self.handle()['name'],NAMES[0])
    def test_caller_quantity_bound_is_not_bypassed(self):
        self.value['tools'][0]['function']['parameters']['properties']['count']['maximum']=2
        self.assertIn('停止',self.handle()['content'])
    def test_tool_choice_and_disabled_backend_cannot_be_bypassed(self):
        self.value['tool_choice']='none'
        def forbidden(_body):self.fail('No planning when tools are disabled')
        self.assertIsNone(self.planner.handle(json.dumps(self.value).encode(),'model',self.allowed,forbidden,document_tasks=True))
        self.setUp();call=self.discovery();append(self.value,call,dict(self.rows[0],batch_id='a'*32,text='正文。'))
        self.assertIn('停止',self.planner.handle(json.dumps(self.value).encode(),'model',self.allowed,infer,document_tasks=False)['content'])
    def test_existing_system_prompt_request_unchanged_without_document_evidence(self):
        text='音量调节成30%。';items=measurements(text)
        self.assertEqual(request('model',text,items),request('model',text,items,document_tasks=True))
    def test_bad_quantity_teaching_or_negation_does_not_bind_document_workflow(self):
        for text in ('如何查找最近三份文档生成摘要保存桌面？','不要生成三份文档摘要保存桌面。','把音量设为30%，总结文档保存桌面。',
                     '解释“查找最近三份文档生成摘要保存桌面”。','找最近三份文件，别做摘要也别保存桌面。'):
            self.assertIsNone(task_schema(text,measurements(text)))
        for text in ('在下载目录找最近三份文件总结保存桌面。','将桌面上的最近三份文档总结保存桌面。',
                     '解释‘查找最近三份文档摘要保存桌面’。','读最近修改的第三份文档生成摘要保存桌面。'):
            self.assertIsNone(task_schema(text,measurements(text)))
        wrong=response({'intent':'documents','directory':'documents','count':'q9','destination':'desktop','order':'modified_desc'})
        with self.assertRaises(ValueError):decode_plan(wrong,self.question,measurements(self.question))
    def test_summary_selection_never_fabricates_text_and_bounds_context(self):
        text=''.join(f'第{i}项真实内容包含数字{i}。' for i in range(170))
        calls=[]
        def select(body):
            value=json.loads(body['messages'][-1]['content']);calls.append(value)
            self.assertLessEqual(sum(len(x['text']) for x in value['segments']),1200)
            return response({'selected':[row['id'] for row in value['segments'][-value['select_count']:]]})
        points=select_points(text,'model',select,lambda:False)
        self.assertLessEqual(len(points),3);self.assertTrue(all(point in text for point in points));self.assertGreater(len(calls),1)
        with self.assertRaises(ValueError):select_points('真实内容。','model',lambda b:response({'selected':['fake']}),lambda:False)


if __name__=='__main__':unittest.main()

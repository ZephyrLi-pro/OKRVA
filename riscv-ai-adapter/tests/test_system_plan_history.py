"""A native Gateway may remove old conversation messages between tool steps."""
import copy
import json
import unittest
from riscv_ai_adapter.semantic_planner import SemanticPlanner, OPERATIONS


def response(value):
    return {'choices': [{'finish_reason': 'stop', 'message': {'content': json.dumps(value)}}]}


def payload():
    tools = [{'type': 'function', 'function': {'name': name, 'parameters': {
        'type': 'object', 'properties': props, 'required': list(props)}}}
        for name, (_, props) in OPERATIONS.items()]
    return {'messages': [{'role': 'system', 'content': 'system'},
        {'role': 'user', 'content': '前一轮问题'}, {'role': 'assistant', 'content': '前一轮回答'},
        {'role': 'user', 'content': '另一个旧问题'}, {'role': 'assistant', 'content': '另一个旧回答'},
        {'role': 'user', 'content': '音量调大一点，然后打开系统设置。'}], 'tools': tools}


def append(value, call):
    value['messages'] += [{'role': 'assistant', 'tool_calls': [{'type': 'function', 'id': call['id'],
        'function': {'name': call['name'], 'arguments': json.dumps(call['arguments'])}}]},
        {'role': 'tool', 'tool_call_id': call['id'], 'content': json.dumps({
            'isError': False, 'content': [{'type': 'text', 'text': '原生工具已回读确认。'}]})}]


class SystemPlanHistoryTests(unittest.TestCase):
    def setUp(self):
        self.value=payload();self.planner=SemanticPlanner(latest_only=True)
        self.inferences=0

    def infer(self, _):
        self.inferences += 1
        return response({'intent':'operate','actions':[
            {'operation':'increase_volume','value':10}, {'operation':'open_settings','value':None}]})

    def handle(self, model='model'):
        return self.planner.handle(json.dumps(self.value).encode(), model, tuple(OPERATIONS), self.infer)

    def first(self):
        call=self.handle();self.assertEqual(call['name'],'sys__adjust_volume')
        append(self.value,call)
        return call

    def test_leading_gateway_history_removal_preserves_active_plan(self):
        self.first()
        self.value['messages']=self.value['messages'][3:]
        call=self.handle();self.assertEqual(call.get('name'),'sys__open_application',call)
        self.assertEqual(call['arguments'],{'application':'system_settings'})
        append(self.value,call)
        self.value['messages']=self.value['messages'][2:]
        result=self.handle()
        self.assertNotIn('停止',result['content'])
        self.assertEqual(self.inferences,1,'Pruning must not repeat planning or earlier effects')

    def test_history_or_current_request_mutation_is_still_rejected(self):
        for mode in ('edit','insert','reorder','current_user','catalog','model','restore'):
            with self.subTest(mode=mode):
                self.setUp();self.first();original=copy.deepcopy(self.value['messages'])
                model='model'
                if mode=='edit':self.value['messages'][2]['content']='changed'
                elif mode=='insert':self.value['messages'].insert(1,{'role':'assistant','content':'new'})
                elif mode=='reorder':self.value['messages'][1:3]=list(reversed(self.value['messages'][1:3]))
                elif mode=='current_user':self.value['messages'][5]['content']='音量设成100%'
                elif mode=='catalog':self.value['tools'][0]['function']['parameters']['description']='changed'
                elif mode=='model':model='other-model'
                elif mode=='restore':
                    self.value['messages']=self.value['messages'][3:]
                    call=self.handle();self.assertEqual(call.get('name'),'sys__open_application')
                    append(self.value,call)
                    self.value['messages']=original[:3]+self.value['messages']
                result=self.handle(model)
                self.assertIn('停止',result['content'])

    def test_changed_native_arguments_or_results_never_advance(self):
        for mode in ('args','id','result'):
            with self.subTest(mode=mode):
                self.setUp();self.first()
                self.value['messages']=self.value['messages'][3:]
                if mode=='args':self.value['messages'][-2]['tool_calls'][0]['function']['arguments']='{"delta":100}'
                elif mode=='id':self.value['messages'][-1]['tool_call_id']='unissued'
                else:self.value['messages'][-1]['content']='{"isError":true,"content":[]}'
                result=self.handle();self.assertIn('停止',result['content'])

    def test_new_turn_ignores_completed_plan_and_gets_a_new_id(self):
        first=self.first();second=self.handle();append(self.value,second)
        done=self.handle();self.value['messages'].append({'role':'assistant','content':done['content']})
        self.value['messages'].append({'role':'user','content':'再帮我把音量调大一点，然后打开系统设置。'})
        call=self.handle()
        self.assertEqual(call['name'],'sys__adjust_volume')
        self.assertNotEqual(call['id'],first['id']);self.assertEqual(self.inferences,2)


if __name__=='__main__':unittest.main()

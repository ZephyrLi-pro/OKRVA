import json
import unittest
from riscv_ai_adapter.document_workflow import SummaryCache

def response(value):return {'choices':[{'finish_reason':'stop','message':{'content':json.dumps(value)}}]}

class SummaryCacheTests(unittest.TestCase):
    def setUp(self):
        self.now=0;self.calls=0
        self.cache=SummaryCache(limit=2,ttl=10,clock=lambda:self.now)
    def infer(self,request):
        self.calls+=1
        value=json.loads(request['messages'][-1]['content'])
        return response({'selected':[row['id'] for row in value['segments'][:value['select_count']]]})
    def get(self,text='第一条事实。第二条事实。',model='model'):
        return self.cache.points(text,model,self.infer,lambda:False)
    def test_exact_source_reuses_model_selection_without_sharing_mutable_lists(self):
        first=self.get();first.append('污染')
        self.assertEqual(self.get(),['第一条事实。','第二条事实。'])
        self.assertEqual(self.calls,1)
    def test_source_or_model_change_recomputes_instead_of_returning_stale_points(self):
        self.get();self.get('新的事实。');self.get(model='another-model')
        self.assertEqual(self.calls,3)
    def test_expiration_and_eviction_recompute(self):
        self.get();self.now=11;self.get();self.assertEqual(self.calls,2)
        self.get('文档乙。');self.get('文档丙。');self.get()
        self.assertEqual(self.calls,5)
    def test_invalid_model_response_is_not_cached(self):
        with self.assertRaises(ValueError):
            self.cache.points('真实事实。','model',lambda r:response({'selected':['fake']}),lambda:False)
        self.assertEqual(self.get('真实事实。'),['真实事实。'])
    def test_cancelled_cache_hit_does_not_continue_document_task(self):
        self.get()
        with self.assertRaises(ConnectionResetError):
            self.cache.points('第一条事实。第二条事实。','model',self.infer,lambda:True)
        self.assertEqual(self.calls,1)

if __name__=='__main__':unittest.main()

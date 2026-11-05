import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('history_cleanup',ROOT/'helpers/clear-conversation-history.py')
cleanup=importlib.util.module_from_spec(spec);spec.loader.exec_module(cleanup)


class Gateway:
    def __init__(self):
        self.sessions={'text-id':{'name':'text'},'voice-id':{'name':'voice-assistant'}}
        self.deleted=[];self.aborted=[];self.offline=False;self.failure=set();self.running=set()

    def request(self,method,path):
        if self.offline:raise OSError('offline')
        if path=='/api/sessions':return {'sessions':[dict(session_id=k,**v) for k,v in self.sessions.items()]}
        identifier=path.split('/')[3]
        if path.endswith('/abort'):
            self.aborted.append(identifier)
            if identifier in self.running:raise RuntimeError('Not stopped')
            return {'status':'no_active_response'}
        if path.endswith('/state'):return {'state':'completed'}
        if method=='DELETE':
            if identifier in self.failure:raise RuntimeError('Temporary delete failure')
            self.sessions.pop(identifier,None);self.deleted.append(identifier);return {'deleted':True}
        raise AssertionError((method,path))


class HistoryCleanupTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.home=Path(self.tmp.name);self.gateway=Gateway()
        self.db=self.home/'.config/kylin-aiassistant/kylin_aiassistant_database.db'
        self.db.parent.mkdir(parents=True)
        with sqlite3.connect(self.db) as db:
            db.executescript('''CREATE TABLE RECORD(sessionID TEXT,session_uuid TEXT,message TEXT);
                CREATE TABLE DOCUMENT_REFERENCE(sessionID TEXT,documents TEXT);
                CREATE TABLE HISTORY_ID(history_id INTEGER);
                INSERT INTO HISTORY_ID VALUES(42);
                CREATE TABLE MEETINGRECORD(message TEXT);
                INSERT INTO MEETINGRECORD VALUES('meeting preserved');''')
            db.executemany('INSERT INTO RECORD VALUES(?,?,?)',[
                ('1','text-id','text history'),('2','voice-id','voice history'),
                ('v_3','voice-assistant','legacy voice')])
            db.executemany('INSERT INTO DOCUMENT_REFERENCE VALUES(?,?)',[('1','reference'),('2','voice ref')])
        (self.home/'report.md').write_text('user output remains')

    def call(self,scope):return cleanup.cleanup(self.home,self.home/'run',scope,self.gateway)
    def ids(self):
        with sqlite3.connect(self.db) as db:return [row[0] for row in db.execute('SELECT sessionID FROM RECORD ORDER BY sessionID')]

    def test_all_clears_conversations_but_preserves_ids_meetings_and_files(self):
        result=self.call('all');self.assertEqual(self.ids(),[])
        self.assertEqual(set(self.gateway.deleted),{'text-id','voice-id'})
        self.assertEqual(result['pending_sessions'],0)
        with sqlite3.connect(self.db) as db:
            self.assertEqual(db.execute('SELECT count(*) FROM DOCUMENT_REFERENCE').fetchone()[0],0)
            self.assertEqual(db.execute('SELECT history_id FROM HISTORY_ID').fetchone()[0],42)
            self.assertEqual(db.execute('SELECT message FROM MEETINGRECORD').fetchone()[0],'meeting preserved')
        self.assertEqual((self.home/'report.md').read_text(),'user output remains')
        self.assertEqual(self.call('all')['local_rows_removed'],0)

    def test_voice_exit_does_not_clear_live_text_history(self):
        self.call('voice');self.assertEqual(self.ids(),['1'])
        self.assertEqual(self.gateway.deleted,['voice-id'])

    def test_text_exit_does_not_clear_voice_history(self):
        self.call('text');self.assertEqual(self.ids(),['2','v_3'])
        self.assertEqual(self.gateway.deleted,['text-id'])

    def test_failed_delete_retries_only_ids_no_message_or_credentials(self):
        self.gateway.failure.add('voice-id')
        self.assertEqual(self.call('voice')['pending_sessions'],1)
        pending=self.home/'.local/state/okrva/conversation-cleanup/pending.json'
        self.assertEqual(json.loads(pending.read_text()),{'text':[],'voice':['voice-id']})
        self.assertEqual(pending.stat().st_mode&0o777,0o600)
        self.gateway.failure.clear();self.assertEqual(self.call('voice')['pending_sessions'],0)

    def test_running_response_is_not_deleted_before_abort_confirmation(self):
        self.gateway.running.add('text-id')
        self.assertEqual(self.call('text')['pending_sessions'],1)
        self.assertEqual(self.gateway.deleted,[])
        self.gateway.running.clear();self.assertEqual(self.call('text')['pending_sessions'],0)

    def test_missing_database_does_not_create_an_empty_database(self):
        self.db.unlink();self.call('all');self.assertFalse(self.db.exists())

    def test_offline_gateway_still_clears_local_records_and_retries_next_boundary(self):
        self.gateway.offline=True;self.assertFalse(self.call('all')['gateway_available'])
        self.assertEqual(self.ids(),[])
        self.gateway.offline=False;self.call('all')
        self.assertEqual(self.gateway.sessions,{})

    def test_voice_prefix_survives_missing_gateway_name_and_offline_cleanup(self):
        identifier=cleanup.VOICE_PREFIX+'test'
        self.gateway.sessions={identifier:{}}
        with sqlite3.connect(self.db) as db:
            db.execute('UPDATE RECORD SET session_uuid=? WHERE sessionID=?',(identifier,'2'))
        self.gateway.offline=True
        result=self.call('voice')
        self.assertEqual(self.ids(),['1'])
        self.assertEqual(result['pending_sessions'],1)
        self.gateway.offline=False
        self.call('voice');self.assertEqual(self.gateway.deleted,[identifier])


if __name__=='__main__':unittest.main()

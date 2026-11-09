import copy
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
import types
from unittest.mock import patch

from presentation_mcp import PresentationTools,PresentationError,ReaderClient,serve,sha


class FakeReader:
    def __init__(self,path):
        self.path=str(path);self.page=2;self.total=4;self.opened=[];self.saved=[]
        self.text='设备通过语音识别用户请求，手势操作用于翻阅演示文稿。'
        self.notes='原有的演讲者备注。';self.failure=None;self.reply_transform=lambda r:r
        self.state_value=None;self.state_failure=None

    def recent(self):return self.state()

    def state(self):
        if self.state_failure:raise self.state_failure
        if self.state_value is not None:return copy.deepcopy(self.state_value)
        return {'reader_owner':':1.7','session_id':'session-one','source_path':self.path,
                'active':True,'mode':'document','page':self.page,'total':self.total}

    def open(self,path):
        self.path=path;self.opened.append(path)
        return {'ok':True,'active':True,'source_path':path,'session_id':'session-one','page':self.page,'total':self.total}

    def snapshot(self):
        return {'ok':True,'session_id':'session-one','source_path':self.path,'page':self.page,'total':self.total,
            'title':'演示文稿','page_title':'多模态交互','text':self.text,'text_sha256':sha(self.text),'notes':self.notes}

    def save(self,session,page,digest,notes):
        self.saved.append((session,page,digest,notes))
        if self.failure:raise self.failure
        final=self.notes+'\n\nAI演讲备注：\n'+notes
        self.notes=final
        return self.reply_transform({'ok':True,'page':page,'path':self.path,'notes':final,
            'notes_sha256':sha(final),'verified':True,'notes_visible':True,
            'display_mode':'normal','notes_pane_supported':True,'normal_view_visible':True})


@unittest.skipUnless(os.name=='posix','Linux scoped-path guarantees')
class PresentationTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name);self.docs=self.base/'文档';self.desktop=self.base/'Desktop';self.demo=self.base/'demo'
        for path in (self.docs,self.desktop,self.demo):path.mkdir()
        self.deck=self.demo/'default.pptx';self.deck.write_bytes(b'fake presentation for scope validation')
        self.reader=FakeReader(self.deck);self.now=10
        self.tools=PresentationTools(self.docs,self.desktop,self.demo,self.deck,self.reader,clock=lambda:self.now)
        self.notes='各位好，这一页介绍多模态交互的具体方式。系统通过语音识别理解用户请求，同时使用手势控制演示文稿翻页，两种方式共同完成页面阅读与操作。'

    def test_default_and_exact_named_open_are_verified(self):
        result=self.tools.open('PPT')
        self.assertTrue(result['opened']);self.assertEqual(result['path'],str(self.deck))
        other=self.docs/'介绍.PPTX';other.write_bytes(b'named')
        result=self.tools.open('介绍')
        self.assertEqual(result['path'],str(other));self.assertEqual(self.reader.opened,[str(self.deck),str(other)])

    def test_exact_name_precedes_partial_but_duplicates_never_choose(self):
        a=self.docs/'报告.pptx';a.write_bytes(b'one')
        (self.desktop/'年度报告.pptx').write_bytes(b'two')
        self.assertEqual(self.tools.open('报告')['path'],str(a))
        (self.desktop/'报告.pptx').write_bytes(b'three')
        with self.assertRaisesRegex(PresentationError,'多份'):self.tools.open('报告')

    def assert_no_recent(self,query):
        result=self.tools.open(query)
        self.assertEqual(result,{'ok':False,'opened':False,'reason':'no_recent',
                                'message':'没有可用的最近打开记录，请先打开一次PPT。'})
        self.assertFalse(self.reader.opened)

    def test_no_history_never_searches_unique_or_multiple_files_or_default(self):
        self.reader.state_failure=PresentationError('reader unavailable')
        for query in ('','PPT','最近:'):
            self.assert_no_recent(query)
        (self.docs/'second.pptx').write_bytes(b'two')
        for query in ('','PPT','最近:'):
            self.assert_no_recent(query)

    def test_live_recent_precedes_legacy_default_and_file_mtime(self):
        current=self.desktop/'刚才打开的.pptx';current.write_bytes(b'real history')
        os.utime(current,(1,1));self.reader.path=str(current)
        result=self.tools.open('PPT')
        self.assertEqual(result['path'],str(current));self.assertEqual(result['selection_reason'],'reader_current')
        self.assertEqual(self.reader.opened,[str(current)])

    def test_closed_reader_recent_is_still_a_real_selection_context(self):
        recent=self.desktop/'closed.pptx';recent.write_bytes(b'recent')
        self.reader.state_value=dict(self.reader.state(),active=False,mode='closed',page=0,total=0,source_path=str(recent))
        result=self.tools.open('最近:')
        self.assertEqual(result['path'],str(recent));self.assertEqual(result['selection_reason'],'reader_recent')

    def test_legacy_default_may_be_missing_but_never_substitutes_for_history(self):
        tools=PresentationTools(self.docs,self.desktop,self.demo,self.base/'missing.pptx',self.reader)
        self.reader.state_failure=PresentationError('unavailable')
        result=tools.open('PPT')
        self.assertEqual(result['reason'],'no_recent');self.assertFalse(self.reader.opened)

    def test_malformed_or_unscoped_recent_context_is_never_selected(self):
        outside=self.base/'outside.pptx';outside.write_bytes(b'outside')
        link=self.desktop/'linked.pptx';link.symlink_to(self.deck)
        normal=self.reader.state()
        changes=[{'reader_owner':None},{'reader_owner':'org.riscv.AIReader'},
                 {'session_id':None},{'source_path':str(outside)},{'source_path':str(link)},
                 {'source_path':str(self.docs/'missing.pptx')},{'source_path':None},
                 {'active':1},{'page':True},{'mode':'unknown'},
                 {'active':False,'mode':'closed'},
                 {'active':False,'mode':'document','page':0,'total':0}]
        for change in changes:
            with self.subTest(change=change):
                self.reader.state_value=dict(normal,**change)
                self.assert_no_recent('PPT')

    def test_deleted_recent_file_does_not_choose_another_existing_ppt(self):
        self.reader.path=str(self.docs/'deleted.pptx')
        self.assert_no_recent('最近:')

    def test_named_open_needs_no_recent_history_or_reader_discovery(self):
        target=self.docs/'指定名字.pptx';target.write_bytes(b'explicit')
        self.reader.state=lambda:(_ for _ in ()).throw(AssertionError('name must not discover history'))
        result=self.tools.open('指定名字')
        self.assertEqual(result['path'],str(target));self.assertEqual(result['selection_reason'],'exact_name')
        literal=self.docs/'最近：项目.pptx';literal.write_bytes(b'literal fullwidth colon')
        self.assertEqual(self.tools.open('最近：项目')['path'],str(literal))

    def test_changed_file_identity_during_open_never_claims_verified_success(self):
        original=self.reader.open
        def changed(path):
            reply=original(path)
            replacement=self.demo/'replacement.pptx';replacement.write_bytes(b'replaced')
            replacement.replace(self.deck)
            return reply
        self.reader.open=changed
        with self.assertRaisesRegex(PresentationError,'身份已改变'):self.tools.open('PPT')

    def test_no_recent_is_a_non_error_mcp_result_without_candidates(self):
        self.reader.state_failure=PresentationError('unavailable')
        request={'jsonrpc':'2.0','id':1,'method':'tools/call','params':
                 {'name':'open_presentation','arguments':{'query':'最近:'}}}
        output=io.StringIO()
        with patch('presentation_mcp.sys.stdin',io.StringIO(json.dumps(request)+'\n')),patch('presentation_mcp.sys.stdout',output):serve(self.tools)
        result=json.loads(output.getvalue())['result']
        self.assertFalse(result['isError'])
        payload=json.loads(result['content'][0]['text'])
        self.assertEqual(payload['reason'],'no_recent');self.assertNotIn('candidates',payload)

    def test_names_never_become_paths(self):
        for query in ('../../etc/passwd','/home/me/deck.pptx','C:\\secret.pptx','~','abc\x00ppt','..','x/y'):
            with self.subTest(query=query),self.assertRaises(PresentationError):self.tools.open(query)
        self.assertFalse(self.reader.opened)

    def test_symlink_files_and_directories_are_not_discovered(self):
        external=self.base/'external';external.mkdir();(external/'secret.pptx').write_bytes(b'secret')
        (self.docs/'link.pptx').symlink_to(external/'secret.pptx')
        (self.docs/'folder').symlink_to(external,target_is_directory=True)
        for name in ('secret','link'):
            with self.assertRaisesRegex(PresentationError,'未找到'):self.tools.open(name)
        with self.assertRaises(PresentationError):PresentationTools(self.docs/'folder',self.desktop)

    def test_replaced_root_fails_closed(self):
        self.docs.rename(self.base/'original');self.docs.mkdir()
        with self.assertRaisesRegex(PresentationError,'改变'):self.tools.open('PPT')

    def test_snapshot_preserves_full_text_and_rejects_long_or_hash_mismatch(self):
        self.reader.text='完整原文。'*1000
        value=self.tools.current()
        self.assertEqual(value['text'],self.reader.text);self.assertEqual(value['tool'],'ppt__get_current_slide')
        self.reader.text='长'*12001
        with self.assertRaises(PresentationError):self.tools.current()
        self.reader.text='正文'
        original=self.reader.snapshot
        self.reader.snapshot=lambda:dict(original(),text_sha256='0'*64)
        with self.assertRaises(PresentationError):self.tools.current()

    def test_snapshot_path_must_remain_scoped(self):
        outside=self.base/'outside.pptx';outside.write_bytes(b'outside');self.reader.path=str(outside)
        with self.assertRaises(PresentationError):self.tools.current()
        link=self.docs/'linked.pptx';link.symlink_to(self.deck);self.reader.path=str(link)
        with self.assertRaises(PresentationError):self.tools.current()

    def test_generating_while_navigating_saves_original_page_once(self):
        snap=self.tools.current();self.reader.page=4
        result=self.tools.save(snap['snapshot_id'],self.notes)
        self.assertEqual(self.reader.saved[0][:3],('session-one',2,snap['text_sha256']))
        self.assertEqual(result['page'],2);self.assertTrue(result['verified']);self.assertTrue(result['notes_visible']);self.assertNotIn('backup_path',result)
        self.assertEqual(result['tool'],'ppt__save_speaker_notes');self.assertTrue(result['notes'].endswith(self.notes))
        self.assertTrue(result['notes'].startswith('原有的演讲者备注。'));self.assertEqual(sha(result['notes']),result['notes_sha256'])
        self.assertEqual(self.tools.save(snap['snapshot_id'],self.notes),result);self.assertEqual(len(self.reader.saved),1)
        with self.assertRaises(PresentationError):self.tools.save(snap['snapshot_id'],self.notes+'新的内容。')

    def test_ppt_file_changed_before_save_never_writes(self):
        snap=self.tools.current();self.deck.write_bytes(b'modified during generation')
        with self.assertRaisesRegex(PresentationError,'已改变'):self.tools.save(snap['snapshot_id'],self.notes)
        self.assertFalse(self.reader.saved)

    def test_non_pptx_is_readable_but_notes_save_refused(self):
        for suffix in ('.ppt','.odp'):
            file=self.docs/('old'+suffix);file.write_bytes(b'legacy');self.reader.path=str(file)
            snap=self.tools.current()
            with self.assertRaisesRegex(PresentationError,'PPTX'):self.tools.save(snap['snapshot_id'],self.notes)
        self.assertFalse(self.reader.saved)

    def test_bad_snapshot_id_and_bad_notes_do_not_write(self):
        snap=self.tools.current()
        for bad in ('../../notes','0'*32,True):
            with self.assertRaises(PresentationError):self.tools.save(bad,self.notes)
        for bad in ('too short','字'*1501,self.notes+'\x00',True):
            with self.assertRaises(PresentationError):self.tools.save(snap['snapshot_id'],bad)
        self.assertFalse(self.reader.saved)

    def test_expired_snapshot_never_writes(self):
        snap=self.tools.current();self.now+=3601
        with self.assertRaisesRegex(PresentationError,'过期'):self.tools.save(snap['snapshot_id'],self.notes)
        self.assertFalse(self.reader.saved)

    def test_unconfirmed_save_cannot_be_retried(self):
        snap=self.tools.current();self.reader.reply_transform=lambda v:dict(v,verified=False)
        with self.assertRaisesRegex(PresentationError,'可能已写入'):self.tools.save(snap['snapshot_id'],self.notes)
        with self.assertRaises(PresentationError):self.tools.save(snap['snapshot_id'],self.notes)
        self.assertEqual(len(self.reader.saved),1)

    def test_hash_and_preservation_must_both_be_verified(self):
        for transform in (lambda v:dict(v,notes_sha256='f'*64),
            lambda v:dict(v,notes=self.notes,notes_sha256=sha(self.notes)),
            lambda v:dict(v,page=3),lambda v:dict(v,path='/outside.pptx')):
            with self.subTest(transform=transform):
                self.reader.notes='原有的演讲者备注。';snap=self.tools.current();self.reader.reply_transform=transform
                with self.assertRaises(PresentationError):self.tools.save(snap['snapshot_id'],self.notes)

    def test_uncertain_save_failure_is_not_repeated(self):
        snap=self.tools.current();self.reader.failure=PresentationError('连接断开，结果未知')
        for _ in range(2):
            with self.assertRaises(PresentationError):self.tools.save(snap['snapshot_id'],self.notes)
        self.assertEqual(len(self.reader.saved),1)

    def test_open_wrong_file_or_inactive_never_claims_success(self):
        for value in ({'active':False},{'active':True,'source_path':'/wrong','session_id':'x','page':1,'total':4}):
            self.reader.open=lambda path:value
            with self.assertRaisesRegex(PresentationError,'未确认'):self.tools.open('PPT')

    def test_tool_schema_strict_and_response_round_trip(self):
        schemas=self.tools.schemas();self.assertEqual(len(schemas),3)
        for schema in schemas:self.assertFalse(schema['inputSchema']['additionalProperties'])
        for name,args in (('get_current_slide',{'page':99}),('open_presentation',{'path':str(self.deck)}),('bad',{})):
            with self.assertRaises(PresentationError):self.tools.call(name,args)
        result=json.loads(self.tools.call('get_current_slide',{}));self.assertEqual(result['page'],2)

    def test_jsonrpc_errors_do_not_kill_server_and_tools_return_is_error(self):
        messages=['not json',json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'bad','arguments':{}}}),
            json.dumps({'jsonrpc':'2.0','id':2,'method':'tools/list'})]
        output=io.StringIO()
        with patch('presentation_mcp.sys.stdin',io.StringIO('\n'.join(messages)+'\n')),patch('presentation_mcp.sys.stdout',output):serve(self.tools)
        rows=[json.loads(line) for line in output.getvalue().splitlines()]
        self.assertIn('error',rows[0]);self.assertTrue(rows[1]['result']['isError']);self.assertEqual(len(rows[2]['result']['tools']),3)

    def test_189mb_deck_is_allowed_and_more_than_512mb_is_rejected(self):
        with self.deck.open('r+b') as out:out.truncate(189*1024*1024)
        self.assertEqual(self.tools.open('PPT')['path'],str(self.deck))
        with self.deck.open('r+b') as out:out.truncate(512*1024*1024)
        self.assertEqual(self.tools.open('PPT')['path'],str(self.deck))
        with self.deck.open('r+b') as out:out.truncate(512*1024*1024+1)
        with self.assertRaisesRegex(PresentationError,'512MB'):self.tools.open('default')

    def test_legacy_libreoffice_normal_view_reports_saved_without_notes_pane(self):
        self.reader.reply_transform=lambda v:dict(v,notes_visible=False,notes_pane_supported=False)
        snap=self.tools.current();result=self.tools.save(snap['snapshot_id'],self.notes)
        self.assertTrue(result['verified']);self.assertFalse(result['notes_visible'])
        self.assertFalse(result['notes_pane_supported']);self.assertTrue(result['normal_view_visible'])
        self.assertEqual(result['display_mode'],'normal')

    def test_modern_libreoffice_normal_view_reports_saved_with_notes_pane(self):
        snap=self.tools.current();result=self.tools.save(snap['snapshot_id'],self.notes)
        self.assertTrue(result['verified']);self.assertTrue(result['notes_visible'])
        self.assertTrue(result['notes_pane_supported']);self.assertTrue(result['normal_view_visible'])
        self.assertEqual(result['display_mode'],'normal')

    def test_display_fields_required_and_normal_view_combinations_consistent(self):
        transformations=[
            lambda v:dict(v,notes_visible=False),
            lambda v:dict(v,notes_pane_supported=False),
            lambda v:dict(v,normal_view_visible=False),
            lambda v:dict(v,display_mode='NotesMode'),
            lambda v:dict(v,display_mode='notes'),
        ]
        for key in ('notes_visible','notes_pane_supported','normal_view_visible','display_mode'):
            transformations.append(lambda v,key=key:{k:x for k,x in v.items() if k!=key})
        for key in ('notes_visible','notes_pane_supported','normal_view_visible'):
            for fake_bool in (0,1,'true','false',None):
                transformations.append(lambda v,key=key,fake_bool=fake_bool:dict(v,**{key:fake_bool}))
        for transform in transformations:
            with self.subTest(transform=transform):
                self.reader.notes='原有的演讲者备注。';snap=self.tools.current()
                self.reader.reply_transform=transform
                with self.assertRaisesRegex(PresentationError,'未通过保存回读确认'):
                    self.tools.save(snap['snapshot_id'],self.notes)

    def test_recent_activates_only_lazy_bridge_then_reads_live_state(self):
        client=ReaderClient();calls=[]
        async def read(member,signature,body):
            calls.append((member,signature,body));return {'active':False}
        with patch.object(client,'start') as start,patch.object(client,'_call',side_effect=read):
            self.assertEqual(client.recent(),{'active':False})
        start.assert_called_once_with()
        self.assertEqual(calls,[('GetState','',[])])

    def test_recent_state_is_tied_to_the_live_bus_owner_without_autostart(self):
        class Message:
            def __init__(self,**values):self.__dict__.update(values)
        for sender in (':1.7',':1.999'):
            calls=[]
            class Bus:
                async def connect(self):return self
                async def call(self,message):
                    calls.append(message)
                    if message.member=='GetNameOwner':return types.SimpleNamespace(message_type='return',body=[':1.7'])
                    return types.SimpleNamespace(message_type='return',sender=sender,body=[json.dumps(self_reader.state())])
                def disconnect(self):pass
            self_reader=self.reader
            modules={'dbus_next':types.SimpleNamespace(Message=Message,MessageType=types.SimpleNamespace(ERROR='error'),
                        MessageFlag=types.SimpleNamespace(NO_AUTOSTART='no-autostart')),
                     'dbus_next.aio':types.SimpleNamespace(MessageBus=lambda **kwargs:Bus())}
            with self.subTest(sender=sender),patch.dict('sys.modules',modules),patch.object(ReaderClient,'start',side_effect=AssertionError('no startup')):
                if sender==':1.7':self.assertEqual(ReaderClient().state()['reader_owner'],':1.7')
                else:
                    with self.assertRaises(PresentationError):ReaderClient().state()
            self.assertEqual([call.member for call in calls],['GetNameOwner','GetState'])
            self.assertEqual(calls[-1].destination,':1.7')
            self.assertEqual(calls[-1].flags,'no-autostart')

    def test_get_and_save_never_start_reader(self):
        client=ReaderClient()
        async def fake_call(member,signature,body):return {'member':member,'body':body}
        with patch.object(client,'start',side_effect=AssertionError('unexpected startup')),patch.object(client,'_call',side_effect=fake_call):
            self.assertEqual(client.snapshot()['member'],'GetSlideSnapshot')
            self.assertEqual(client.save('sid',2,'sha',self.notes)['body'],['sid',2,'sha',self.notes])


if __name__=='__main__':unittest.main()

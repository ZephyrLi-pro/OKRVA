import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile
import hashlib
from unittest.mock import patch
import shutil
from document_mcp import DocumentTools,DocumentError,user_directory,normalize_native_text


@unittest.skipUnless(os.name=='posix','Linux filesystem guarantees')
class DocumentTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.docs=self.root/'文档';self.desktop=self.root/'Desktop'
        self.docs.mkdir();self.desktop.mkdir()
        self.tools=DocumentTools(self.docs,self.desktop,os.environ.get('DOCUMENT_READER_CLI','/missing-reader'))

    def files(self):
        for index,name in enumerate(('z.txt','a.md','y.txt','b.md')):
            path=self.docs/name;path.write_text('正文：'+name+'。不能编造。')
            os.utime(path,ns=(1000000000+index,1000000000+index))

    def batch(self,count=3):return self.tools.recent('documents',count,'modified_desc')

    def entries(self,batch):
        result=[]
        for row in batch['documents']:
            value=self.tools.read(batch['batch_id'],row['document_id'])
            result.append({'document_id':row['document_id'],'points':[value['text']]})
        return result

    def test_mtime_not_filename_and_recursive_discovery(self):
        self.files();nested=self.docs/'sub';nested.mkdir();p=nested/'x.txt';p.write_text('最新')
        value=self.batch()
        self.assertEqual([r['name'] for r in value['documents']],['sub/x.txt','b.md','y.txt'])

    def test_user_directory_is_not_hardcoded_english(self):
        config=self.root/'.config';config.mkdir()
        (config/'user-dirs.dirs').write_text('XDG_DOCUMENTS_DIR="$HOME/文档"\nXDG_DESKTOP_DIR="$HOME/Desktop"\n')
        self.assertEqual(user_directory('documents',self.root),self.docs)

    def test_symlink_escape_not_discovered_and_newest_unsupported_not_skipped(self):
        outside=self.root/'secret.txt';outside.write_text('不得读取')
        (self.docs/'link.txt').symlink_to(outside);(self.docs/'image.png').write_bytes(b'not-text')
        self.files();value=self.batch(4)
        self.assertNotIn('link.txt',[r['name'] for r in value['documents']])
        self.assertEqual(value['documents'][0]['name'],'image.png')
        with self.assertRaises(DocumentError):self.tools.read(value['batch_id'],value['documents'][0]['document_id'])
    def test_newest_empty_or_oversized_file_is_not_silently_substituted(self):
        self.files();empty=self.docs/'empty.txt';empty.write_text('')
        batch=self.batch();self.assertEqual(batch['documents'][0]['name'],'empty.txt')
        with self.assertRaises(DocumentError):self.tools.read(batch['batch_id'],batch['documents'][0]['document_id'])
        with empty.open('wb') as out:out.truncate(21*1024*1024)
        batch=self.batch()
        with self.assertRaises(DocumentError):self.tools.read(batch['batch_id'],batch['documents'][0]['document_id'])

    def test_bad_count_and_insufficient_files_stop(self):
        self.files()
        for count in (True,0,6,1.5):
            with self.assertRaises(DocumentError):self.batch(count)
        with self.assertRaises(DocumentError):self.batch(5)

    def test_unknown_ids_never_become_paths(self):
        self.files();batch=self.batch()
        for batch_id,document_id in (('/etc/passwd','x'),(batch['batch_id'],'../../etc/passwd')):
            with self.assertRaises(DocumentError):self.tools.read(batch_id,document_id)
    def test_native_wrapped_lines_join_without_losing_facts(self):
        text='===== Slide 1 =====\nFirmware maintenance\n• Firmware R2 fixes missing audio after\n  suspend.\n• Camera issue remains unresolved.'
        value=normalize_native_text(text)
        self.assertIn('Firmware R2 fixes missing audio after suspend.',value)
        self.assertIn('Camera issue remains unresolved.',value)
    def test_desktop_symlink_swap_cannot_redirect_report(self):
        self.files();batch=self.batch();entries=self.entries(batch)
        outside=self.root/'outside';outside.mkdir();self.desktop.rename(self.root/'original-desktop');self.desktop.symlink_to(outside,target_is_directory=True)
        with self.assertRaises((OSError,DocumentError)):self.tools.save(batch['batch_id'],entries)
        self.assertFalse(list(outside.iterdir()))

    def test_changed_document_rejected_before_read_and_save(self):
        self.files();batch=self.batch();row=batch['documents'][0]
        Path(row['path']).write_text('已更改')
        with self.assertRaises(DocumentError):self.tools.read(batch['batch_id'],row['document_id'])
        batch=self.batch();entries=self.entries(batch)
        Path(batch['documents'][0]['path']).write_text('再次改变')
        with self.assertRaises(DocumentError):self.tools.save(batch['batch_id'],entries)
        self.assertFalse(list(self.desktop.iterdir()))

    def test_save_verifies_original_quotes_and_no_overwrite(self):
        self.files();batch=self.batch();entries=self.entries(batch)
        existing=self.desktop/'现有文件.md';existing.write_text('保留')
        bad=json.loads(json.dumps(entries));bad[0]['points']=['不存在的数字999%']
        with self.assertRaises(DocumentError):self.tools.save(batch['batch_id'],bad)
        result=self.tools.save(batch['batch_id'],entries)
        self.assertTrue(result['verified']);self.assertEqual(result['count'],3)
        self.assertEqual(result['layout'],'one_file_per_document');self.assertEqual(len(result['reports']),3)
        self.assertNotIn('path',result);self.assertEqual(len({r['path'] for r in result['reports']}),3)
        for row,saved in zip(batch['documents'],result['reports']):
            self.assertEqual(row['document_id'],saved['document_id']);self.assertEqual(row['name'],saved['source_name'])
            path=Path(saved['path']);wire=path.read_bytes();report=wire.decode()
            self.assertEqual(hashlib.sha256(wire).hexdigest(),saved['sha256'])
            self.assertIn('文档摘要-'+Path(row['name']).stem+'-',path.name)
            self.assertIn(row['name'],report);self.assertEqual(report.count('来源：'),1)
            for other in batch['documents']:
                if other['document_id']!=row['document_id']:self.assertNotIn('来源：'+other['path'],report)
        self.assertEqual(existing.read_text(),'保留')
        with self.assertRaises(DocumentError):self.tools.save(batch['batch_id'],entries)

    def test_partial_multifile_failure_preserves_confirmed_file_and_blocks_retry(self):
        self.files();batch=self.batch();entries=self.entries(batch)
        original_open=os.open;writes=[]
        def disk_full(path,flags,*args,**kwargs):
            if flags&os.O_CREAT:
                writes.append(path)
                if len(writes)==2:raise OSError('synthetic disk full')
            return original_open(path,flags,*args,**kwargs)
        with patch('document_mcp.os.open',side_effect=disk_full):result=self.tools.save(batch['batch_id'],entries)
        self.assertFalse(result['verified']);self.assertEqual(result['count'],1);self.assertEqual(result['expected_count'],3)
        self.assertEqual(len(list(self.desktop.iterdir())),1)
        self.assertEqual(hashlib.sha256(Path(result['reports'][0]['path']).read_bytes()).hexdigest(),result['reports'][0]['sha256'])
        with self.assertRaises(DocumentError):self.tools.save(batch['batch_id'],entries)

    def test_one_source_creates_exactly_one_independent_file(self):
        self.files();batch=self.batch(1);result=self.tools.save(batch['batch_id'],self.entries(batch))
        self.assertTrue(result['verified']);self.assertEqual(result['count'],1)
        self.assertEqual(len(result['reports']),1);self.assertEqual(len(list(self.desktop.iterdir())),1)

    def test_same_source_basename_still_produces_distinct_files_and_lists_with_legacy(self):
        for i in range(3):
            directory=self.docs/str(i);directory.mkdir();(directory/'相同 名称.txt').write_text('本文件事实。')
        batch=self.batch();result=self.tools.save(batch['batch_id'],self.entries(batch))
        self.assertTrue(result['verified']);self.assertEqual(len({r['path'] for r in result['reports']}),3)
        legacy=self.report()
        rows=self.tools.list_reports()['reports']
        self.assertEqual({r['name'] for r in rows},{Path(r['path']).name for r in result['reports']}|{legacy.name})

    @unittest.skipUnless(os.environ.get('DOCUMENT_READER_CLI'),'Native reader integration')
    def test_docx_uses_shared_documentreader(self):
        path=self.docs/'office.docx';text='设备维护说明：每月断电清洁风扇滤网。'
        with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('[Content_Types].xml','<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="xml" ContentType="application/xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/></Types>')
            archive.writestr('_rels/.rels','<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="r1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>')
            archive.writestr('word/document.xml','<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:p><w:r><w:t>'+text+'</w:t></w:r></w:p></w:body></w:document>')
        batch=self.batch(1);value=self.tools.read(batch['batch_id'],batch['documents'][0]['document_id'])
        self.assertIn(text,value['text']);self.assertNotIn('PK\x03',value['text'])

    def test_protocol_notification_does_not_write(self):
        reader=os.environ.get('DOCUMENT_READER_CLI','/missing-reader')
        line=json.dumps({'jsonrpc':'2.0','method':'tools/call','params':{'name':'save_report','arguments':{}}})+'\n'
        process=subprocess.run([os.sys.executable,str(Path(__file__).with_name('document_mcp.py')),
            '--documents-root',str(self.docs),'--desktop-root',str(self.desktop),'--reader-cli',reader],input=line,capture_output=True,text=True)
        self.assertEqual(process.stdout,'');self.assertFalse(list(self.desktop.iterdir()))

    def report(self,index=1):
        from document_mcp import REPORT_HEADER
        path=self.desktop/f'文档摘要-20261026-120000-{index:08x}.md'
        path.write_bytes(REPORT_HEADER+b'\nTEST REPORT, not a user document.\n')
        return path

    def test_report_listing_excludes_other_files_wrong_headers_and_symlinks(self):
        path=self.report();other=self.desktop/'ordinary.md';other.write_text('preserve')
        fake=self.desktop/'文档摘要-20261026-120000-00000002.md';fake.write_text('wrong header')
        link=self.desktop/'文档摘要-20261026-120000-00000003.md';link.symlink_to(path)
        rows=self.tools.list_reports()['reports'];self.assertEqual([r['name'] for r in rows],[path.name])
        self.assertTrue(path.exists());self.assertTrue(other.exists())

    def test_report_changed_unknown_ids_and_duplicate_selection_never_call_gio(self):
        path=self.report();selection=self.tools.list_reports();ids=[selection['reports'][0]['report_id']]
        with patch('document_mcp.subprocess.run') as run:
            for invalid in (ids*2,['unknown'],['/etc/passwd']):
                with self.assertRaises(DocumentError):self.tools.trash_reports(selection['selection_id'],invalid)
            path.write_text('changed')
            with self.assertRaises(DocumentError):self.tools.trash_reports(selection['selection_id'],ids)
            run.assert_not_called()

    @unittest.skipUnless(shutil.which('gio'),'GIO native Trash')
    def test_real_gio_moves_only_selected_test_report_and_readbacks_trash(self):
        from document_mcp import DocumentTools
        data_home=self.root/'data';data_home.mkdir()
        self.tools=DocumentTools(self.docs,self.desktop,'/missing-reader',trash_data_home=data_home)
        one=self.report(1);renamed=self.desktop/'文档摘要-会议纪要-20261026-120000-00000001.md';one.rename(renamed);one=renamed
        two=self.report(2);before=one.read_bytes();other=self.desktop/'keep.md';other.write_text('preserve')
        selection=self.tools.list_reports();ids=[r['report_id'] for r in selection['reports'] if r['name']==one.name]
        result=self.tools.trash_reports(selection['selection_id'],ids)
        self.assertTrue(result['verified'],result);self.assertEqual(result['count'],1)
        self.assertFalse(one.exists());self.assertTrue(two.exists());self.assertEqual(other.read_text(),'preserve')
        self.assertEqual((data_home/'Trash/files'/result['moved'][0]['trash_name']).read_bytes(),before)
        with self.assertRaises(DocumentError):self.tools.trash_reports(selection['selection_id'],ids)

    def test_gio_failure_is_bounded_and_does_not_reexecute_or_remove(self):
        data_home=self.root/'data';data_home.mkdir()
        self.tools=DocumentTools(self.docs,self.desktop,'/missing-reader',trash_data_home=data_home)
        path=self.report();selection=self.tools.list_reports();ids=[selection['reports'][0]['report_id']]
        with patch('document_mcp.shutil.which',return_value='/usr/bin/gio'),patch('document_mcp.subprocess.run',return_value=subprocess.CompletedProcess([],1)) as run:
            result=self.tools.trash_reports(selection['selection_id'],ids)
            self.assertFalse(result['verified']);self.assertTrue(path.exists());self.assertEqual(run.call_count,1)
            with self.assertRaises(DocumentError):self.tools.trash_reports(selection['selection_id'],ids)


if __name__=='__main__':unittest.main()

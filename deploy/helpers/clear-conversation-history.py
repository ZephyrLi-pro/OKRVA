#!/usr/bin/env python3
"""Remove desktop conversation records at service process boundaries.

Minimize/hide does not stop a service. UI databases retain their schema and
monotonic ID counters; user documents, reports, settings and scheduled jobs are
not part of this cleanup. Failed Gateway deletions retain only opaque IDs for
retry on the next process boundary.
"""
import argparse
import configparser
import fcntl
import http.client
import json
import os
from pathlib import Path
import socket
import sqlite3
import subprocess
import time
from urllib.parse import quote

VOICE_PREFIX='voice-assistant-'


def is_voice(session):
    return session.get('name')=='voice-assistant' or session['session_id'].startswith(VOICE_PREFIX)


def local_voice_ids(database):
    if not database.exists():return set()
    with sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True) as connection:
        if not connection.execute("SELECT 1 FROM sqlite_master WHERE name='RECORD'").fetchone():return set()
        return {row[0] for row in connection.execute('SELECT DISTINCT session_uuid FROM RECORD')
                if isinstance(row[0],str) and row[0].startswith(VOICE_PREFIX)}


class Gateway:
    def __init__(self, home, runtime):
        config=configparser.ConfigParser(interpolation=None,strict=False)
        config.read(home/'.config/kylin-aiassistant/settings.ini',encoding='utf-8-sig')
        self.token=next((config[s]['gateway-token'].strip('"') for s in config.sections()
                         if 'gateway-token' in config[s]),'')
        self.path=runtime/'kylin-bot/gateway.sock'

    def request(self, method, path):
        connection=http.client.HTTPConnection('localhost',timeout=3)
        connection.sock=socket.socket(socket.AF_UNIX)
        connection.sock.settimeout(3)
        try:
            connection.sock.connect(str(self.path))
            headers={'Authorization':'Bearer '+self.token} if self.token else {}
            connection.request(method,path,headers=headers)
            response=connection.getresponse()
            raw=response.read(4*1024*1024+1)
            if response.status==404:return {}
            if response.status!=200 or len(raw)>4*1024*1024:
                raise RuntimeError('Gateway cleanup request failed')
            return json.loads(raw)
        finally:connection.close()


def write_pending(path, pending):
    temporary=path.with_suffix('.tmp')
    fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
    with os.fdopen(fd,'w') as output:json.dump(pending,output)
    temporary.replace(path)


def clear_local(database, scope, voice_ids):
    if not database.exists():return 0
    with sqlite3.connect(database,timeout=8) as connection:
        tables={row[0] for row in connection.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        if 'RECORD' not in tables:return 0
        rows=connection.execute('SELECT DISTINCT sessionID, session_uuid FROM RECORD').fetchall()
        selected=set()
        for sid,uid in rows:
            voice=(str(sid).startswith('v_') or uid=='voice-assistant' or uid in voice_ids
                   or isinstance(uid,str) and uid.startswith(VOICE_PREFIX))
            if scope=='all' or voice==(scope=='voice'):selected.add(sid)
        before=connection.total_changes
        if scope=='all':
            connection.execute('DELETE FROM RECORD')
            if 'DOCUMENT_REFERENCE' in tables:connection.execute('DELETE FROM DOCUMENT_REFERENCE')
        else:
            for sid in selected:
                connection.execute('DELETE FROM RECORD WHERE sessionID=?',(sid,))
                if 'DOCUMENT_REFERENCE' in tables:
                    connection.execute('DELETE FROM DOCUMENT_REFERENCE WHERE sessionID=?',(sid,))
        return connection.total_changes-before


def cleanup(home, runtime, scope, gateway=None):
    state=home/'.local/state/okrva/conversation-cleanup'
    state.mkdir(parents=True,exist_ok=True,mode=0o700)
    with (state/'lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        pending_file=state/'pending.json'
        pending=json.loads(pending_file.read_text()) if pending_file.exists() else {'text':[],'voice':[]}
        gateway=gateway or Gateway(home,runtime)
        online=True
        try:sessions=gateway.request('GET','/api/sessions')['sessions']
        except (OSError,ValueError,KeyError,RuntimeError,http.client.HTTPException):
            sessions=[];online=False
        database=home/'.config/kylin-aiassistant/kylin_aiassistant_database.db'
        voice_ids=(set(pending['voice'])|{s['session_id'] for s in sessions if is_voice(s)}
                   |local_voice_ids(database))
        scopes=('text','voice') if scope=='all' else (scope,)
        for kind in scopes:
            chosen=voice_ids if kind=='voice' else {s['session_id'] for s in sessions if not is_voice(s)}
            pending[kind]=sorted(set(pending[kind])|chosen)
        local_count=clear_local(database,scope,voice_ids)
        # Record retry IDs before contacting the Gateway; no message bodies or
        # credentials are copied into this state file.
        write_pending(pending_file,pending)
        removed=0
        cleanup_deadline=time.monotonic()+12
        for kind in scopes:
            for identifier in list(pending[kind]):
                if not online or time.monotonic()>=cleanup_deadline:break
                path='/api/sessions/'+quote(identifier,safe='')
                try:
                    abort=gateway.request('POST',path+'/abort')
                    deadline=time.monotonic()+4
                    while (abort.get('status')!='no_active_response'
                           and gateway.request('GET',path+'/state').get('state')=='running'):
                        if time.monotonic()>=deadline:raise RuntimeError('Response still shutting down')
                        time.sleep(0.1)
                    gateway.request('DELETE',path)
                except (OSError,ValueError,RuntimeError,http.client.HTTPException):continue
                pending[kind].remove(identifier);removed+=1
        write_pending(pending_file,pending)
        return {'scope':scope,'local_rows_removed':local_count,'gateway_sessions_removed':removed,
                'pending_sessions':sum(len(pending[k]) for k in scopes),'gateway_available':online}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scope',choices=('text','voice','all'))
    args=parser.parse_args()
    result=cleanup(Path.home(),Path(os.environ.get('XDG_RUNTIME_DIR','/run/user/'+str(os.getuid()))),args.scope)
    print(json.dumps(result),flush=True)
    if args.scope=='voice':
        # Refresh an already running text window after voice's normal exit
        # flushed and removed its temporary history; never activate a new UI.
        try:
            subprocess.run(['busctl','--user','--auto-start=no','call','org.ukui.kylin_aiassistant',
                '/org/ukui/kylin_aiassistant','org.ukui.kylin_aiassistant','NotifyHistoryChanged'],
                stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=3,check=False)
        except (OSError,subprocess.TimeoutExpired):pass


if __name__=='__main__':main()

import { useMemo, useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { FileText } from 'lucide-react';
import { listChatSessions } from '@/lib/chatHistoryStorage';
import { setSessionId } from '@/lib/ws';
import { t, useLocale } from '@/lib/i18n';

function formatUpdatedAt(iso: string): string {
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return '';
  const year = date.getFullYear();
  const month = String(date.getMonth() + 1).padStart(2, '0');
  const day = String(date.getDate()).padStart(2, '0');
  const hours = String(date.getHours()).padStart(2, '0');
  const minutes = String(date.getMinutes()).padStart(2, '0');
  return `${year}/${month}/${day} ${hours}:${minutes}`;
}

type GroupKey = 'today' | 'last7days' | 'last30days' | 'lastYear' | 'older';

interface GroupedSessions {
  key: GroupKey;
  label: string;
  sessions: { sessionId: string; title: string; preview: string; updatedAt: string; messageCount: number }[];
}

function groupSessions(sessions: { updatedAt: string; sessionId: string; title: string; preview: string; messageCount: number }[]): GroupedSessions[] {
  const now = new Date();
  const today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  const last7Days = new Date(today.getTime() - 7 * 24 * 60 * 60 * 1000);
  const last30Days = new Date(today.getTime() - 30 * 24 * 60 * 60 * 1000);
  const lastYear = new Date(today.getTime() - 365 * 24 * 60 * 60 * 1000);

  const groups: Record<GroupKey, { label: string; sessions: typeof sessions }> = {
    today: { label: t('history.today'), sessions: [] },
    last7days: { label: t('history.last_7_days'), sessions: [] },
    last30days: { label: t('history.last_30_days'), sessions: [] },
    lastYear: { label: t('history.last_year'), sessions: [] },
    older: { label: t('history.earlier'), sessions: [] },
  };

  sessions.forEach((session) => {
    const sessionDate = new Date(session.updatedAt);
    if (sessionDate >= today) {
      groups.today.sessions.push(session);
    } else if (sessionDate >= last7Days) {
      groups.last7days.sessions.push(session);
    } else if (sessionDate >= last30Days) {
      groups.last30days.sessions.push(session);
    } else if (sessionDate >= lastYear) {
      groups.lastYear.sessions.push(session);
    } else {
      groups.older.sessions.push(session);
    }
  });

  return [
    { key: 'today' as GroupKey, ...groups.today },
    { key: 'last7days' as GroupKey, ...groups.last7days },
    { key: 'last30days' as GroupKey, ...groups.last30days },
    { key: 'lastYear' as GroupKey, ...groups.lastYear },
    { key: 'older' as GroupKey, ...groups.older },
  ].filter((g) => g.sessions.length > 0);
}

export default function AgentHistory() {
  const { locale } = useLocale();
  const navigate = useNavigate();
  const [sessions, setSessions] = useState<{ sessionId: string; title: string; preview: string; updatedAt: string; messageCount: number }[]>([]);
  const [loading, setLoading] = useState(true);
  const [selectedSessions, setSelectedSessions] = useState<Set<string>>(new Set());
  const [selectAll, setSelectAll] = useState(false);

  useEffect(() => {
    listChatSessions().then((data) => {
      setSessions(data);
      setLoading(false);
    });
  }, []);

  const groupedSessions = useMemo(() => {
    const sorted = [...sessions].sort(
      (a, b) => new Date(b.updatedAt).getTime() - new Date(a.updatedAt).getTime(),
    );
    return groupSessions(sorted);
  }, [sessions, locale]);

  const handleSelectAll = () => {
    if (selectAll) {
      setSelectedSessions(new Set());
    } else {
      setSelectedSessions(new Set(sessions.map((s) => s.sessionId)));
    }
    setSelectAll(!selectAll);
  };

  const handleSelectSession = (sessionId: string) => {
    const newSelected = new Set(selectedSessions);
    if (newSelected.has(sessionId)) {
      newSelected.delete(sessionId);
    } else {
      newSelected.add(sessionId);
    }
    setSelectedSessions(newSelected);
  };

  const openSession = (sessionId: string) => {
    setSessionId(sessionId);
    navigate('/agent');
  };

  return (
    <div className="h-[calc(100vh-3.5rem)] overflow-y-auto px-6 py-5 md:px-10">
      <div className="mx-auto w-full max-w-4xl">
        <div className="mb-5">
          <h2 className="text-lg font-semibold" style={{ color: 'var(--pc-text-primary)' }}>
            {t('history.title')}
          </h2>
        </div>

        {sessions.length > 0 && (
          <div className="mb-4 flex items-center gap-2">
            <input
              type="checkbox"
              checked={selectAll}
              onChange={handleSelectAll}
              className="h-4 w-4 accent-blue-500"
            />
            <span className="text-sm font-medium" style={{ color: 'var(--pc-text-primary)' }}>
              {t('history.task_title')}
            </span>
          </div>
        )}

        {loading && (
          <div className="text-sm" style={{ color: 'var(--pc-text-muted)' }}>
            {t('history.loading')}
          </div>
        )}
        {!loading && sessions.length === 0 && (
          <div className="text-sm" style={{ color: 'var(--pc-text-muted)' }}>
            {t('history.empty')}
          </div>
        )}
        {sessions.length > 0 && (
          <div className="space-y-4">
            {groupedSessions.map((group) => (
              <div key={group.key}>
                <div
                  className="mb-2 px-1 py-1 text-xs font-medium"
                  style={{ color: 'var(--pc-text-muted)' }}
                >
                  {group.label}
                </div>
                <div className="space-y-1">
                  {group.sessions.map((session) => (
                    <div
                      key={session.sessionId}
                      className="flex cursor-pointer items-center gap-3 px-2 py-2"
                      onClick={() => openSession(session.sessionId)}
                    >
                      <input
                        type="checkbox"
                        checked={selectedSessions.has(session.sessionId)}
                        onChange={(e) => {
                          e.stopPropagation();
                          handleSelectSession(session.sessionId);
                        }}
                        className="h-4 w-4 shrink-0 accent-blue-500"
                      />
                      <FileText className="h-4 w-4 shrink-0" style={{ color: 'var(--pc-text-muted)' }} />
                      <div className="min-w-0 flex-1">
                        <p className="truncate text-sm" style={{ color: 'var(--pc-text-primary)' }}>
                          {session.title}
                        </p>
                      </div>
                      <span className="shrink-0 text-xs" style={{ color: 'var(--pc-text-faint)' }}>
                        {formatUpdatedAt(session.updatedAt)}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

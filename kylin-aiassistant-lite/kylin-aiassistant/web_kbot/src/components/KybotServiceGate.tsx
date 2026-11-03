import type { ReactNode } from 'react';
import { useLocation } from 'react-router-dom';
import KybotServiceUnavailable from '@/components/KybotServiceUnavailable';
import { KYBOT_SERVICE_UNAVAILABLE_SHELL } from '@/components/kybotServiceUnavailableStyles';
import { useKybotService } from '@/hooks/useKybotService';
import { PageLoadingSpinner } from '@/components/ui/PageLoadingSpinner';

interface KybotServiceGateProps {
  children: ReactNode;
}

export default function KybotServiceGate({ children }: KybotServiceGateProps) {
  const { pathname } = useLocation();
  const { available, starting, start, contentEpoch } = useKybotService(pathname);

  if (available === null) {
    return (
      <div style={KYBOT_SERVICE_UNAVAILABLE_SHELL}>
        <PageLoadingSpinner />
      </div>
    );
  }

  if (!available) {
    return <KybotServiceUnavailable starting={starting} onStart={() => void start()} />;
  }

  // contentEpoch 变化时 remount 子页面，等效于刷新但不会在 qrc WebEngine 里触发整页 reload 错误。
  return (
    <div key={contentEpoch} style={{ width: '100%', height: '100%' }}>
      {children}
    </div>
  );
}

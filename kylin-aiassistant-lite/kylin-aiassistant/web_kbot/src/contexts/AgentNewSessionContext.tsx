import {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useRef,
  type ReactNode,
} from 'react';

type NewSessionHandler = () => void;

const AgentNewSessionContext = createContext<{
  registerNewSessionHandler: (handler: NewSessionHandler | null) => void;
  requestNewSession: () => void;
} | null>(null);

export function AgentNewSessionProvider({ children }: { children: ReactNode }) {
  const handlerRef = useRef<NewSessionHandler | null>(null);

  const registerNewSessionHandler = useCallback((handler: NewSessionHandler | null) => {
    handlerRef.current = handler;
  }, []);

  const requestNewSession = useCallback(() => {
    handlerRef.current?.();
  }, []);

  const value = useMemo(
    () => ({ registerNewSessionHandler, requestNewSession }),
    [registerNewSessionHandler, requestNewSession],
  );

  return (
    <AgentNewSessionContext.Provider value={value}>
      {children}
    </AgentNewSessionContext.Provider>
  );
}

export function useAgentNewSession(): {
  registerNewSessionHandler: (handler: NewSessionHandler | null) => void;
  requestNewSession: () => void;
} {
  const ctx = useContext(AgentNewSessionContext);
  if (!ctx) {
    throw new Error('useAgentNewSession must be used within AgentNewSessionProvider');
  }
  return ctx;
}

import { useState, useEffect } from 'react';
import { X } from 'lucide-react';

interface IdentitySetupModalProps {
  open: boolean;
  onClose: () => void;
  onConfirm: (name: string, role: string) => void;
}

export function IdentitySetupModal({ open, onClose, onConfirm }: IdentitySetupModalProps) {
  const [name, setName] = useState('');
  const [role, setRole] = useState('');

  useEffect(() => {
    if (!open) {
      setName('');
      setRole('');
    }
  }, [open]);

  useEffect(() => {
    if (!open) return;
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [open, onClose]);

  if (!open) return null;

  const handleSubmit = () => {
    if (name.trim() || role.trim()) {
      onConfirm(name.trim(), role.trim());
    }
  };

  return (
    <div
      role="dialog"
      aria-modal="true"
      aria-label="设置身份"
      className="fixed inset-0 z-50 flex items-center justify-center"
      onClick={onClose}
    >
      <div className="absolute inset-0" style={{ background: 'rgba(0,0,0,0.6)', backdropFilter: 'blur(8px)' }} />
      <div
        className="relative w-full max-w-sm mx-4 rounded-3xl border shadow-2xl animate-fade-in"
        style={{ background: 'var(--pc-bg-base)', borderColor: 'var(--pc-border)' }}
        onClick={(e) => e.stopPropagation()}
      >
        <div
          className="flex items-center justify-between px-5 py-4 border-b"
          style={{ borderColor: 'var(--pc-border)' }}
        >
          <h2 className="text-sm font-semibold" style={{ color: 'var(--pc-text-primary)' }}>设置你的AI助手</h2>
          <button
            onClick={onClose}
            className="h-7 w-7 rounded-lg flex items-center justify-center transition-colors"
            style={{ color: 'var(--pc-text-muted)', background: 'transparent', border: 'none', cursor: 'default' }}
            onMouseEnter={(e) => { e.currentTarget.style.color = 'var(--pc-text-primary)'; e.currentTarget.style.background = 'var(--pc-hover)'; }}
            onMouseLeave={(e) => { e.currentTarget.style.color = 'var(--pc-text-muted)'; e.currentTarget.style.background = 'transparent'; }}
            aria-label="关闭"
          >
            <X size={16} />
          </button>
        </div>

        <div className="px-5 py-4 space-y-4">
          <div>
            <label
              className="text-xs mb-1.5 block"
              style={{ color: 'var(--pc-text-secondary)' }}
            >
              给你的AI助手起个名字
            </label>
            <input
              type="text"
              value={name}
              onChange={(e) => setName(e.target.value)}
              placeholder="例如：小K、小智、助手"
              className="w-full px-3 py-2 rounded-xl border text-sm transition-all"
              style={{
                background: 'var(--pc-bg-input)',
                borderColor: 'var(--pc-border)',
                color: 'var(--pc-text-primary)',
              }}
              autoFocus
            />
          </div>

          <div>
            <label
              className="text-xs mb-1.5 block"
              style={{ color: 'var(--pc-text-secondary)' }}
            >
              描述它的角色（可选）
            </label>
            <textarea
              value={role}
              onChange={(e) => setRole(e.target.value)}
              placeholder="例���：专业的编程助手，擅长代码优化和Debug"
              rows={2}
              className="w-full px-3 py-2 rounded-xl border text-sm transition-all resize-none"
              style={{
                background: 'var(--pc-bg-input)',
                borderColor: 'var(--pc-border)',
                color: 'var(--pc-text-primary)',
              }}
            />
          </div>

          <button
            onClick={handleSubmit}
            className="w-full py-2.5 rounded-xl font-medium text-sm transition-all"
            style={{
              background: 'var(--pc-accent)',
              color: 'white',
              border: 'none',
              cursor: 'default',
            }}
            onMouseEnter={(e) => { e.currentTarget.style.opacity = '0.9'; }}
            onMouseLeave={(e) => { e.currentTarget.style.opacity = '1'; }}
          >
            确定
          </button>
        </div>
      </div>
    </div>
  );
}
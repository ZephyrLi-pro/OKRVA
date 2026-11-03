import { useState, useRef, useEffect, useCallback } from 'react';
import { Loader2, X } from 'lucide-react';
import { t, tf } from '@/lib/i18n';
import { useSkills } from '@/hooks/useApi';
import { toggleSkill as toggleSkillApi, createSkill, deleteSkill, importSkill, openSkillFolder } from '@/lib/api';
import { uninstallSkill } from '@/lib/skillhubApi';
import { assetBasePath } from '@/lib/basePath';
import { PageLoadingState } from '@/components/ui/PageLoadingSpinner';
import { ModelCloseButton } from '@/components/model/modelTooltips';
import type { Skill } from '@/types/api';

function ImportMenu({
  onImportResult,
  onAddClick,
}: {
  onImportResult: (success: boolean) => void;
  onAddClick: () => void;
}) {
  const [menuOpen, setMenuOpen] = useState(false);
  const [dropUp, setDropUp] = useState(false);
  const [overwriteName, setOverwriteName] = useState<string | null>(null);
  const [overwriteFile, setOverwriteFile] = useState<File | null>(null);
  const [overwriting, setOverwriting] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!menuOpen) return;
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) setMenuOpen(false);
    };
    const closeOnBlur = () => setMenuOpen(false);
    document.addEventListener('mousedown', handler);
    window.addEventListener('blur', closeOnBlur);
    return () => {
      document.removeEventListener('mousedown', handler);
      window.removeEventListener('blur', closeOnBlur);
    };
  }, [menuOpen]);

  const handleToggleMenu = () => {
    setMenuOpen((v) => {
      if (!v && ref.current) {
        const rect = ref.current.getBoundingClientRect();
        setDropUp(rect.bottom + 170 > window.innerHeight);
      }
      return !v;
    });
  };

  const handleUploadFile = () => {
    fileInputRef.current?.click();
  };

  const handleFileChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    e.target.value = '';
    setMenuOpen(false);
    await doImport(file);
  };

  const doImport = async (file: File) => {
    try {
      console.log('[Skills] doImport starting for:', file.name, file.size, file.type);
      await importSkill(file);
      console.log('[Skills] importSkill succeeded');
      onImportResult(true);
    } catch (err: unknown) {
      const e = err as Error & { duplicate?: boolean; skillName?: string };
      console.error('[Skills] importSkill failed:', String(err), 'type:', typeof err, 'stack:', (err as Error)?.stack);
      console.error('[Skills] raw error:', err);
      if (e.duplicate && e.skillName) {
        setOverwriteName(e.skillName);
        setOverwriteFile(file);
        return;
      }
      onImportResult(false);
    }
  };

  const handleOverwrite = async () => {
    if (!overwriteFile || !overwriteName) return;
    setOverwriting(true);
    try {
      await deleteSkill(overwriteName);
      await doImport(overwriteFile);
      setOverwriteName(null);
      setOverwriteFile(null);
    } catch {
      onImportResult(false);
    } finally {
      setOverwriting(false);
    }
  };

  return (
    <div ref={ref} style={{ position: 'relative', display: 'inline-block' }}>
      <button
        className="skill-toolbar-btn"
        onClick={handleToggleMenu}
      >
        <span style={{ fontSize: 18, lineHeight: 1 }}>+</span>
        <span>{t('skills.add')}</span>
      </button>
      {menuOpen && (
        <div
          onMouseDown={(e) => e.stopPropagation()}
          style={{
            position: 'absolute',
            right: 0,
            ...(dropUp ? { bottom: '100%' } : { top: '100%' }),
            zIndex: 100,
            background: '#fff',
            border: '1px solid #e8e8e8',
            borderRadius: 8,
            boxShadow: '0 4px 16px rgba(0,0,0,0.12)',
            width: 180,
            padding: 8,
            boxSizing: 'border-box',
            display: 'flex',
            flexDirection: 'column',
          }}
        >
          <button
            className="skill-uninstall-btn"
            style={{ width: 164, height: 36 }}
            onClick={() => { setMenuOpen(false); onAddClick(); }}
          >
            {t('skills.add_custom')}
          </button>
          <button
            className="skill-uninstall-btn"
            style={{ width: 164, height: 36, marginTop: 2 }}
            onClick={handleUploadFile}
          >
            {t('skills.upload_file')}
          </button>
        </div>
      )}
      <input ref={fileInputRef} type="file" accept=".md,.zip" style={{ display: 'none' }} onChange={handleFileChange} />
      <OverwriteConfirmDialog
        open={!!overwriteName}
        onCancel={() => { setOverwriteName(null); setOverwriteFile(null); }}
        onConfirm={handleOverwrite}
        overwriting={overwriting}
        skillName={overwriteName || ''}
      />
    </div>
  );
}

function AddSkillDialog({
  open,
  onCancel,
  onCreated,
  existingSlugs,
}: {
  open: boolean;
  onCancel: () => void;
  onCreated: () => void;
  existingSlugs: string[];
}) {
  const [name, setName] = useState('');
  const [description, setDescription] = useState('');
  const [capability, setCapability] = useState('');
  const [creating, setCreating] = useState(false);
  const [nameOverflow, setNameOverflow] = useState(false);
  const [descOverflow, setDescOverflow] = useState(false);
  const [nameInvalidChar, setNameInvalidChar] = useState(false);
  const [overwriteName, setOverwriteName] = useState<string | null>(null);
  const [overwriting, setOverwriting] = useState(false);
  const [nameFocused, setNameFocused] = useState(false);
  const [descFocused, setDescFocused] = useState(false);
  const [capFocused, setCapFocused] = useState(false);
  const nameRef = useRef<HTMLInputElement>(null);

  const byteLen = (s: string) => new TextEncoder().encode(s).length;
  const INVALID_CHARS = ['\\', '/', ':', '*', '?', '"', '<', '>', '|'];
  const hasInvalidChar = (s: string) => INVALID_CHARS.some((c) => s.includes(c));
  const findMaxLen = (s: string, maxBytes: number): number => {
    let total = 0;
    for (let i = 0; i < s.length; i++) {
      total += new TextEncoder().encode(s[i]).length;
      if (total > maxBytes) return i;
    }
    return s.length;
  };

  useEffect(() => {
    if (open) {
      setName('');
      setDescription('');
      setCapability('');
      setCreating(false);
      setNameOverflow(false);
      setDescOverflow(false);
      setNameInvalidChar(false);
      setOverwriteName(null);
      setOverwriting(false);
      requestAnimationFrame(() => nameRef.current?.focus());
    }
  }, [open]);

  const handleCreate = async () => {
    const trimmed = name.trim();
    if (!trimmed) return;
    if (existingSlugs.some((s) => s.toLowerCase() === trimmed.toLowerCase())) {
      setOverwriteName(trimmed);
      return;
    }
    await doCreate(trimmed);
  };

  const doCreate = async (skillName: string) => {
    setCreating(true);
    try {
      const body: { description?: string; prompts?: string[] } = {};
      if (description.trim()) body.description = description.trim();
      if (capability.trim()) {
        body.prompts = [capability.trim()];
      }
      await createSkill(skillName, body);
      onCreated();
    } catch (err) {
      console.error('Failed to create skill:', err);
    } finally {
      setCreating(false);
    }
  };

  const handleOverwrite = async () => {
    if (!overwriteName) return;
    setOverwriting(true);
    try {
      await deleteSkill(overwriteName);
      await doCreate(overwriteName);
      setOverwriteName(null);
    } catch (err) {
      console.error('Failed to overwrite skill:', err);
    } finally {
      setOverwriting(false);
    }
  };

  const getInputStyle = (focused: boolean): React.CSSProperties => ({
    width: '100%',
    height: 36,
    padding: '0 10px',
    fontSize: 14,
    color: 'var(--pc-text-primary)',
    border: focused ? '2px solid #3790FA' : '2px solid transparent',
    borderRadius: 4,
    background: focused ? '#FFFFFF' : '#E6E6E6',
    outline: 'none',
    boxSizing: 'border-box',
  });

  const labelStyle: React.CSSProperties = {
    display: 'block',
    fontSize: 14,
    color: 'rgba(0, 0, 0, 0.85)',
    marginBottom: 4,
    fontWeight: 400,
  };

  const errorStyle: React.CSSProperties = {
    fontSize: 12,
    color: '#F53F3F',
    minHeight: 18,
    lineHeight: '18px',
  };

  const canSubmit = name.trim() && description.trim();

  if (!open && !overwriteName) return null;

  return (
    <>
      <div
        style={{
          position: 'fixed',
          top: 0,
          left: 0,
          width: '100vw',
          height: '100vh',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 200,
          transform: 'translate(-28px, -40px)',
        }}
      >
        <div
          style={{
            width: 424,
            height: 520,
            background: '#fff',
            borderRadius: 8,
            boxSizing: 'border-box',
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            boxShadow: '0 6px 30px rgba(0,0,0,0.12)',
          }}
        >
          <div
            style={{
              height: 40,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              paddingLeft: 12,
              paddingRight: 8,
              flexShrink: 0,
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center' }}>
              <img
                src={`${assetBasePath}/icons/logo.png`}
                alt=""
                width={24}
                height={24}
                style={{ width: 24, height: 24, objectFit: 'contain', display: 'block', flexShrink: 0 }}
                onError={(e) => { (e.target as HTMLImageElement).style.display = 'none'; }}
              />
              <span style={{ fontSize: 14, fontWeight: 400, color: 'rgba(0, 0, 0, 0.85)', marginLeft: 8 }}>
                {t('skills.add_title')}
              </span>
            </div>
            <ModelCloseButton onClick={onCancel} />
          </div>

          <div style={{ paddingLeft: 24, paddingRight: 24, paddingTop: 8, paddingBottom: 24, display: 'flex', flexDirection: 'column', flex: 1, overflowY: 'auto' }}>
            <div>
              <label style={labelStyle}>{t('skills.field.name')}</label>
              <input
                ref={nameRef}
                value={name}
                onChange={(e) => {
                  const v = e.target.value;
                  if (hasInvalidChar(v)) {
                    setNameInvalidChar(true);
                    return;
                  }
                  setNameInvalidChar(false);
                  if (byteLen(v) > 255) {
                    setNameOverflow(true);
                    setName(v.slice(0, findMaxLen(v, 255)));
                  } else {
                    setNameOverflow(false);
                    setName(v);
                  }
                }}
                onFocus={() => setNameFocused(true)}
                onBlur={() => setNameFocused(false)}
                placeholder={t('skills.placeholder.required')}
                className="placeholder-[#A6A6A6]"
                style={getInputStyle(nameFocused)}
              />
              <div style={errorStyle}>
                {nameInvalidChar ? t('skills.error.invalid_char') : nameOverflow ? t('skills.error.max_len') : '\u00A0'}
              </div>
            </div>

            <div>
              <label style={labelStyle}>{t('skills.field.desc')}</label>
              <input
                value={description}
                onChange={(e) => {
                  const v = e.target.value;
                  if (byteLen(v) > 255) {
                    setDescOverflow(true);
                    setDescription(v.slice(0, findMaxLen(v, 255)));
                  } else {
                    setDescOverflow(false);
                    setDescription(v);
                  }
                }}
                onFocus={() => setDescFocused(true)}
                onBlur={() => setDescFocused(false)}
                placeholder={t('skills.placeholder.required')}
                className="placeholder-[#A6A6A6]"
                style={getInputStyle(descFocused)}
              />
              <div style={errorStyle}>
                {descOverflow ? t('skills.error.max_len') : '\u00A0'}
              </div>
            </div>

            <div>
              <label style={labelStyle}>{t('skills.field.capability')}</label>
              <textarea
                value={capability}
                onChange={(e) => setCapability(e.target.value)}
                onFocus={() => setCapFocused(true)}
                onBlur={() => setCapFocused(false)}
                placeholder={t('skills.placeholder.capability')}
                className="placeholder-[#A6A6A6]"
                rows={9}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  fontSize: 14,
                  color: 'var(--pc-text-primary)',
                  border: capFocused ? '2px solid #3790FA' : '2px solid transparent',
                  borderRadius: 4,
                  background: capFocused ? '#FFFFFF' : '#E6E6E6',
                  outline: 'none',
                  boxSizing: 'border-box',
                  resize: 'none',
                  lineHeight: '20px',
                }}
              />
            </div>

            <div style={{ display: 'flex', justifyContent: 'flex-end', marginTop: 'auto' }}>
              <button className="skill-dialog-cancel-btn" onClick={onCancel} style={{ width: 64, height: 36 }}>
                {t('common.cancel')}
              </button>
              <button
                className="skill-dialog-uninstall-btn"
                onClick={handleCreate}
                disabled={creating || !canSubmit}
                style={{ opacity: !canSubmit ? 0.5 : undefined, width: 64, height: 36, cursor: !canSubmit ? 'default' : 'pointer', marginLeft: 8 }}
              >
                {creating ? <Loader2 className="h-4 w-4 animate-spin" style={{ color: '#fff' }} /> : t('skills.add')}
              </button>
            </div>
          </div>
        </div>
      </div>
      <OverwriteConfirmDialog
        open={!!overwriteName}
        onCancel={() => setOverwriteName(null)}
        onConfirm={handleOverwrite}
        overwriting={overwriting}
        skillName={overwriteName || ''}
      />
    </>
  );
}

function SkillSwitch({
  enabled,
  loading,
  onToggle,
}: {
  enabled: boolean;
  loading: boolean;
  onToggle: () => void;
}) {
  return (
    <button
      onClick={onToggle}
      disabled={loading}
      className="relative rounded-full transition-colors disabled:opacity-50 flex-shrink-0"
      style={{
        width: 44,
        height: 22,
        background: enabled ? '#3790FA' : 'var(--pc-border)',
      }}
      aria-pressed={enabled}
      aria-label={enabled ? t('skills.enabled') : t('skills.disabled')}
    >
      <span
        className="absolute top-1 left-1 w-[14px] h-[14px] rounded-full bg-white transition-transform"
        style={{
          transform: enabled ? 'translateX(22px)' : 'translateX(0)',
        }}
      />
    </button>
  );
}

function OverwriteConfirmDialog({
  open,
  onCancel,
  onConfirm,
  overwriting,
  skillName,
}: {
  open: boolean;
  onCancel: () => void;
  onConfirm: () => void;
  overwriting: boolean;
  skillName: string;
}) {
  if (!open) return null;

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        width: '100vw',
        height: '100vh',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 200,
          transform: 'translate(-28px, -40px)',
        }}
      >
        <div
          style={{
            width: 376,
            height: 212,
            background: '#fff',
            borderRadius: 8,
            boxSizing: 'border-box',
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            boxShadow: '0 6px 30px rgba(0,0,0,0.12)',
          }}
        >
          <div
            style={{
              width: 376,
              height: 40,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'flex-end',
              paddingRight: 8,
              boxSizing: 'border-box',
              flexShrink: 0,
            }}
          >
            <ModelCloseButton onClick={onCancel} />
          </div>

          <div
            style={{
              paddingLeft: 24,
              paddingRight: 24,
              paddingBottom: 24,
              flex: 1,
              display: 'flex',
              flexDirection: 'column',
            }}
          >
            <div style={{ display: 'flex', alignItems: 'flex-start', marginBottom: 4 }}>
              <img
                src={`${assetBasePath}/icons/dialog-question.svg`}
                alt="question"
                width={16}
                height={16}
                style={{ flexShrink: 0, marginTop: 2 }}
              />
              <span style={{ fontSize: 14, color: 'var(--pc-text-primary)', lineHeight: '20px', marginLeft: 8 }}>
                {t('skills.overwrite_title')}
            </span>
          </div>

          <span
            style={{
              fontSize: 14,
              color: 'var(--pc-text-muted)',
              lineHeight: '20px',
              marginLeft: 24,
            }}
          >
             {tf('skills.overwrite_body', { name: skillName })}
          </span>

          <div style={{ flex: 1 }} />

          <div style={{ display: 'flex', justifyContent: 'flex-end' }}>
            <button
              className="skill-dialog-cancel-btn"
              onClick={onCancel}
            >
              {t('common.cancel')}
            </button>
            <button
              className="skill-dialog-uninstall-btn"
              onClick={onConfirm}
              disabled={overwriting}
              style={{ marginLeft: 8 }}
            >
              {overwriting ? (
                <Loader2 className="h-4 w-4 animate-spin" style={{ color: '#fff' }} />
              ) : (
                t('skills.overwrite')
              )}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

function UninstallConfirmDialog({
  open,
  onCancel,
  onConfirm,
  uninstalling,
}: {
  open: boolean;
  onCancel: () => void;
  onConfirm: () => void;
  uninstalling: boolean;
}) {
  const overlayRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (open) {
      overlayRef.current?.focus();
    }
  }, [open]);

  if (!open) return null;

  return (
    <div
      ref={overlayRef}
      tabIndex={-1}
      onKeyDown={(e) => {
        if (e.key === 'Enter' && !uninstalling) {
          e.preventDefault();
          onConfirm();
        }
      }}
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        width: '100vw',
        height: '100vh',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 200,
        transform: 'translate(-28px, -40px)',
      }}
    >
      <div
        style={{
          width: 376,
          height: 212,
          background: '#fff',
          borderRadius: 8,
          boxSizing: 'border-box',
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          boxShadow: '0 6px 30px rgba(0,0,0,0.12)',
        }}
      >
        <div
          style={{
            width: 376,
            height: 40,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'flex-end',
            paddingRight: 8,
            boxSizing: 'border-box',
            flexShrink: 0,
          }}
        >
          <ModelCloseButton onClick={onCancel} />
        </div>

        <div
          style={{
            paddingLeft: 24,
            paddingRight: 24,
            paddingBottom: 24,
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'flex-start', marginBottom: 4 }}>
            <img
              src={`${assetBasePath}/icons/dialog-question.svg`}
              alt="question"
              width={16}
              height={16}
              style={{ flexShrink: 0, marginTop: 2 }}
            />
            <span style={{ fontSize: 14, color: 'var(--pc-text-primary)', lineHeight: '20px', marginLeft: 8 }}>
              {t('skills.uninstall_title')}
            </span>
          </div>

          <span
            style={{
              fontSize: 14,
              color: 'var(--pc-text-muted)',
              lineHeight: '20px',
              marginLeft: 24,
            }}
          >
            {t('skills.uninstall_body')}
          </span>

          <div style={{ flex: 1 }} />

          <div style={{ display: 'flex', justifyContent: 'flex-end' }}>
            <button
              className="skill-dialog-cancel-btn"
              onClick={onCancel}
            >
              {t('common.cancel')}
            </button>
            <button
              className="skill-dialog-uninstall-btn"
              onClick={onConfirm}
              disabled={uninstalling}
              style={{ marginLeft: 8 }}
            >
              {uninstalling ? (
                <Loader2 className="h-4 w-4 animate-spin" style={{ color: '#fff' }} />
              ) : (
                t('skills.uninstall')
              )}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}



function SkillOperateMenu({
  skillSlug,
  onUninstalled,
}: {
  skillSlug: string;
  onUninstalled: () => void;
}) {
  const [menuOpen, setMenuOpen] = useState(false);
  const [dialogOpen, setDialogOpen] = useState(false);
  const [uninstalling, setUninstalling] = useState(false);
  const [dropUp, setDropUp] = useState(false);
  const [tooltipVisible, setTooltipVisible] = useState(false);
  const [tooltipPos, setTooltipPos] = useState({ x: 0, y: 0 });
  const tooltipTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!menuOpen) return;
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) setMenuOpen(false);
    };
    const closeOnBlur = () => setMenuOpen(false);
    document.addEventListener('mousedown', handler);
    window.addEventListener('blur', closeOnBlur);
    return () => {
      document.removeEventListener('mousedown', handler);
      window.removeEventListener('blur', closeOnBlur);
    };
  }, [menuOpen]);

  const handleToggleMenu = () => {
    setMenuOpen((v) => {
      if (!v && ref.current) {
        const rect = ref.current.getBoundingClientRect();
        setDropUp(rect.bottom + 80 > window.innerHeight);
      }
      return !v;
    });
  };

  const handleUninstall = async () => {
    setUninstalling(true);
    try {
      await Promise.all([
        uninstallSkill(skillSlug).catch(() => {}),
        deleteSkill(skillSlug).catch(() => {}),
      ]);
      setDialogOpen(false);
      setMenuOpen(false);
      onUninstalled();
    } catch (err) {
      console.error('Failed to uninstall skill:', err);
    } finally {
      setUninstalling(false);
    }
  };

  return (
    <>
      <UninstallConfirmDialog
        open={dialogOpen}
        onCancel={() => setDialogOpen(false)}
        onConfirm={handleUninstall}
        uninstalling={uninstalling}
      />
      <div ref={ref} style={{ position: 'relative', display: 'inline-block' }}>
        <button
          className="skill-operate-btn"
          style={{ cursor: 'default' }}
          onClick={handleToggleMenu}
          onMouseEnter={(e) => {
            setTooltipPos({ x: e.clientX, y: e.clientY });
            tooltipTimerRef.current = setTimeout(() => setTooltipVisible(true), 500);
          }}
          onMouseLeave={() => {
            if (tooltipTimerRef.current) clearTimeout(tooltipTimerRef.current);
            tooltipTimerRef.current = null;
            setTooltipVisible(false);
          }}
        >
          <img
            src={`${assetBasePath}/icons/operate-omit.svg`}
            alt={t('skills.more')}
            width={16}
            height={16}
          />
        </button>
        {tooltipVisible && !menuOpen && (
          <div
            style={{
              position: 'fixed',
              left: tooltipPos.x + 8,
              top: tooltipPos.y + 8,
              zIndex: 100,
              background: '#fff',
              border: '1px solid #e8e8e8',
              borderRadius: 8,
              boxShadow: '0 4px 16px rgba(0,0,0,0.12)',
              padding: '8px 12px',
              boxSizing: 'border-box',
              fontSize: 14,
              color: 'var(--pc-text-primary)',
              whiteSpace: 'nowrap',
              pointerEvents: 'none',
            }}
          >
            {t('skills.more')}
          </div>
        )}
        {menuOpen && (
          <div
            style={{
              position: 'absolute',
              right: 0,
              ...(dropUp ? { bottom: '100%' } : { top: '100%' }),
              zIndex: 100,
              background: '#fff',
              border: '1px solid #e8e8e8',
              borderRadius: 8,
              boxShadow: '0 4px 16px rgba(0,0,0,0.12)',
              width: 180,
              padding: 8,
              boxSizing: 'border-box',
              display: 'flex',
              flexDirection: 'column',
            }}
          >
            <button
              className="skill-uninstall-btn"
              style={{ height: 32 }}
              onClick={async () => {
                setMenuOpen(false);
                try { await openSkillFolder(skillSlug); } catch (e) { console.error('Failed to open folder:', e); }
              }}
            >
              {t('skills.open_folder')}
            </button>
            <button
              className="skill-uninstall-btn"
              style={{ height: 32, marginTop: 2 }}
              onClick={() => { setMenuOpen(false); setDialogOpen(true); }}
            >
              {t('skills.uninstall')}
            </button>
          </div>
        )}
      </div>
    </>
  );
}

function SkillCard({
  skill,
  isDefault,
  isToggling,
  onToggle,
  onUninstalled,
}: {
  skill: Skill;
  isDefault: boolean;
  isToggling: boolean;
  onToggle: () => void;
  onUninstalled: () => void;
}) {
  const descRef = useRef<HTMLParagraphElement>(null);
  const nameRef = useRef<HTMLDivElement>(null);
  const [isTruncated, setIsTruncated] = useState(false);
  const [showTooltip, setShowTooltip] = useState(false);
  const [tooltipAbove, setTooltipAbove] = useState(false);
  const [nameTruncated, setNameTruncated] = useState(false);
  const [showNameTooltip, setShowNameTooltip] = useState(false);
  const [nameTooltipAbove, setNameTooltipAbove] = useState(false);

  useEffect(() => {
    requestAnimationFrame(() => {
      if (descRef.current) {
        setIsTruncated(descRef.current.scrollHeight - descRef.current.clientHeight > 1);
      }
      if (nameRef.current) {
        setNameTruncated(nameRef.current.scrollWidth > nameRef.current.clientWidth);
      }
    });
  }, [skill.description, skill.name]);

  return (
    <div
      style={{
        width: 326,
        height: 140,
        padding: 16,
        background: '#F6F6F6',
        borderRadius: 6,
        boxSizing: 'border-box',
        position: 'relative',
      }}
    >
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          height: 20,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', minWidth: 0, flex: 1 }}>
          <div
            style={{
              width: 32,
              height: 32,
              background: '#E1EEFE',
              borderRadius: 4,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              flexShrink: 0,
            }}
          >
            <span style={{ fontSize: 16, color: '#3790FA', fontWeight: 600, lineHeight: 1 }}>
              {skill.name.charAt(0).toUpperCase()}
            </span>
          </div>
          <div
            className="relative"
            onMouseEnter={() => {
              if (!nameTruncated) return;
              if (nameRef.current) {
                const rect = nameRef.current.getBoundingClientRect();
                setNameTooltipAbove(rect.bottom + 100 > window.innerHeight);
              }
              setShowNameTooltip(true);
            }}
            onMouseLeave={() => setShowNameTooltip(false)}
            style={{ flex: 1, minWidth: 0, marginLeft: 8 }}
          >
            <div
              ref={nameRef}
              style={{
                fontSize: 14,
                color: 'var(--pc-text-primary)',
                fontWeight: 600,
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap',
              }}
            >
              {skill.name}
            </div>
            {nameTruncated && showNameTooltip && (
              <div
                className={`absolute z-50 left-0 p-2 rounded-md text-xs whitespace-pre-wrap shadow-lg border ${nameTooltipAbove ? 'bottom-full mb-1' : 'top-full mt-1'}`}
                style={{
                  background: '#ffffff',
                  color: 'var(--pc-text-primary)',
                  borderColor: 'var(--pc-border)',
                  maxWidth: 300,
            overflowWrap: 'break-word',
                }}
              >
                {skill.name}
              </div>
            )}
          </div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', flexShrink: 0 }}>
          {!isDefault && (
            <SkillOperateMenu
              skillSlug={skill.slug}
              onUninstalled={onUninstalled}
            />
          )}
          <div style={!isDefault ? { marginLeft: 4 } : undefined}>
            <SkillSwitch
            enabled={skill.enabled}
            loading={isToggling}
            onToggle={onToggle}
          />
          </div>
        </div>
      </div>

      <div
        className="relative"
        onMouseEnter={() => {
          if (!isTruncated) return;
          if (descRef.current) {
            const rect = descRef.current.getBoundingClientRect();
            setTooltipAbove(rect.bottom + 100 > window.innerHeight);
          }
          setShowTooltip(true);
        }}
        onMouseLeave={() => setShowTooltip(false)}
      >
        <p
          ref={descRef}
          style={{
            width: 294,
            height: 40,
            fontSize: 14,
            color: 'var(--pc-text-muted)',
            margin: 0,
            marginTop: 14,
            lineHeight: '20px',
            overflow: 'hidden',
            display: '-webkit-box',
            WebkitLineClamp: 2,
            WebkitBoxOrient: 'vertical',
            wordBreak: 'break-all',
          }}
        >
          {skill.description}
        </p>
        {isTruncated && showTooltip && (
          <div
            className={`absolute z-50 left-0 p-2 rounded-md whitespace-pre-wrap shadow-lg border ${tooltipAbove ? 'bottom-full mb-1' : 'top-full mt-1'}`}
            style={{
              background: '#ffffff',
              color: 'var(--pc-text-primary)',
              borderColor: 'var(--pc-border)',
              maxWidth: 300,
              wordBreak: 'break-all',
              fontSize: 14,
            }}
          >
            {skill.description}
          </div>
        )}
      </div>

      <div style={{ position: 'absolute', bottom: 16, left: 16 }}>
        {isDefault ? (
          <span
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              height: 24,
              paddingLeft: 8,
              paddingRight: 8,
              fontSize: 12,
              color: '#3790FA',
              background: '#E1EEFE',
              borderRadius: 10,
              boxSizing: 'border-box',
            }}
          >
            {t('skills.certified')}
          </span>
        ) : (
          <span
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              height: 24,
              paddingLeft: 8,
              paddingRight: 8,
              fontSize: 12,
              color: 'var(--pc-text-secondary)',
              background: '#E6E6E6',
              borderRadius: 10,
              boxSizing: 'border-box',
            }}
          >
            {t('skills.uncertified')}
          </span>
        )}
      </div>
    </div>
  );
}

export default function Skills() {
  const { data: skills, loading, error, refetch } = useSkills();
  const [toggling, setToggling] = useState<string | null>(null);
  const togglingRef = useRef<string | null>(null);
  const lastToggleTimeRef = useRef(0);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchFocused, setIsSearchFocused] = useState(false);
  const searchInputRef = useRef<HTMLInputElement>(null);
  const [addDialogOpen, setAddDialogOpen] = useState(false);
  const [tip, setTip] = useState<{ success: boolean; message: string } | null>(null);
  const [atBottom, setAtBottom] = useState(false);
  const scrollRef = useRef<HTMLDivElement>(null);

  const handleScroll = useCallback((e: React.UIEvent<HTMLDivElement>) => {
    const el = e.currentTarget;
    const isAtBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 1;
    setAtBottom(isAtBottom);
  }, []);

  useEffect(() => {
    const handler = (e: MouseEvent) => e.preventDefault();
    document.addEventListener('contextmenu', handler);
    return () => document.removeEventListener('contextmenu', handler);
  }, []);

  const handleToggle = async (skillName: string, currentEnabled: boolean) => {
    const now = Date.now();
    if (now - lastToggleTimeRef.current < 500) return;
    lastToggleTimeRef.current = now;
    if (togglingRef.current === skillName) return;
    togglingRef.current = skillName;
    setToggling(skillName);
    try {
      await toggleSkillApi(skillName, !currentEnabled);
      refetch();
    } catch (err) {
      console.error('Failed to toggle skill:', err);
      setToggling(null);
      togglingRef.current = null;
    }
  };

  useEffect(() => {
    if (toggling) {
      setToggling(null);
      togglingRef.current = null;
    }
  }, [skills, error]);

  const normalizedSearchQuery = searchQuery.trim().toLowerCase();
  const hasSearchQuery = normalizedSearchQuery.length > 0;

  const filtered = (skills || []).filter((skill) => {
    if (!hasSearchQuery) return true;
    const q = normalizedSearchQuery;
    return skill.name.toLowerCase().includes(q);
  });

  const sorted = filtered;
  const emptyText = (skills?.length || 0) === 0 ? t('skills.none') : hasSearchQuery ? t('skills.no_results') : t('skills.none');

  if (loading && !skills) {
    return <PageLoadingState />;
  }

  if (error) {
    return (
      <div style={{ textAlign: 'center', padding: 24 }}>
        <p style={{ color: 'var(--pc-text-muted)' }}>{t('skills.empty')}</p>
      </div>
    );
  }

  return (
    <div
      style={{
        maxWidth: 1040,
        width: '100%',
        padding: '0 0 0 24px',
        boxSizing: 'border-box',
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
      }}
    >
      <div
        style={{
          height: 36,
          display: 'flex',
          alignItems: 'center',
          flexShrink: 0,
          paddingRight: 24,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', marginLeft: 16 }}>
          <span style={{ fontSize: 14, color: '#000000', fontWeight: 500 }}>
            {tf('skills.list_count', { count: skills?.length || 0 })}
          </span>
        </div>

        <div style={{ flex: 1 }} />

        <div style={{ display: 'flex', alignItems: 'center' }}>
          <div
            onClick={() => searchInputRef.current?.focus()}
            style={{
              width: 236,
              height: 36,
              background: isSearchFocused ? '#FFFFFF' : '#E6E6E6',
              borderRadius: 4,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              paddingLeft: 10,
              paddingRight: 10,
              boxSizing: 'border-box',
              border: isSearchFocused ? '2px solid #3790FA' : '2px solid transparent',
              transition: 'background 0.2s ease, border-color 0.2s ease',
              cursor: 'text',
            }}
          >
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                width: '100%',
                justifyContent: isSearchFocused || searchQuery ? 'flex-start' : 'center',
              }}
            >
              <img
                src={`${assetBasePath}/icons/search.svg`}
                alt="search"
                width={14}
                height={14}
                style={{ flexShrink: 0 }}
              />
              <input
                style={{
                  border: 'none',
                  outline: 'none',
                  background: 'transparent',
                  fontSize: 14,
                  color: 'var(--pc-text-primary)',
                  width: isSearchFocused || searchQuery ? '100%' : 40,
                  flex: isSearchFocused || searchQuery ? 1 : 'none',
                  padding: 0,
                  textAlign: 'left',
                  marginLeft: 6,
                }}
                ref={searchInputRef}
                type="text"
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                onFocus={() => setIsSearchFocused(true)}
                onBlur={() => setIsSearchFocused(false)}
                placeholder={t('skills.search')}
                className="placeholder-[#A6A6A6]"
              />
              {searchQuery && (
                <button
                  onClick={() => setSearchQuery('')}
                  style={{
                    border: 'none',
                    background: 'transparent',
                    cursor: 'pointer',
                    padding: 0,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    flexShrink: 0,
                    color: 'var(--pc-text-muted)',
                  }}
                >
                  <X size={14} />
                </button>
              )}
            </div>
          </div>

          <div style={{ marginLeft: 8 }}>
            <ImportMenu
              onImportResult={(success) => {
                setTip({ success, message: success ? t('skills.add_success') : t('skills.add_failed') });
                if (success) refetch();
                setTimeout(() => setTip(null), 3000);
              }}
              onAddClick={() => setAddDialogOpen(true)}
            />
          </div>

          {/* 技能中心按钮已隐藏 */}
        </div>
      </div>

      <div
        className="skill-list-scroll scrollbar-auto-hide"
        ref={scrollRef}
        onScroll={handleScroll}
        style={{
          flex: 1,
          minHeight: 0,
          overflowY: 'overlay',
          overflowX: 'hidden',
          marginTop: 8,
          paddingRight: 24,
          paddingBottom: atBottom ? 16 : 0,
          position: 'relative',
        }}
      >
        {sorted.length === 0 ? (
          <div
            style={{
              position: 'absolute',
              top: 0,
              right: 0,
              bottom: 0,
              left: 0,
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              transform: 'translate(-28px, -60px)',
            }}
          >
            <img
              src={`${assetBasePath}/icons/noDialogue.png`}
              alt=""
              style={{ width: 128, height: 128, display: 'block' }}
            />
            <span style={{ marginTop: 0, fontSize: 14, color: 'rgba(0, 0, 0, 0.35)' }}>
              {emptyText}
            </span>
          </div>
        ) : (
          <div
            style={{
              width: 992,
              display: 'flex',
              flexWrap: 'wrap',
            }}
          >
            {sorted.map((skill, idx) => {
              const isLastInRow = (idx + 1) % 3 === 0;
              return (
                <div key={skill.name} style={{ marginRight: isLastInRow ? 0 : 7, marginBottom: 7 }}>
                  <SkillCard
                    skill={skill}
                    isDefault={skill.is_default}
                    isToggling={toggling === skill.slug}
                    onToggle={() => handleToggle(skill.slug, skill.enabled)}
                    onUninstalled={() => refetch()}
                  />
                </div>
              );
            })}
          </div>
        )}
      </div>
      <AddSkillDialog
        open={addDialogOpen}
        onCancel={() => setAddDialogOpen(false)}
        onCreated={() => { setAddDialogOpen(false); refetch(); setTip({ success: true, message: t('skills.add_success') }); setTimeout(() => setTip(null), 3000); }}
        existingSlugs={(skills ?? []).map((s: Skill) => s.slug)}
      />
      {tip && (
        <div
          style={{
            position: 'fixed',
            top: 0,
            left: 0,
            width: '100vw',
            height: '100vh',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 300,
            pointerEvents: 'none',
          }}
          onClick={() => setTip(null)}
        >
          <div
            style={{
              padding: '16px 24px',
              borderRadius: 8,
              fontSize: 14,
              color: 'var(--pc-text-primary)',
              background: '#ffffff',
              border: '1px solid var(--pc-border)',
              boxShadow: '0 6px 30px rgba(0,0,0,0.12)',
              display: 'flex',
              alignItems: 'center',
              pointerEvents: 'auto',
            }}
            onClick={(e) => e.stopPropagation()}
          >
            <img
              src={`${assetBasePath}/icons/${tip.success ? 'dialog-success.svg' : 'dialog-error.svg'}`}
              alt={tip.success ? 'success' : 'error'}
              width={16}
              height={16}
              style={{ flexShrink: 0 }}
            />
            <span style={{ marginLeft: 8 }}>{tip.message}</span>
          </div>
        </div>
      )}
    </div>
  );
}

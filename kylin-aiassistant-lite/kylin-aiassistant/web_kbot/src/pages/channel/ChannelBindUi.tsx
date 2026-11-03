import { useState } from 'react';
import type { CSSProperties, ReactNode } from 'react';
import { AlertTriangle, CheckCircle2, Eye, EyeOff } from 'lucide-react';
import { assetBasePath } from '@/lib/basePath';

const CHANNEL_CONFIG_BUTTON_BG = '#E5E5E5';
const CHANNEL_CONFIG_BUTTON_HOVER_BG = '#D9D9D9';
const CHANNEL_CONFIG_BUTTON_ACTIVE_BG = '#CCCCCC';
const CHANNEL_CONNECT_BUTTON_BG = '#3790FA';
const CHANNEL_CONNECT_BUTTON_HOVER_BG = '#2F7DE1';
const CHANNEL_CONNECT_BUTTON_ACTIVE_BG = '#2568C7';

interface ChannelBindHeaderProps {
  icon: string;
  alt: string;
  title: string;
  description: string;
  descriptionMaxWidth?: number;
}

const titleStyle: CSSProperties = {
  fontFamily: "'Noto Sans CJK SC', sans-serif",
  fontSize: 24,
  fontWeight: 500,
  lineHeight: 'normal',
  textAlign: 'center',
  letterSpacing: 0,
  color: '#000000',
  marginTop: 0,
  marginBottom: 2,
};

const descriptionStyle: CSSProperties = {
  fontFamily: "'Noto Sans CJK SC', sans-serif",
  fontSize: 14,
  fontWeight: 400,
  lineHeight: 'normal',
  textAlign: 'center',
  letterSpacing: 0,
  color: 'rgba(0, 0, 0, 0.6)',
  marginTop: 0,
  marginBottom: 16,
};

export function ChannelBindHeader({
  icon,
  alt,
  title,
  description,
  descriptionMaxWidth = 400,
}: ChannelBindHeaderProps) {
  return (
    <>
      <div style={{ marginBottom: 8 }}>
        <img src={`${assetBasePath}/icons/${icon}`} alt={alt} width="64" height="64" />
      </div>
      <h1 style={titleStyle}>{title}</h1>
      <p style={{ ...descriptionStyle, maxWidth: descriptionMaxWidth }}>{description}</p>
    </>
  );
}

export function ChannelErrorAlert({ message, visible = true }: { message: string | null; visible?: boolean }) {
  if (!message || !visible) return null;

  return (
    <div
      className="flex items-center rounded-lg"
      style={{
        display: 'flex',
        alignItems: 'center',
        background: 'rgba(239,68,68,0.06)',
        borderRadius: 6,
        marginBottom: 16,
        padding: '8px 16px',
      }}
    >
      <AlertTriangle
        className="flex-shrink-0"
        style={{ color: '#ef4444', width: 16, height: 16, marginRight: 8 }}
      />
      <span style={{ fontSize: 14, color: '#ef4444' }}>{message}</span>
    </div>
  );
}

export function ChannelBindSuccess({ label }: { label: string }) {
  return (
    <div
      className="flex flex-col items-center"
      style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', marginTop: 16 }}
    >
      <CheckCircle2 style={{ color: '#10b981', width: 48, height: 48, marginBottom: 16 }} />
      <span
        style={{
          fontFamily: "'Noto Sans CJK SC', sans-serif",
          fontSize: 12,
          fontWeight: 500,
          color: '#10b981',
          textAlign: 'center',
        }}
      >
        {label}
      </span>
    </div>
  );
}

interface ChannelModeOption {
  value: string;
  label: string;
}

interface ChannelModeSelectorProps {
  value: string;
  options: ChannelModeOption[];
  onChange: (value: string) => void;
  visible?: boolean;
  variant?: 'ring' | 'filled';
}

function getModeSelectorContainerStyle(variant: 'ring' | 'filled'): CSSProperties {
  if (variant === 'filled') {
    return {
      display: 'flex',
      flexShrink: 0,
      justifyContent: 'flex-start',
      alignItems: 'flex-start',
      marginBottom: 16,
    };
  }

  return {
    display: 'flex',
    alignItems: 'center',
    marginBottom: 16,
  };
}

function getModeOptionStyle(variant: 'ring' | 'filled'): CSSProperties {
  if (variant === 'filled') {
    return {
      display: 'flex',
      alignItems: 'center',
      height: 16,
      overflow: 'hidden',
      cursor: 'pointer',
    };
  }

  return {
    display: 'flex',
    alignItems: 'center',
    cursor: 'pointer',
  };
}

function getModeIndicatorStyle(checked: boolean, variant: 'ring' | 'filled'): CSSProperties {
  if (variant === 'filled') {
    return {
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      width: 16,
      height: 16,
      minWidth: 16,
      maxWidth: 16,
      minHeight: 16,
      maxHeight: 16,
      borderRadius: 8,
      border: '1px solid rgba(0, 0, 0, 0.1)',
      background: checked ? '#3790FA' : '#E6E6E6',
      marginRight: 8,
    };
  }

  return {
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    width: 16,
    height: 16,
    borderRadius: '50%',
    border: checked ? '5px solid #3790FA' : '1.5px solid #d0d0d0',
    background: checked ? '#fff' : 'transparent',
    marginRight: 8,
  };
}

function getModeLabelStyle(variant: 'ring' | 'filled'): CSSProperties {
  if (variant === 'filled') {
    return {
      flexShrink: 0,
      fontFamily: "'Source Han Sans', 'Noto Sans CJK SC', sans-serif",
      fontSize: 14,
      fontWeight: 400,
      lineHeight: '20px',
      color: 'rgba(0, 0, 0, 0.85)',
    };
  }

  return {
    fontSize: 14,
    fontWeight: 400,
    color: 'rgba(0,0,0,0.85)',
  };
}

export function ChannelModeSelector({
  value,
  options,
  onChange,
  visible = true,
  variant = 'ring',
}: ChannelModeSelectorProps) {
  if (!visible) return null;

  return (
    <div style={getModeSelectorContainerStyle(variant)}>
      {options.map((option, index) => {
        const checked = option.value === value;
        return (
          <label
            key={option.value}
            onClick={() => onChange(option.value)}
            style={{
              ...getModeOptionStyle(variant),
              marginRight: index === options.length - 1 ? 0 : 24,
            }}
          >
            <div className="flex items-center justify-center" style={getModeIndicatorStyle(checked, variant)}>
              {variant === 'filled' && checked ? (
                <span
                  style={{
                    width: 6,
                    height: 6,
                    borderRadius: '50%',
                    background: '#FFFFFF',
                  }}
                />
              ) : null}
            </div>
            <span style={getModeLabelStyle(variant)}>{option.label}</span>
          </label>
        );
      })}
    </div>
  );
}

interface ChannelTextFieldProps {
  label: string;
  value: string;
  onChange: (value: string) => void;
  placeholder: string;
  required?: boolean;
  type?: string;
  rightControl?: ReactNode;
}

const fieldLabelStyle: CSSProperties = {
  fontFamily: "'Noto Sans CJK SC', sans-serif",
  fontSize: 14,
  fontWeight: 400,
  color: 'rgba(0, 0, 0, 0.85)',
  height: 21,
  lineHeight: '21px',
};

const fieldInputBaseStyle: CSSProperties = {
  fontFamily: "'Noto Sans CJK SC', sans-serif",
  fontSize: 14,
  fontWeight: 400,
  borderRadius: 4,
  color: '#333',
  height: 36,
  width: '100%',
  paddingLeft: 12,
  paddingRight: 12,
  boxSizing: 'border-box',
  outline: 'none',
};

export function ChannelTextField({
  label,
  value,
  onChange,
  placeholder,
  required = true,
  type = 'text',
  rightControl,
}: ChannelTextFieldProps) {
  const [focused, setFocused] = useState(false);
  const inputStyle: CSSProperties = {
    ...fieldInputBaseStyle,
    background: focused ? '#FFFFFF' : '#E6E6E6',
    border: focused ? '2px solid #3790FA' : '2px solid transparent',
  };
  return (
    <div
      className="flex flex-col"
      style={{ display: 'flex', flexDirection: 'column', height: 61, padding: 0 }}
    >
      <label style={fieldLabelStyle}>
        {label} {required ? <span style={{ color: '#F53F3F' }}>*</span> : null}
      </label>
      {rightControl ? (
        <div className="relative" style={{ position: 'relative', marginTop: 4 }}>
          <input
            type={type}
            value={value}
            onChange={(event) => onChange(event.target.value)}
            placeholder={placeholder}
            className="channel-input"
            style={{ ...inputStyle, paddingRight: 36 }}
            onFocus={() => setFocused(true)}
            onBlur={() => setFocused(false)}
          />
          {rightControl}
        </div>
      ) : (
        <input
          type={type}
          value={value}
          onChange={(event) => onChange(event.target.value)}
          placeholder={placeholder}
          className="channel-input"
          style={{ ...inputStyle, marginTop: 4 }}
          onFocus={() => setFocused(true)}
          onBlur={() => setFocused(false)}
        />
      )}
    </div>
  );
}

interface ChannelSecretFieldProps {
  label: string;
  value: string;
  onChange: (value: string) => void;
  placeholder: string;
  showSecret: boolean;
  onToggleSecret: () => void;
}

export function ChannelSecretField({
  label,
  value,
  onChange,
  placeholder,
  showSecret,
  onToggleSecret,
}: ChannelSecretFieldProps) {
  const iconStyle: CSSProperties = { width: 16, height: 16, color: '#999' };
  return (
    <ChannelTextField
      label={label}
      value={value}
      onChange={onChange}
      placeholder={placeholder}
      type={showSecret ? 'text' : 'password'}
      rightControl={
        <button
          type="button"
          onClick={onToggleSecret}
          className="rounded hover:bg-gray-100 transition-colors"
          style={{
            position: 'absolute',
            right: 10,
            top: 6,
            width: 24,
            height: 24,
            padding: 2,
            border: 'none',
            background: 'transparent',
            cursor: 'pointer',
          }}
        >
          {showSecret ? <Eye className="w-4 h-4" style={iconStyle} /> : <EyeOff className="w-4 h-4" style={iconStyle} />}
        </button>
      }
    />
  );
}

function ChannelButtonSpinner() {
  return (
    <svg className="w-4 h-4 animate-spin" viewBox="0 0 50 50" fill="none" style={{ width: 16, height: 16 }}>
      <circle cx="25" cy="25" r="20" stroke="#fff" strokeWidth="4" strokeLinecap="round" strokeDasharray="90 200" />
    </svg>
  );
}

interface ChannelBindActionsProps {
  cancelLabel: string;
  submitLabel: string;
  onCancel: () => void;
  onSubmit: () => void;
  submitting: boolean;
  canSubmit: boolean;
  compact?: boolean;
}

export function ChannelBindActions({
  cancelLabel,
  submitLabel,
  onCancel,
  onSubmit,
  submitting,
  canSubmit,
  compact = false,
}: ChannelBindActionsProps) {
  const [isCancelHovered, setIsCancelHovered] = useState(false);
  const [isCancelPressed, setIsCancelPressed] = useState(false);
  const [isSubmitHovered, setIsSubmitHovered] = useState(false);
  const [isSubmitPressed, setIsSubmitPressed] = useState(false);
  const buttonBase: CSSProperties = {
    fontFamily: "'Noto Sans CJK SC', sans-serif",
    fontSize: 14,
    fontWeight: 400,
    borderRadius: 6,
    border: 'none',
  };
  const fixedButton = compact ? { width: 64, height: 36, padding: 0 } : { padding: '8px 20px' };
  const submitDisabled = submitting || !canSubmit;
  const cancelBackground = isCancelPressed
    ? CHANNEL_CONFIG_BUTTON_ACTIVE_BG
    : isCancelHovered
      ? CHANNEL_CONFIG_BUTTON_HOVER_BG
      : CHANNEL_CONFIG_BUTTON_BG;
  const submitBackground = isSubmitPressed
    ? CHANNEL_CONNECT_BUTTON_ACTIVE_BG
    : isSubmitHovered
      ? CHANNEL_CONNECT_BUTTON_HOVER_BG
      : CHANNEL_CONNECT_BUTTON_BG;

  return (
    <div
      className="flex items-end justify-end"
      style={{ display: 'flex', alignItems: 'flex-end', justifyContent: 'flex-end', height: 52, padding: 0 }}
    >
      <button
        onClick={onCancel}
        disabled={submitting}
        style={{
          ...buttonBase,
          ...fixedButton,
          background: cancelBackground,
          color: 'rgba(0, 0, 0, 0.85)',
          cursor: submitting ? 'not-allowed' : 'pointer',
          marginRight: compact ? 8 : 12,
          transition: 'background 0.15s ease',
        }}
        onMouseEnter={() => {
          if (!submitting) setIsCancelHovered(true);
        }}
        onMouseLeave={() => {
          setIsCancelHovered(false);
          setIsCancelPressed(false);
        }}
        onMouseDown={(e) => {
          if (!submitting && e.button === 0) {
            setIsCancelPressed(true);
          }
        }}
        onMouseUp={(e) => {
          if (!submitting && e.button === 0) {
            setIsCancelPressed(false);
          }
        }}
        onBlur={() => {
          setIsCancelHovered(false);
          setIsCancelPressed(false);
        }}
      >
        {cancelLabel}
      </button>
      <button
        onClick={onSubmit}
        disabled={submitDisabled}
        className="inline-flex items-center justify-center transition-all disabled:opacity-50"
        style={{
          ...buttonBase,
          ...fixedButton,
          display: 'inline-flex',
          alignItems: 'center',
          justifyContent: 'center',
          background: submitBackground,
          color: '#FFFFFF',
          cursor: submitDisabled ? 'not-allowed' : 'pointer',
          transition: 'background 0.15s ease',
        }}
        onMouseEnter={() => {
          if (!submitDisabled) setIsSubmitHovered(true);
        }}
        onMouseLeave={() => {
          setIsSubmitHovered(false);
          setIsSubmitPressed(false);
        }}
        onMouseDown={(e) => {
          if (!submitDisabled && e.button === 0) {
            setIsSubmitPressed(true);
          }
        }}
        onMouseUp={(e) => {
          if (!submitDisabled && e.button === 0) {
            setIsSubmitPressed(false);
          }
        }}
        onBlur={() => {
          setIsSubmitHovered(false);
          setIsSubmitPressed(false);
        }}
      >
        {submitting ? <span style={{ display: 'inline-flex', marginRight: 8 }}><ChannelButtonSpinner /></span> : null}
        {submitLabel}
      </button>
    </div>
  );
}

interface ChannelManualConfigFormProps {
  primary: Omit<ChannelTextFieldProps, 'type' | 'rightControl'>;
  secret: Omit<ChannelSecretFieldProps, 'showSecret' | 'onToggleSecret'>;
  showSecret: boolean;
  onToggleSecret: () => void;
  cancelLabel: string;
  submitLabel: string;
  onCancel: () => void;
  onSubmit: () => void;
  submitting: boolean;
  canSubmit: boolean;
  compactActions?: boolean;
}

export function ChannelManualConfigForm({
  primary,
  secret,
  showSecret,
  onToggleSecret,
  cancelLabel,
  submitLabel,
  onCancel,
  onSubmit,
  submitting,
  canSubmit,
  compactActions = false,
}: ChannelManualConfigFormProps) {
  return (
    <div className="w-full flex flex-col" style={{ width: '100%', display: 'flex', flexDirection: 'column' }}>
      <div style={{ marginBottom: 16 }}>
        <ChannelTextField {...primary} />
      </div>
      <div style={{ marginBottom: 16 }}>
        <ChannelSecretField {...secret} showSecret={showSecret} onToggleSecret={onToggleSecret} />
      </div>
      <ChannelBindActions
        cancelLabel={cancelLabel}
        submitLabel={submitLabel}
        onCancel={onCancel}
        onSubmit={onSubmit}
        submitting={submitting}
        canSubmit={canSubmit}
        compact={compactActions}
      />
    </div>
  );
}

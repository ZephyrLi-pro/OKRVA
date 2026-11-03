import { assetBasePath } from '@/lib/basePath';
import type { ProviderMeta } from './providerMeta';

interface ProviderIconProps {
  meta: ProviderMeta;
  /** File name under `public/icons/`; when unset, render the letter chip. */
  bundledIcon?: string;
  size?: number;
  className?: string;
}

function iconUrl(file: string) {
  const rel = file.replace(/^\//, '');
  const subPath = rel.includes('/') ? rel : `icons/${rel}`;
  const prefix = assetBasePath.replace(/\/+$/, '');
  return prefix ? `${prefix}/${subPath}` : `/${subPath}`;
}

/**
 * Provider row / picker icon: bundled SVG when `bundledIcon` is set, otherwise
 * a colored letter chip fallback.
 */
export function ProviderIcon({
  meta,
  bundledIcon,
  size = 24,
  className,
}: ProviderIconProps) {
  if (bundledIcon) {
    return (
      <img
        src={iconUrl(bundledIcon)}
        alt=""
        className={`inline-block flex-shrink-0 object-contain ${className ?? ''}`}
        style={{ width: size, height: size }}
      />
    );
  }
  return (
    <span
      className={`inline-flex items-center justify-center flex-shrink-0 rounded-full font-semibold ${className ?? ''}`}
      style={{
        width: size,
        height: size,
        background: meta.iconBg,
        color: meta.iconColor,
        fontSize: size <= 16 ? 11 : size <= 20 ? 10 : 12,
        letterSpacing: 0,
      }}
    >
      {meta.glyph}
    </span>
  );
}

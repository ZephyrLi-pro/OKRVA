import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';

interface ChatMarkdownProps {
  content: string;
}

export default function ChatMarkdown({ content }: ChatMarkdownProps) {
  return (
    <div
      data-testid="chat-markdown"
      className="break-words text-sm text-inherit leading-relaxed [--chat-accent:var(--pc-accent)] [--chat-border:var(--pc-border)] [--chat-code-bg:var(--pc-bg)] [--chat-faint:var(--pc-text-faint)] [--chat-table-bg:color-mix(in srgb,var(--pc-bg-surface) 82%,transparent)] [--chat-table-header:color-mix(in srgb,var(--pc-bg-elevated) 88%,transparent)] [&_a]:text-[var(--chat-accent)] [&_a]:underline [&_a]:underline-offset-2 [&_blockquote]:my-3 [&_blockquote]:border-l-2 [&_blockquote]:border-[var(--chat-border)] [&_blockquote]:pl-4 [&_blockquote]:text-[var(--chat-faint)] [&_code]:rounded-md [&_code]:bg-[var(--chat-code-bg)] [&_code]:px-1.5 [&_code]:py-0.5 [&_code]:text-[0.9em] [&_h1]:mt-4 [&_h1]:mb-2 [&_h1]:text-lg [&_h1]:font-semibold [&_h2]:mt-4 [&_h2]:mb-2 [&_h2]:text-base [&_h2]:font-semibold [&_h3]:mt-3 [&_h3]:mb-1 [&_h3]:text-sm [&_h3]:font-semibold [&_hr]:my-4 [&_hr]:border-[var(--chat-border)] [&_li]:my-1 [&_ol]:my-2 [&_ol]:list-decimal [&_ol]:pl-5 [&_p]:my-2 [&_p]:leading-7 [&_pre]:my-3 [&_pre]:overflow-x-auto [&_pre]:rounded-xl [&_pre]:border [&_pre]:border-[var(--chat-border)] [&_pre]:bg-[var(--chat-code-bg)] [&_pre]:p-3 [&_pre]:text-[13px] [&_pre]:leading-6 [&_pre_code]:bg-transparent [&_pre_code]:p-0 [&_strong]:font-semibold [&_ul]:my-2 [&_ul]:list-disc [&_ul]:pl-5"
    >
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        components={{
          table: ({ children }) => (
            <div
              className="my-4 overflow-x-auto rounded-xl border"
              style={{ borderColor: 'var(--pc-border)', background: 'var(--pc-bg)' }}
            >
              <table className="min-w-full border-separate border-spacing-0 text-sm">
                {children}
              </table>
            </div>
          ),
          thead: ({ children }) => (
            <thead style={{ background: 'var(--chat-table-header)' }}>{children}</thead>
          ),
          tr: ({ children }) => <tr className="align-top">{children}</tr>,
          th: ({ children }) => (
            <th
              className="border-b px-4 py-2.5 text-left text-xs font-semibold uppercase tracking-[0.12em]"
              style={{ borderColor: 'var(--chat-border)', color: 'var(--pc-text-muted)' }}
            >
              {children}
            </th>
          ),
          td: ({ children }) => (
            <td
              className="border-t px-4 py-3 align-top text-sm"
              style={{
                borderColor: 'var(--chat-border)',
                color: 'var(--pc-text-secondary)',
                background: 'var(--chat-table-bg)',
              }}
            >
              {children}
            </td>
          ),
        }}
      >
        {content}
      </ReactMarkdown>
    </div>
  );
}

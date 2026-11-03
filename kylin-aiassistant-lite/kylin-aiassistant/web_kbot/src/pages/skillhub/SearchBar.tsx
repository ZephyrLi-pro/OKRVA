import { useState, useEffect, useRef } from 'react';
import { Search, X, Loader2 } from 'lucide-react';
import { t } from '@/lib/i18n';

interface SearchBarProps {
  onSearch: (query: string) => void;
  isLoading: boolean;
}

export default function SearchBar({ onSearch, isLoading }: SearchBarProps) {
  const [query, setQuery] = useState('');
  const debounceRef = useRef<ReturnType<typeof setTimeout>>(undefined);

  useEffect(() => {
    if (debounceRef.current) {
      clearTimeout(debounceRef.current);
    }

    debounceRef.current = setTimeout(() => {
      onSearch(query.trim());
    }, 300);

    return () => {
      if (debounceRef.current) {
        clearTimeout(debounceRef.current);
      }
    };
  }, [query, onSearch]);

  return (
    <div className="relative">
      <Search
        className="absolute left-3 top-1/2 -translate-y-1/2 h-4 w-4"
        style={{ color: 'rgba(0, 0, 0, 0.3)' }}
      />
      <input
        type="text"
        value={query}
        onChange={(e) => setQuery(e.target.value)}
        placeholder={t('skillhub.search.placeholder')}
        className="w-full pl-10 pr-10 py-2.5 text-sm rounded-lg outline-none transition-all placeholder:text-gray-400"
        style={{
          background: '#F5F5F7',
          border: '1px solid rgba(0, 0, 0, 0.06)',
          color: 'rgba(0, 0, 0, 0.9)',
        }}
        onFocus={(e) => {
          e.currentTarget.style.borderColor = 'rgba(0, 122, 255, 0.5)';
          e.currentTarget.style.boxShadow = '0 0 0 3px rgba(0, 122, 255, 0.1)';
        }}
        onBlur={(e) => {
          e.currentTarget.style.borderColor = 'rgba(0, 0, 0, 0.06)';
          e.currentTarget.style.boxShadow = 'none';
        }}
      />
      {isLoading && (
        <Loader2
          className="absolute right-10 top-1/2 -translate-y-1/2 h-4 w-4 animate-spin"
          style={{ color: 'rgba(0, 0, 0, 0.3)' }}
        />
      )}
      {query && (
        <button
          onClick={() => setQuery('')}
          className="absolute right-3 top-1/2 -translate-y-1/2 p-0.5 rounded transition-colors"
          style={{ color: 'rgba(0, 0, 0, 0.3)' }}
          onMouseEnter={(e) => {
            e.currentTarget.style.color = 'rgba(0, 0, 0, 0.6)';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.color = 'rgba(0, 0, 0, 0.3)';
          }}
        >
          <X className="h-4 w-4" />
        </button>
      )}
    </div>
  );
}

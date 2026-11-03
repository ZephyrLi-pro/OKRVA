import { useState, useCallback, useEffect } from 'react';
import { Store, Loader2 } from 'lucide-react';
import { searchSkills, listInstalled } from '@/lib/skillhubApi';
import { t } from '@/lib/i18n';
import SearchBar from './SearchBar';
import ShowcaseTab from './ShowcaseTab';
import SkillCardGrid from './SkillCardGrid';
import CategoryPills from './CategoryPills';
import InstallModal from './InstallModal';
import type { SkillSearchResult, InstalledSkill } from './types';

export default function SkillHubPage() {
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<SkillSearchResult[]>([]);
  const [searchLoading, setSearchLoading] = useState(false);
  const [installedSkills, setInstalledSkills] = useState<InstalledSkill[]>([]);
  const [category, setCategory] = useState('hot');
  const [modalSlug, setModalSlug] = useState<string | null>(null);
  const [modalName, setModalName] = useState('');
  const [tabLoading, setTabLoading] = useState(true);

  const installedSlugs = new Set(installedSkills.map((s) => s.slug));

  const fetchInstalled = useCallback(async () => {
    try {
      const resp = await listInstalled();
      setInstalledSkills(resp.skills);
    } catch {
      // silently ignore
    }
  }, []);

  useEffect(() => {
    fetchInstalled();
  }, [fetchInstalled]);

  const handleSearch = useCallback(async (query: string) => {
    setSearchQuery(query);
    if (!query) {
      setSearchResults([]);
      return;
    }
    setSearchLoading(true);
    try {
      const resp = await searchSkills(query, 100);
      setSearchResults(resp.results);
    } catch {
      setSearchResults([]);
    } finally {
      setSearchLoading(false);
    }
  }, []);

  const handleInstall = (slug: string) => {
    const skill = searchResults.find((s) => s.slug === slug);
    setModalSlug(slug);
    setModalName(skill?.name || slug);
  };

  const handleInstallSuccess = () => {
    fetchInstalled();
  };

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: '100vh',
        overflow: 'hidden',
        background: '#F5F5F7',
      }}
    >
      <div className="shrink-0" style={{ paddingTop: 10, paddingLeft: 24, paddingRight: 24, background: '#FFFFFF' }}>
        <div className="flex items-center gap-2 mb-4" style={{ marginLeft: 8 }}>
          <Store className="h-5 w-5" style={{ color: 'var(--pc-accent)' }} />
          <h2 style={{ fontSize: 14, fontWeight: 500, color: 'rgba(0, 0, 0, 0.85)' }}>
            {t('skillhub.title')}
          </h2>
        </div>

        <div className="mb-5">
          <SearchBar onSearch={handleSearch} isLoading={searchLoading} />
        </div>

        {!searchQuery && (
          <div className="mb-4">
            <CategoryPills active={category} onChange={setCategory} installedCount={installedSkills.length} disabled={tabLoading} />
          </div>
        )}
      </div>

      <div
        className="flex-1 min-h-0"
        style={{
          display: 'flex',
          flexDirection: 'column',
          paddingLeft: 24,
          paddingRight: 24,
          paddingTop: 16,
          paddingBottom: 24,
          background: '#F5F5F7',
        }}
      >
        <div style={{ display: searchQuery ? 'none' : 'flex', flex: 1, minHeight: 0, flexDirection: 'column' }}>
          <ShowcaseTab
            category={category}
            installedSlugs={installedSlugs}
            installedSkills={installedSkills}
            onInstall={handleInstall}
            onUninstalled={fetchInstalled}
            onLoadingChange={setTabLoading}
          />
        </div>
        {searchQuery && (
          <div style={{ display: 'flex', flexDirection: 'column', flex: 1, minHeight: 0 }} className="space-y-4">
            <h3 className="text-xs font-semibold uppercase tracking-wider shrink-0" style={{ color: 'var(--pc-text-faint)' }}>
              {t('skillhub.search.results').replace('{query}', searchQuery)}
            </h3>
            {searchLoading ? (
              <div className="flex items-center justify-center py-12">
                <Loader2 className="h-8 w-8 animate-spin" style={{ color: 'var(--pc-accent)' }} />
              </div>
            ) : (
              <SkillCardGrid
                skills={searchResults}
                installedSlugs={installedSlugs}
                installingSlug={null}
                onInstall={handleInstall}
                pageSize={12}
              />
            )}
          </div>
        )}
      </div>

      {modalSlug && (
        <InstallModal
          slug={modalSlug}
          name={modalName}
          isOpen={true}
          onClose={() => setModalSlug(null)}
          onSuccess={handleInstallSuccess}
        />
      )}
    </div>
  );
}

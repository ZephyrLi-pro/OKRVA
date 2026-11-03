import { apiFetch } from './api';
import type {
  SkillSearchResult,
  ShowcaseResponse,
  InstalledSkill,
  InstallResult,
  UpgradeResult,
  UninstallResult,
  SkillDetail,
} from '../pages/skillhub/types';

const BASE = '/api/skillhub';

export function searchSkills(query: string, limit?: number): Promise<{ results: SkillSearchResult[] }> {
  const params = new URLSearchParams({ q: query });
  if (limit) params.set('limit', String(limit));
  return apiFetch<{ results: SkillSearchResult[] }>(`${BASE}/search?${params}`);
}

export function getShowcase(category: string): Promise<ShowcaseResponse> {
  const params = new URLSearchParams({ cat: category });
  return apiFetch<ShowcaseResponse>(`${BASE}/showcase?${params}`);
}

export function installSkill(
  source: string,
  force: boolean = false,
  timeout: number = 30,
): Promise<InstallResult> {
  return apiFetch<InstallResult>(`${BASE}/install`, {
    method: 'POST',
    body: JSON.stringify({ source, force, timeout }),
  });
}

export function listInstalled(): Promise<{ skills: InstalledSkill[] }> {
  return apiFetch<{ skills: InstalledSkill[] }>(`${BASE}/list`);
}

export function upgradeSkill(
  slug?: string,
  checkOnly: boolean = false,
  timeout: number = 30,
): Promise<UpgradeResult> {
  return apiFetch<UpgradeResult>(`${BASE}/upgrade`, {
    method: 'POST',
    body: JSON.stringify({ slug: slug ?? null, check_only: checkOnly, timeout }),
  });
}

export function uninstallSkill(slug: string): Promise<UninstallResult> {
  return apiFetch<UninstallResult>(`${BASE}/uninstall`, {
    method: 'POST',
    body: JSON.stringify({ slug }),
  });
}

export function getSkillDetail(slug: string): Promise<SkillDetail> {
  const params = new URLSearchParams({ slug });
  return apiFetch<SkillDetail>(`${BASE}/detail?${params}`);
}

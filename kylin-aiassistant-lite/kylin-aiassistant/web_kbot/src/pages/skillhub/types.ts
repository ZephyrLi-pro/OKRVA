export interface SkillSearchResult {
  name: string;
  slug: string;
  description: string;
  version: string;
  author: string;
  tags: string[];
  downloads: number | null;
  stars: number | null;
  created_at: string | null;
  updated_at: string | null;
  profile_url: string | null;
  zip_url: string | null;
  sha256: string | null;
}

export interface ShowcaseSkill {
  slug: string;
  name: string;
  description: string;
  description_zh: string;
  downloads: number;
  stars: number;
  category: string;
  created_at: number;
  homepage: string;
  version: string;
  icon_url: string | null;
}

export interface ShowcaseResponse {
  section: string;
  skills: ShowcaseSkill[];
}

export interface InstalledSkill {
  name: string;
  slug: string;
  version: string;
  source: string;
  zip_url: string;
  update_url: string;
  sha256: string;
  installed_at: number;
  detail?: SkillDetail | null;
}

export interface InstallResult {
  success: boolean;
  slug: string;
  message: string;
}

export interface UpgradeResult {
  success: boolean;
  message: string;
}

export interface UninstallResult {
  success: boolean;
  slug: string;
  message: string;
}

export interface SkillDetailLatestVersion {
  changelog: string;
  createdAt: number;
  version: string;
}

export interface SkillDetailOwner {
  displayName: string;
  handle: string;
  image: string | null;
}

export interface SkillDetailSecurityReport {
  reportUrl: string;
  status: string;
  statusText: string;
}

export interface SkillDetailSecurityReports {
  keen: SkillDetailSecurityReport;
  sanbu: SkillDetailSecurityReport;
}

export interface SkillDetailStats {
  comments: number;
  downloads: number;
  installs: number;
  stars: number;
  versions: number;
}

export interface SkillDetailSkill {
  category: string;
  createdAt: number;
  displayName: string;
  iconUrl: string | null;
  labels: Record<string, unknown>;
  slug: string;
  source: string;
  stats: SkillDetailStats;
  summary: string;
  summary_zh: string;
  tags: Record<string, unknown>;
  updatedAt: number;
}

export interface SkillDetail {
  latestVersion: SkillDetailLatestVersion;
  owner: SkillDetailOwner;
  securityReports: SkillDetailSecurityReports;
  skill: SkillDetailSkill;
}

export type SkillHubSkill = SkillSearchResult | ShowcaseSkill | InstalledSkill;

export function isShowcaseSkill(skill: SkillHubSkill): skill is ShowcaseSkill {
  return 'category' in skill;
}

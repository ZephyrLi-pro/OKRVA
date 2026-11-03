export type NavItemPath =
  | '/agent'
  | '/agent/history'
  | '/general'
  | '/model'
  | '/skills'
  | '/skillhub'
  | '/cost'
  | '/memory'
  | '/cron';

type NavMode = {
  key: 'agent' | 'config';
  railTo: NavItemPath;
  matches: (pathname: string) => boolean;
  visibleNavItems: NavItemPath[];
};

const configNavItems: NavItemPath[] = [
  '/agent',
  '/general',
  '/model',
  '/skills',
  '/skillhub',
  '/cost',
  '/memory',
  '/cron',
];

export const navModes: NavMode[] = [
  {
    key: 'agent',
    railTo: '/agent',
    matches: (pathname) => pathname === '/agent' || pathname === '/agent/history',
    visibleNavItems: ['/agent', '/agent/history'],
  },
  {
    key: 'config',
    railTo: '/general',
    matches: (pathname) => configNavItems.includes(pathname as NavItemPath),
    visibleNavItems: ['/general', '/model', '/skills', '/skillhub', '/cost', '/memory', '/cron'],
  },
];

export function resolveNavMode(pathname: string): NavMode {
  return navModes.find((mode) => mode.matches(pathname)) ?? navModes[0]!;
}

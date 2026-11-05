// French names shown in the interface
import emotions from '../../../eyes/emotions.json';
import type { Family, PupilType } from '../data/looks';

export const PUPIL_LABELS: Record<PupilType, string> = { dot: 'Point', cat: 'Chat', acorn: 'Gland', circle: 'Rond' };

export const EMOTION_LABELS: Record<string, string> = Object.fromEntries(
    Object.entries(emotions.emotions).map(([name, emotion]) => [name, emotion.label])
);

export const FAMILY_LABELS: Record<Family, string> = {
    red: 'Rouges',
    orange: 'Oranges',
    yellow: 'Jaunes',
    green: 'Verts',
    blue: 'Bleus',
    purple: 'Violets',
    pink: 'Roses',
    neutral: 'Neutres'
};

// A color that stands for each family in the filter
export const FAMILY_COLORS: Record<Family, string> = {
    red: '#E5302A',
    orange: '#F28A1E',
    yellow: '#F2DC2A',
    green: '#3CCB5A',
    blue: '#2F7BF2',
    purple: '#8A4DF0',
    pink: '#F25CC4',
    neutral: '#BDB6C9'
};

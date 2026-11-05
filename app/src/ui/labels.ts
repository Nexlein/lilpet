// French names shown in the interface
import type { Family, PupilType } from '../data/looks';

export const PUPIL_LABELS: Record<PupilType, string> = { dot: 'Point', cat: 'Chat', acorn: 'Gland', circle: 'Rond' };

// Same names as the prototype's emotion buttons (prototype/eyes.js)
export const EMOTION_LABELS: Record<string, string> = {
    neutral: 'Neutre',
    happy: 'Content',
    love: 'Amoureux',
    angry: 'Fâché',
    squint: 'Plissé',
    frown: 'Renfrogné',
    sad: 'Triste',
    surprised: 'Surpris',
    sleepy: 'Endormi',
    dizzy: 'Étourdi'
};

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

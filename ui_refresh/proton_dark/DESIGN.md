---
name: Proton Dark
colors:
  surface: '#0b1326'
  surface-dim: '#0b1326'
  surface-bright: '#31394d'
  surface-container-lowest: '#060e20'
  surface-container-low: '#131b2e'
  surface-container: '#171f33'
  surface-container-high: '#222a3d'
  surface-container-highest: '#2d3449'
  on-surface: '#dae2fd'
  on-surface-variant: '#c2c6d6'
  inverse-surface: '#dae2fd'
  inverse-on-surface: '#283044'
  outline: '#8c909f'
  outline-variant: '#424754'
  surface-tint: '#adc6ff'
  primary: '#adc6ff'
  on-primary: '#002e6a'
  primary-container: '#4d8eff'
  on-primary-container: '#00285d'
  inverse-primary: '#005ac2'
  secondary: '#bcc7de'
  on-secondary: '#263143'
  secondary-container: '#3e495d'
  on-secondary-container: '#aeb9d0'
  tertiary: '#c0c1ff'
  on-tertiary: '#1000a9'
  tertiary-container: '#8083ff'
  on-tertiary-container: '#0d0096'
  error: '#ffb4ab'
  on-error: '#690005'
  error-container: '#93000a'
  on-error-container: '#ffdad6'
  primary-fixed: '#d8e2ff'
  primary-fixed-dim: '#adc6ff'
  on-primary-fixed: '#001a42'
  on-primary-fixed-variant: '#004395'
  secondary-fixed: '#d8e3fb'
  secondary-fixed-dim: '#bcc7de'
  on-secondary-fixed: '#111c2d'
  on-secondary-fixed-variant: '#3c475a'
  tertiary-fixed: '#e1e0ff'
  tertiary-fixed-dim: '#c0c1ff'
  on-tertiary-fixed: '#07006c'
  on-tertiary-fixed-variant: '#2f2ebe'
  background: '#0b1326'
  on-background: '#dae2fd'
  surface-variant: '#2d3449'
typography:
  headline-xl:
    fontFamily: Hanken Grotesk
    fontSize: 32px
    fontWeight: '700'
    lineHeight: 40px
    letterSpacing: -0.02em
  headline-md:
    fontFamily: Hanken Grotesk
    fontSize: 20px
    fontWeight: '600'
    lineHeight: 28px
  body-lg:
    fontFamily: Hanken Grotesk
    fontSize: 16px
    fontWeight: '400'
    lineHeight: 24px
  body-md:
    fontFamily: Hanken Grotesk
    fontSize: 14px
    fontWeight: '400'
    lineHeight: 20px
  label-md:
    fontFamily: Geist
    fontSize: 12px
    fontWeight: '500'
    lineHeight: 16px
    letterSpacing: 0.05em
  label-sm:
    fontFamily: Geist
    fontSize: 11px
    fontWeight: '600'
    lineHeight: 14px
rounded:
  sm: 0.125rem
  DEFAULT: 0.25rem
  md: 0.375rem
  lg: 0.5rem
  xl: 0.75rem
  full: 9999px
spacing:
  container-padding: 1.5rem
  gutter-md: 1rem
  stack-sm: 0.5rem
  stack-md: 1rem
  safe-area: 2rem
---

## Brand & Style

The design system is engineered for utility, focus, and technical reliability. It targets power users and gamers who prioritize performance and streamlined workflows. The visual identity sits at the intersection of **Corporate Modern** and **Minimalist High-Tech**, drawing inspiration from high-end developer tools and modern gaming clients like Discord or the updated Steam interface.

The brand personality is authoritative yet approachable. It uses deep architectural tones to create a sense of immersion and permanence, while vibrant accent colors provide functional signposting without overwhelming the user. The goal is to evoke a "pro-tool" emotional response—clean, efficient, and sophisticated.

## Colors

The palette is anchored in a sophisticated dark theme. The primary foundation is built on deep navy and slate tones to reduce eye strain during extended use.

- **Primary (#3B82F6):** A vibrant, high-energy blue used strictly for primary calls to action, active states, and critical feedback.
- **Secondary (#1E293B):** A slate gray used for container backgrounds, secondary buttons, and UI chrome to create soft layering.
- **Neutral (#0F172A):** The base background color, providing a deep, ink-like canvas that makes content pop.
- **Status Colors:** Use a muted Red (#EF4444) for errors (as seen in the "No installed games found" text) and Emerald (#10B981) for success/online states.
- **Accents:** Use Slate-400 (#94A3B8) for low-priority labels and borders to maintain a low-contrast, professional appearance.

## Typography

This design system utilizes **Hanken Grotesk** for its clean, geometric, yet humanistic personality, ensuring readability at all scales. For technical metadata and labels, **Geist** provides a monospaced-adjacent feel that reinforces the "pro-software" aesthetic.

Hierarchy is established through weight and color rather than extreme size shifts. Use `white` or `slate-50` for headlines, `slate-300` for primary body text, and `slate-500` for secondary metadata. Ensure all interactive labels use the Geist font for a distinct, functional character.

## Layout & Spacing

The layout follows a **Fixed Grid** philosophy for desktop environments to maintain a predictable, stable workspace. A 12-column system is recommended for content areas, with a sidebar or header structure that remains anchored.

- **Desktop:** 24px (1.5rem) margins and 16px (1rem) gutters. Content is centered with a max-width of 1440px.
- **Density:** High-density spacing is preferred to maximize information display, consistent with desktop productivity software. 
- **Reflow:** On smaller viewports, the layout collapses into a single-column fluid grid, increasing touch targets and padding for accessibility.

## Elevation & Depth

Visual hierarchy in this design system is achieved through **Tonal Layering** and **Low-Contrast Outlines**. Avoid heavy drop shadows to keep the UI feeling "flat" and modern.

- **Level 0 (Base):** Neutral (#0F172A) for the main application background.
- **Level 1 (Surface):** Secondary (#1E293B) for sidebars, headers, and cards.
- **Borders:** Use a subtle 1px border (#334155) to define sections instead of shadows. 
- **Active States:** Elements being interacted with should receive a soft inner glow or a primary-colored border rather than a lift in Z-index.

## Shapes

The shape language is "Soft" (4px - 8px radius) to maintain a professional, structured look without appearing overly playful or aggressive. 

- **Standard Elements:** Buttons and inputs use a 4px (0.25rem) radius.
- **Large Containers:** Cards and main content panels use an 8px (0.5rem) radius.
- **Icons:** Use linear icons with a 2px stroke width to match the clean typography.

## Components

### Buttons
- **Primary:** Solid #3B82F6 background with white text. High contrast.
- **Secondary:** Transparent with a #334155 border or solid #1E293B background.
- **Ghost:** No background, #94A3B8 text, appearing on hover.

### Input Fields
- **Search/Filter:** Deep #0F172A background with a 1px #334155 border. Text is #94A3B8 until typed.
- **Focus State:** Border transitions to #3B82F6 with a subtle 2px outer glow.

### Cards & Lists
- **Game List Items:** Hover state should change the background to a slightly lighter slate (#334155). 
- **Badges/Chips:** Small, Geist-font labels with a #1E293B background and #94A3B8 text for secondary info (e.g., "Accounts (2)").

### Scrollbars
- Custom-styled scrollbars are essential for the dark theme. Use a #334155 thumb on a transparent or #0F172A track.
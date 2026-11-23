#!/usr/bin/env python3
"""Six Track's wordmark for the menu, as SVG: SIX TRACK in gold with a dark outline, a red ball and a
bat crossed behind it. Original artwork. Writes art/logo.svg (300 x 74)."""
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'logo.svg')
SVG = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 300 74" width="300" height="74">
  <defs>
    <linearGradient id="gold" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#FFF2B0"/><stop offset="0.45" stop-color="#FFD24A"/><stop offset="1" stop-color="#E08A10"/>
    </linearGradient>
    <radialGradient id="ball" cx="0.35" cy="0.32" r="0.75">
      <stop offset="0" stop-color="#FF8A80"/><stop offset="0.5" stop-color="#D81E2A"/><stop offset="1" stop-color="#7A0A12"/>
    </radialGradient>
    <linearGradient id="wood" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#F0C890"/><stop offset="0.5" stop-color="#D9A464"/><stop offset="1" stop-color="#A87438"/>
    </linearGradient>
  </defs>
  <!-- the bat, crossed behind the words -->
  <g transform="rotate(-28 150 38)" stroke="#1A1206" stroke-width="2" stroke-linejoin="round">
    <rect x="40" y="33" width="34" height="7" rx="2" fill="#2A2A30"/>
    <path d="M72 30 L238 30 Q246 30 246 37 Q246 44 238 44 L72 44 Z" fill="url(#wood)" fill-opacity="0.55"/>
  </g>
  <!-- the words: a dark outline, then the gold -->
  <g font-family="Helvetica, Arial, sans-serif" font-weight="bold" font-style="italic" font-size="44" text-anchor="middle">
    <text x="150" y="52" fill="none" stroke="#10131F" stroke-width="9" stroke-linejoin="round">SIX TRACK</text>
    <text x="150" y="52" fill="url(#gold)" stroke="#7A4A08" stroke-width="1.2">SIX TRACK</text>
  </g>
  <!-- the ball, flying off to the right, its seam and a little trail -->
  <path d="M232 20 L262 12" stroke="#FFFFFF" stroke-opacity="0.35" stroke-width="3" stroke-linecap="round"/>
  <path d="M236 26 L262 20" stroke="#FFFFFF" stroke-opacity="0.2" stroke-width="2" stroke-linecap="round"/>
  <circle cx="276" cy="16" r="12" fill="url(#ball)" stroke="#10131F" stroke-width="2.4"/>
  <path d="M266 10 Q276 18 286 22" fill="none" stroke="#FFF0D8" stroke-width="1.4" stroke-dasharray="2 2"/>
</svg>
'''
open(OUT, 'w').write(SVG)
print('->', OUT)

// Hymnal's page: the feature cards and the year
(function () {
  'use strict';
  const features = [
    ['Songs from a style', 'Describe it in your own words. The songwriter writes the lyrics and the structure, then the singer and band perform it, verses and chorus.'],
    ['Your words or its own', 'Sing your lyrics, have them written for you, or make an instrumental, in 50+ languages, with up to 4 variations at a time.'],
    ['Every setting a slider', 'Length, tempo, key, time signature; the songwriter\'s creativity and focus; the song model\'s steps, guidance, shift and solver.'],
    ['Remix', 'Drop in any song for a cover in another style, a close cover, a repainted chorus, an ending it was missing, or an extra track.'],
    ['Stems', 'Vocals, backing vocals, drums, bass, guitar, keys, percussion, strings, synth, FX, brass and woodwinds, each split out as its own WAV.'],
    ['Tempo and key', 'Measured from the audio, with the loudness; and ACE-Step listens and writes down the style, BPM, key and lyrics it hears.'],
    ['On your own PC', 'No cloud and no account. The CUDA engine makes a 30-second song in about 30 seconds on an RTX 2060; the CPU engine runs anywhere.'],
    ['Models in one click', 'The starter set downloads from the app and carries on where it stopped if the connection drops; keep the models on any drive.']
  ];
  const grid = document.getElementById('features');
  if (grid) {
    for (const [title, text] of features) {
      const card = document.createElement('article');
      const h = document.createElement('h2');
      h.textContent = title;
      const p = document.createElement('p');
      p.textContent = text;
      card.append(h, p);
      grid.appendChild(card);
    }
  }
  const year = document.getElementById('year');
  if (year) year.textContent = String(Math.max(2026, new Date().getFullYear()));
})();

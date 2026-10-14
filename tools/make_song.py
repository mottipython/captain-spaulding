#!/usr/bin/env python3
"""The music, from Animal Crackers (1930, public domain in the US): Captain Spaulding's
arrival, the march and chorus (4:59 to 6:11 in the film), joined to "Hello, I must
be going", "Hooray for Captain Spaulding" and his dance (6:40 to 8:57), leaving out
the talk between. Writes assets/art/spaulding.ogg, and src/song.h with the times of
its beats, found from the music, so that the doll can dance in time.

    tools/make_song.py FILM

FILM is the film: the Wikimedia Commons copy of "Animal Crackers (1930).webm",
whose 1080p version runs at the true speed (some other copies run 4% fast). Needs ffmpeg and numpy.
"""
import subprocess, sys, tempfile, os, wave
import numpy as np

PARTS = [(310.97, 385.81), (418.06, 562.19)]  # seconds into the film (the Commons 1080p copy)
JOIN = 0.6                                   # crossfade between them
RATE = 32000
OUT, HDR = "assets/art/spaulding.ogg", "src/song.h"

def main(film):
    os.makedirs("assets/art", exist_ok=True)
    tmp = tempfile.mkdtemp()
    (a0, a1), (b0, b1) = PARTS
    flt = (f"[0:a]atrim={a0}:{a1},asetpts=PTS-STARTPTS,afade=t=in:d=0.3[a];"
           f"[0:a]atrim={b0}:{b1},asetpts=PTS-STARTPTS,afade=t=out:st={b1 - b0 - 2.5}:d=2.5[b];"
           f"[a][b]acrossfade=d={JOIN}[m]")
    wav = os.path.join(tmp, "song.wav")
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", film, "-filter_complex", flt, "-map", "[m]",
                    "-ac", "1", "-ar", str(RATE), wav], check=True)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", wav, "-c:a", "libvorbis", "-q:a", "3", OUT], check=True)

    # beats: where the sound gets louder fastest, at the steadiest tempo near them
    w = wave.open(wav)
    x = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(float) / 32768
    hop = RATE // 100                                             # 10 ms
    env = np.sqrt(np.add.reduceat(x * x, np.arange(0, len(x) - hop, hop)) / hop)
    flux = np.clip(np.diff(np.log(env + 1e-4)), 0, None)
    flux = np.convolve(flux, np.hanning(7) / 3, "same")
    beats, t, n = [], 0.0, len(flux)
    while t < n / 100:
        # tempo for the next eight seconds, from how the onsets repeat
        i0, i1 = int(t * 100), min(n, int(t * 100) + 800)
        seg = flux[i0:i1] - flux[i0:i1].mean()
        ac = np.correlate(seg, seg, "full")[len(seg) - 1:]
        lo, hi = 100 * 60 // 170, 100 * 60 // 70                 # 70 to 170 beats a minute
        period = (lo + int(np.argmax(ac[lo:hi]))) / 100 if len(ac) > hi else 0.5
        # its phase: the offset at which the beats land on the most onsets
        best, phase = -1, 0
        for ph in range(int(period * 100)):
            idx = np.arange(i0 + ph, i1, period * 100).astype(int)
            score = flux[idx[idx < n]].sum()
            if score > best: best, phase = score, ph
        for b in np.arange(i0 + phase, i1, period * 100) / 100:
            if not beats or b - beats[-1] > period * 0.6: beats.append(round(float(b), 2))
        t = i1 / 100
    length = len(x) / RATE
    with open(HDR, "w") as f:
        f.write("/* Made by tools/make_song.py: the beats of assets/art/spaulding.ogg, in seconds. */\n")
        f.write(f"#define SONG_LENGTH {length:.2f}\n#define SONG_PART2 {a1 - a0 - JOIN:.2f}   /* where \"Hello, I must be going\" begins */\n")
        f.write(f"#define FILM_OFFSET {b0 - (a1 - a0 - JOIN):.2f}   /* film time minus song time, from there on */\n")
        f.write(f"static const float BEAT[] = {{\n")
        for i in range(0, len(beats), 12):
            f.write("    " + ", ".join(f"{b:.2f}f" for b in beats[i:i + 12]) + ",\n")
        f.write("};\n")
    print(f"{OUT}: {length:.1f} s, {os.path.getsize(OUT) // 1024} KB, {len(beats)} beats, about {60 * len(beats) / length:.0f} a minute")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])

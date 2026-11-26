#!/usr/bin/env python3
"""Generate golden MP3 vectors: libmp3lame encode, ffmpeg 'mp3' decode -> s16le.
Usage: gen_mp3_vectors.py OUTDIR   (needs ffmpeg with libmp3lame, numpy)"""
import subprocess, sys, os, json
import numpy as np

def signal(sr, ch, dur, seed):
    rng = np.random.default_rng(seed)
    n = int(sr * dur)
    t = np.arange(n) / sr
    base = (0.30*np.sin(2*np.pi*440*t) + 0.15*np.sin(2*np.pi*1234.5*t)
            + 0.10*np.sin(2*np.pi*(sr*0.31)*t))
    f0, f1 = 200.0, sr*0.45
    base += 0.20*np.sin(2*np.pi*(f0*t + (f1-f0)*t*t/(2*dur)))     # chirp
    x = np.zeros((n, ch))
    for c in range(ch):
        x[:, c] = base * (1.0 if c == 0 else 0.7)
    for k in range(1, 6):                                         # transients -> short blocks
        i = int(n * k / 6)
        m = int(sr*0.004)
        x[i:i+m, :] += rng.uniform(-0.8, 0.8, (m, ch))
    if ch == 2:                                                   # make L/R differ so M/S matters
        x[:, 1] += 0.1*np.sin(2*np.pi*777*t)
    x = np.clip(x, -0.99, 0.99)
    return (x * 32767).astype('<i2')

# name, sample rate, channels, extra encoder args
CASES = [
    ("m1_44k_st_128",  44100, 2, ["-b:a","128k"]),
    ("m1_48k_js_192",  48000, 2, ["-b:a","192k","-joint_stereo","1"]),
    ("m1_32k_mono_64", 32000, 1, ["-b:a","64k"]),
    ("m1_44k_st_320",  44100, 2, ["-b:a","320k"]),
    ("m1_44k_vbr_q2",  44100, 2, ["-q:a","2"]),
    ("m2_22k_st_64",   22050, 2, ["-b:a","64k"]),
    ("m2_24k_mono_32", 24000, 1, ["-b:a","32k"]),
    ("m2_16k_st_48",   16000, 2, ["-b:a","48k"]),
    ("m25_11k_mono_16",11025, 1, ["-b:a","16k"]),
    ("m25_8k_mono_16",  8000, 1, ["-b:a","16k"]),
]

def run(cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def main(out):
    os.makedirs(out, exist_ok=True)
    man = []
    for i, (name, sr, ch, extra) in enumerate(CASES):
        pcm = signal(sr, ch, 0.35, 100+i)
        raw_in = os.path.join(out, name + ".in.raw")
        pcm.tofile(raw_in)
        mp3 = os.path.join(out, name + ".mp3")
        ref = os.path.join(out, name + ".pcm")
        run(["ffmpeg","-y","-f","s16le","-ar",str(sr),"-ac",str(ch),"-i",raw_in,
             "-c:a","libmp3lame",*extra,"-write_xing","0","-id3v2_version","0",
             "-fflags","+bitexact","-flags","+bitexact", mp3])
        run(["ffmpeg","-y","-i",mp3,"-f","s16le","-acodec","pcm_s16le","-ac",str(ch),ref])
        os.remove(raw_in)
        man.append(dict(name=name, sample_rate=sr, channels=ch,
                        mp3_bytes=os.path.getsize(mp3), pcm_bytes=os.path.getsize(ref)))
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(man, f, indent=1)

if __name__ == "__main__":
    main(sys.argv[1])

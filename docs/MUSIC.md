# The game song

## What it should sound like

Late 80s Amiga tracker music, the kind that came with cracktros and shooters
from the Finnish scene. That sound has a few clear parts:

- **Bright, fast arpeggios.** Trackers had four channels, so one channel plays
  the notes of a chord one after another, very quickly, to fake a full chord.
- **A fat square-wave lead** with a little vibrato, singing a simple minor-key
  melody you can whistle after one listen.
- **A rolling bass** that jumps between the root note and the octave.
- **Punchy sampled drums**: a hard kick, a snappy snare, tight hi-hats.
- **A build and a break**: layers come in one at a time, then a short rising
  fill throws you back to the start of the loop.
- A heroic but slightly worried mood. You are defending something fragile.

## The loop

The song must be about **35 seconds** and loop without a seam.

137 BPM in 4/4 gives 20 bars in 35.04 seconds. That is why the built-in tune
and the prompts below use 137 BPM. If the song has a whole number of bars, the
end lands exactly on a beat and flows back into the start.

The game also blends the last 40 milliseconds into the first 40 to hide the
tiny gap every MP3 has at its ends.

## File name and format

| What                         | Value                               |
|------------------------------|-------------------------------------|
| File name                    | `theme.mp3`                         |
| Format                       | MP3, 128 kbit/s, stereo, 44.1 kHz   |
| Length                       | 35 s recommended (5 s to 8 min works) |
| Ships with the game          | `music/theme.mp3` in the game folder |
| Player's own song            | `~/.config/shooter-upper/theme.mp3` |

The player's own file wins over the shipped one. Delete it to go back. If the
file is damaged, too short or not an MP3, the game shows a dialog that says
what is wrong and plays the built-in tune instead.

If there is no `theme.mp3` at all, the built-in chip tune plays. It is also
35 seconds, so the game always has music.

## Making the song with ACE-Step 1.5 turbo XL

Generate several takes of each prompt and pick the one that sounds best.
Always use the same seed settings across prompts only if you want to compare
the prompts fairly. I have not run ACE-Step here, so the exact slider names in
your ComfyUI workflow may differ. Use the turbo defaults for steps and
guidance, and set the duration a little over 35 seconds (38 to 40) so you can
trim to a clean bar line afterwards.

**Lyrics field (all prompts):**

    [Instrumental]

### Prompt A: Heroic (my first choice)

    instrumental, amiga tracker music, 16-bit chiptune, late 1980s shoot em up,
    fast bright square wave arpeggios, fat square lead melody with vibrato,
    rolling octave bassline, punchy sampled drums, tight hi-hats, A minor,
    137 bpm, 4/4, heroic and urgent, builds in layers, short rising fill at
    the end, clean loop, no vocals, no reverb tail

### Prompt B: Darker

    instrumental, amiga mod tracker, dark chiptune, space shooter, A minor,
    137 bpm, pulsing triangle bass, stabbing square chords, noisy snare,
    tense arpeggios, minor key melody on pulse wave, driving and ominous,
    demoscene intro feel, tight loop, no vocals

### Prompt C: Bouncy

    instrumental, finnish demoscene tracker music, cheerful but fast chiptune,
    bouncy bass, sparkling arpeggios, catchy whistle-able square wave melody,
    sampled hip hop drums, 137 bpm, A minor to C major lift, arcade shooter
    title theme, loops cleanly, no vocals

### Tips

- If the result sounds too modern or too smooth, add `lo-fi, 8-bit samples,
  crunchy, dry` and remove anything about "cinematic".
- If it drifts from 137 BPM, keep the take if it sounds great and trim to
  whole bars of whatever tempo it has.
- Avoid artist names in the prompt. It is not needed, and a song that is too
  close to a real tune is a problem if you sell the game.

## Trim and convert

Find the best loop point by ear, then cut at a bar line. If your best take
runs at exactly 137 BPM, 20 bars is 35.04 seconds:

    ffmpeg -i take.wav -ss 0 -t 35.04 -ac 2 -ar 44100 -b:a 128k theme.mp3

What the options do: `-ss 0` starts at the beginning, `-t 35.04` keeps 35.04
seconds, `-ac 2` makes it stereo, `-ar 44100` sets the sample rate, and
`-b:a 128k` sets the MP3 quality. Put the result in `music/theme.mp3` in the
project, and rebuild the packages.

## Rights

A song made with ACE-Step is yours to use only as far as the model's license
and your plan allow. Check ACE-Step's current license terms before you sell
the game with a generated song in it. The built-in tune is plain code in this
project and has no such question.

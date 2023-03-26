#!/usr/bin/env python3

# wget https://dominik-braun.net/wp-content/uploads/2019/09/107-Free-Retro-Game-Sounds.zip
# 
# unzip 107-Free-Retro-Game-Sounds.zip -d tmp

import glob
import os
import shutil

dirs = sorted(glob.glob("tmp/107 Free Retro Game Sounds/*"))

dest_dir_number = 4
dest=f"sounds/{dest_dir_number:02d}"

start_number = 32

def collect_sounds():
    sounds = []
    i = start_number
    for j, d in enumerate(dirs):
        files = sorted(glob.glob(f"{d}/*.mp3"))
        for k, f in enumerate(files):
            cat = os.path.basename(d)
            cat = cat.replace(" ", "_").lower()
            cat = cat.replace("&", "and")
            cat = cat.replace(",", "")
            cat = cat.replace("'", "")
            cat = cat.replace("-", "_")

            if cat == "music":
                continue

            name = os.path.basename(f)
            name = name.replace(" ", "_").lower()
            name = name.replace("&", "and")
            name = name.replace("'", "")
            name = name.replace("(", "")
            name = name.replace(")", "")
            name = name.replace("-", "_")
            name = name.replace(".mp3", "")
            name = name.replace(".wav", "")
            new_fname = f"{i:03d}"
            i += 1
            
            sounds.append((f, i, cat, name, f"{dest}/{new_fname}.wav"))

    return sounds

sds = collect_sounds()

enum_f = open("arcade_sounds.h", 'w')
enum_f.write("#pragma once\n\n")
enum_f.write("enum class ArcadeSounds : uint16_t {\n")
for i, (f, sound_num, cat, name, new_fname) in enumerate(sds):
    # shutil.copyfile(f, new_fname)
    enum_f.write(f"  {cat}__{name} = ({dest_dir_number} << 8) | {sound_num}")
    if i < len(sds)-1:
        enum_f.write(",")
    enum_f.write("\n")
enum_f.write("};\n")

for i, (f, sound_num, cat, name, new_fname) in enumerate(sds):
    if False:
        #shutil.copyfile(f, new_fname)
        cmd = f"ffmpeg -i '{f}' -acodec pcm_s16le -ac 1 -ar 44100 '{new_fname}'"
        print(cmd)
        os.system(cmd)
    if False:
        cmd = f"ffmpeg -i '{f}' -codec:a libmp3lame -b:a 128k '{new_fname}'"
        print(cmd)
        os.system(cmd)

        

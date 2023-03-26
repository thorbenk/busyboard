#!/usr/bin/env python3

# wget https://dominik-braun.net/wp-content/uploads/2019/09/107-Free-Retro-Game-Sounds.zip
# 
# unzip 107-Free-Retro-Game-Sounds.zip -d tmp

import glob
import os
import shutil

dirs = sorted(glob.glob("tmp/107 Free Retro Game Sounds/*"))

dest_dir_number = 5
dest=f"sounds/{dest_dir_number:02d}"




def collect_sounds():
    sounds = []
    i = 0
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
            new_fname = f"{i:03d}_{cat}__{name}"
            i += 1
            
            sounds.append((f, cat, name, f"{dest}/{new_fname}.mp3"))

    return sounds

sds = collect_sounds()

enum_f = open("arcade_sounds.h", 'w')
enum_f.write("#pragma once\n\n")
enum_f.write("enum class ArcadeSounds : uint16_t {\n")
for i, (f, cat, name, new_fname) in enumerate(sds):
    # shutil.copyfile(f, new_fname)
    if True:
        cmd = f"ffmpeg -i '{f}' -codec:a libmp3lame -b:a 128k '{new_fname}'"
        print(cmd)
        os.system(cmd)
    enum_f.write(f"  {cat}__{name} = ({dest_dir_number} << 8) | {i}")
    if i < len(sds)-1:
        enum_f.write(",")
    enum_f.write("\n")
enum_f.write("};\n")

        

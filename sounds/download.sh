#!/bin/bash

mkdir -p 01

cd 01

wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/f/f4/De-Null.ogg -O 000_de_0.ogg
wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/4/4d/De-Eins.ogg -O 001_de_1.ogg 
wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/5/51/De-Zwei.ogg -O 002_de_2.ogg
wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/0/07/De-Drei.ogg -O 003_de_3.ogg
wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/e/e4/De-Vier.ogg -O 004_de_4.ogg
wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/b/b1/De-F%C3%BCnf.ogg -O 005_de_5.ogg
wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/5/5f/De-Sechs.ogg -O 006_de_6.ogg
wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/a/a2/De-Sieben.ogg -O 007_de_7.ogg
wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/b/b2/De-Acht.ogg -O 008_de_8.ogg
wget --no-clobber https://upload.wikimedia.org/wikipedia/commons/a/aa/De-Neun.ogg -O 009_de_9.ogg

# wget --no-clobber https://commons.wikimedia.org/wiki/File:Model_500_Telephone_British_ring.ogg -O 010_ringtone_british.ogg

#oggdec *.ogg
#rm *.ogg

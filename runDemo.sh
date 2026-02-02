#!/bin/bash
# Music Bottles - Usage: cap1 cap2 cap3
# Classic tracks + birthday song (all caps removed)
# Detection margin: +/-30

# Cap weights: 629, 728, 426
make clean
make
amixer set PCM 100%
sudo ./musicBottles 619 724 415 

#!/usr/bin/env bash

# This script builds Stages on the vagrant development machine, creating
# the WAV file if desired and plays it on the default ALSA device.
# Usage:
#   ./makeflash.sh [clean] [size] [wav]

# Configuration
ALSADEVICE="sysdefault" # Audio device used for playing the WAV file

echo $*

# # Build command
# COMMAND=""
# if [[ $* == *clean* ]]; then
# 	COMMAND="$COMMAND make -f stages/makefile clean &&"
# fi
# COMMAND="$COMMAND make -f stages/makefile"
# if [[ $* == *size* ]]; then
# 	COMMAND="$COMMAND && make -f stages/makefile size"
# fi
# if [[ $* == *wav* ]]; then
# 	COMMAND="$COMMAND && make -f stages/makefile wav"
# fi
COMMAND="make -f stages/makefile $*"

# Show command
echo
echo "EXECUTING:"
echo "  $COMMAND"
echo

# Run command on the vagrant development machine
# https://github.com/mklement0/n-install/issues/1#issuecomment-159089258
vagrant ssh -c "set -i; . ~/.bashrc; set +i; $COMMAND"
if [ $? -ne 0 ]; then
	exit $?
fi

# Play the WAV file on the default ALSA device
if [[ $* == *wav* ]]; then
	WAVFILE="`dirname $0`/../build/stages/stages.wav"
	echo
	echo "PLAYING:"
	echo "  `ls -l "$WAVFILE"`"
	echo
	# Seems to play more consistently when I don't specify device
	#aplay -D $ALSADEVICE -V mono "$WAVFILE"
	aplay -V mono "$WAVFILE"
fi

echo

exit 0
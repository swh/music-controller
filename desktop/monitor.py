#!/usr/bin/env python

"""
monitor.py
"""

import io
import sys
import time
import applescript
import serial
import serial.tools.list_ports


def run_as(cmd: str, app='Music', cast=str):
    ret = applescript.tell.app(app, cmd)
    if ret.code == 0:
        return cast(ret.out)
    print(ret.err, file=sys.stderr)

    return None

def run_as_list(cmd: str, app='Music', cast=str):
    string_res = run_as(cmd, app)
    if string_res is None:
        return None
    ret = string_res.split(', ')
    if cast != str:
        ret = [cast(x) for x in ret]

    return ret

"""
Convert any number form to a 1dp fixedpoint number.
"""
def fixedcast(s):
    try:
        return int(float(s) * 10)
    except ValueError:
        return 0

def get_track_times():
    try:
        track_pos, track_len = run_as_list('get {player position} & {duration} of current track', cast=fixedcast)
    except TypeError:
        track_pos = track_len = 0

    return track_pos, track_len


def send_all(sio):
    track_pos, track_len = get_track_times()
    track_artist = run_as('get artist of current track')
    track_album = run_as('get album of current track')
    track_name = run_as('get name of current track')
    print()
    print(track_artist)
    print(track_album)
    print(track_name)
    sio.write(f'BA {track_artist}\n')
    sio.write(f'AL {track_album}\n')
    sio.write(f'TR {track_name}\n')
    sio.write(f'TL {track_len}\n')
    sio.write(f'TP {track_pos}\n')
    sio.flush()

    return track_name


def send_track_pos(sio):
    track_pos, track_len = get_track_times()
    #sio.write(f'TL {track_len}\n')
    sio.write(f'TP {track_pos}\n')
    sio.flush()


def is_playing(sio) -> bool:
    state = run_as('player state')
    return state == 'playing'


def send_state(sio, playing: bool) -> bool:
    if playing:
        sio.write('PL\n')
        sio.flush()
    else:
        sio.write('PA\n')
        sio.flush()

    return playing


def main(argv):
    while True:
        ports = list(serial.tools.list_ports.grep('.'))
        ser_device = None
        for p in ports:
            if p.description == 'Nano ESP32':
                ser_device = p.device
                break
        if ser_device:
            ser = serial.Serial(ser_device, baudrate=115200, timeout=1)
            monitor(ser)
        else:
            print('Unable to find active USB serial interface', file=sys.stderr)
            time.sleep(5)
   

def monitor(ser):
    sio = io.TextIOWrapper(io.BufferedRWPair(ser, ser), encoding='utf-8')

    last_track_name = send_all(sio)
    playing: bool = is_playing(sio)
    send_state(sio, playing)

    last_sync = time.time()
    while True:
        try:
            line = sio.readline().strip()
            if line:
                cmd = line[:2]
                if len(line) > 3:
                    arg = line[3:]
                else:
                    arg = None
                match cmd:
                    case 'PP':
                        run_as('playpause')
                        playing = send_state(sio, is_playing(sio))
                        last_track_name = send_all(sio)
                    case 'LL':
                        print('List... [not implemented]')
                    case 'FA':
                        run_as('''get current track
    set favorited of current track to true''')
                    case 'SK':
                        run_as('play next track')
                        last_track_name = send_all(sio)
                        playing = send_state(sio, True)
                    case '!!':
                        print('Arduino:', arg)
                    case _:
                        print(f'Unknown command: {cmd}')

            track_name = run_as('get name of current track')
            if track_name != last_track_name:
                send_all(sio)
                playing = send_state(sio, is_playing(sio))
                last_track_name = track_name
            else:
                playing_now = is_playing(sio)
                if playing_now != playing:
                    playing = send_state(sio, playing_now)
            now = time.time()
            if playing and now > last_sync + 10:
                send_track_pos(sio)
                last_sync = now
        except serial.serialutil.SerialException:
            print('Lost connection to serial device')
            return

if __name__ == '__main__':
    main(sys.argv)

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
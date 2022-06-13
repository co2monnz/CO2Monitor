#!/usr/bin/python
import configparser
import os
import subprocess
import sys

import gitrev

Import("env")
PIOENV = env.get('PIOENV')
c = env.GetProjectConfig()

try:
    ota_host = c.get('ota', 'host')
    ota_path = c.get('ota', 'path')
    ota_url = c.get('ota', 'url')
except (configparser.NoSectionError, configparser.NoOptionError) as e:
    Fatal(f'Missing config in platformio.ini: {e}')

def Fatal(msg):
    sys.stderr.write(f'{msg}\n')
    env.Exit(1)

def PushOta(*args, **kwargs):
    if len(env.get('PROGRAM_ARGS')) == 0:
        Fatal('missing -a NODE_ID for push-ota')

    # Copy
    src_filename = f'.pio/build/{PIOENV}/firmware.bin'
    if not os.path.exists(src_filename):
        Fatal(f'Built firmware not at expected location: {src_filename}!')
    dst_filename = 'fw_%s.bin' % gitrev.version
    subprocess.check_call(['ssh', ota_host, 'mkdir', '-p',
        os.path.join(ota_path, PIOENV)])
    subprocess.check_call(['scp', src_filename,
        f'{ota_host}:{ota_path}/{PIOENV}/{dst_filename}'])

    # trigger OTA if node specified
    for node in env.get('PROGRAM_ARGS'):
        subprocess.check_call(['mosquitto_pub', '-h', ota_host, '-t',
            f'co2monitor/{node}/down/forceota', '-m',
            os.path.join(ota_url, PIOENV, dst_filename)])

env.AddCustomTarget("push-ota", dependencies=['buildprog'], actions=PushOta)

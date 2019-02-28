#!/usr/bin/env python3
from nuc_lights import *
import json
from pprint import pprint


if __name__ == "__main__":
    with open('log', 'w+') as log:
        log.write("Begin")
        try:
            with open('lights_conf.json') as f:
                data = json.load(f)
                log.write("Loaded 'lights_conf.json'\n")
                for entry in data['lights']:
                    log.write("setting %s\n" % entry)
                    setLEDColorAndSource(entry['led'], entry['source'], entry['brightness'], entry['color'])
            log.write("Completed Successfuly")
        except Exception as e:
            log.write(e)
            log.write("Failure")
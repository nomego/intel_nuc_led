#!/usr/bin/env python3

# Script by Jessica (TammyJess)
# https://github.com/TammyJess

import argparse
import subprocess


def value0_100(string):
  try: value = int(string)
  except:
    msg = "brightness needs to be an integer"
    raise argparse.ArgumentTypeError(msg)
  if value < 0 or value > 100:
    msg = "brightness needs to be in the range 0..100"
    raise argparse.ArgumentTypeError(msg)
  return value

def value0_1(string):
  try: value = int(string)
  except:
    msg = "argument needs to be an integer"
    raise argparse.ArgumentTypeError(msg)
  if value < 0 or value > 1:
    msg = "argument needs to be 0 or 1"
    raise argparse.ArgumentTypeError(msg)
  return value

def value1_10(string):
  try: value = int(string)
  except:
    msg = "blinks needs to be an integer"
    raise argparse.ArgumentTypeError(msg)
  if value < 1 or value > 10:
    msg = "blinks per second needs to be in the range 1..10"
    raise argparse.ArgumentTypeError(msg)
  return value

def value0_2(string):
  try: value = int(string)
  except:
    msg = "blinks needs to be an integer"
    raise argparse.ArgumentTypeError(msg)
  if value < 0 or value > 2:
    msg = "argument needs to be 0, 1 or 2"
    raise argparse.ArgumentTypeError(msg)
  return value

parser = argparse.ArgumentParser(prog="nucLED",add_help=False, \
  usage='%(prog)s led mode [-c \'#rrggbb\'] [-b brightness] [-t blink_type] [-f blink_freq] [--hdd mode] [--lan mode] [--scheme mode] [-h] [-v]', \
  description='A script to set the LEDs on Intel NUC 8i7-HNK boxen, requires the nuc_led kernel module. Load it with \
  \'modprobe nuc_led nuc_led_perms=0777\' to let all users control it, otherwise must be root user', \
  )
parser.add_argument("led", metavar='led', choices=['skull','eyes','button','led1','led2','led3'], help="Specify the LED [skull|eyes|power|led1|led2|led3]") 	# naming it "led"
parser.add_argument("trigger", metavar='mode', choices=['pwr','hdd','eth','wifi','limit','off'], help="Set the mode [pwr|hdd|eth|wifi|limit|off]") 
parser.add_argument("-c","--colour", metavar='', help="Specify colour (use \'#rrggbb\')") 	# naming it "colour"
parser.add_argument("-b","--bright", metavar='', type=value0_100, help="Specify brightness percentage [0..100]")
parser.add_argument("-t","--type", metavar='', choices=['solid','pulse','flash','strobe'], help="Specify blinking type [solid|breath|pulse|strobe]")
parser.add_argument("-f","--freq", metavar='', type=value1_10, help="Specify blinks per second [1..10]")
parser.add_argument("--hdd", metavar='', type=value0_1, help="For HDD activity indication: 0=normally off, 1=normally on")
parser.add_argument("--lan", metavar='', type=value0_2, help="For Lan activity indication: 0=lan1, 1=lan2, 2=Lan1+Lan2")
parser.add_argument("--scheme", metavar='', type=value0_1, help="For Power-Limit indication: scheme 0=green to red, 1=single colour")

parser.add_argument("-v","--version", action='version', version='%(prog)s 0.1.1')
parser.add_argument("-h","--help", action='help', help="show this help message and exit")

args=parser.parse_args()

led_list = ['power','hdd','skull','eyes','led1','led2','led3']
trigger_list = ['pwr','hdd','eth','wifi','soft','limit','off']

led_dict = {'button':0, 'hdd':1, 'skull':2, 'eyes':3, 'led1':4, 'led2':5, 'led3':6} # on NUC8i7HVK & NUC8i7HNK hdd led doesn't exist?
trigger_dict = {'pwr':0, 'hdd':1, 'eth':2, 'wifi':3, 'soft':4, 'limit':5, 'off':6} # not using software trigger in this script
flash_dict = {'solid':0,'pulse':1,'flash':2,'strobe':3}

power_dict = {'bright':0, 'behavior':1, 'frequency':2, 'red':3, 'green':4, 'blue':5, 'name': 'power_dict'}
hdd_dict = {'bright':0,'red':1,'green':2,'blue':3,'hdd':4, 'name': 'hdd_dict'}
lan_dict = {'lan':0, 'bright':1, 'red':2, 'green':3, 'blue':4, 'name': 'lan_dict'}
wifi_dict = {'bright':0, 'red':1, 'green':2, 'blue':3, 'name': 'wifi_dict'}
limit_dict = {'scheme':0,'bright':1, 'red':2, 'green':3, 'blue':4, 'name': 'limit_dict'}
off_dict = {'name': 'not needed'}

if args.trigger == 'pwr': active_dict = power_dict
elif args.trigger == 'hdd': active_dict = hdd_dict
elif args.trigger == 'eth': active_dict = lan_dict
elif args.trigger == 'wifi': active_dict = wifi_dict
elif args.trigger == 'limit': active_dict = limit_dict
elif args.trigger == 'off': active_dict = off_dict

#Initialise all the cmd vars, whether they are all needed or not
led_cmd0 = led_cmd1 = led_cmd2 = led_cmd3 = led_cmd4 = led_cmd5 = led_cmd6 = led_cmd7 = ''

#always want to set the LED & trigger first, otherwise the other arguments may not be appropriate, nor in the right field
led_cmd0=('echo "set_indicator,{led},{trigger}"  | tee /proc/acpi/nuc_led \\\n').format(
  led=led_dict[args.led],
  trigger=trigger_dict[args.trigger])

#Brightness is the most common feature to all triggers
#If bright passed and trigger != off 
if args.bright != None and args.trigger != 'off':
  led_cmd1=('&& echo "set_indicator_value,{led},{trigger},{bright_bit},{value}"  | tee /proc/acpi/nuc_led \\\n').format(
    led=led_dict[args.led],trigger=trigger_dict[args.trigger],
    bright_bit=active_dict['bright'],
    value=args.bright)

#Colour is the next most common feature
#If colour passed and trigger != off.  NB: Spec seems to suggest that Power Limit Indicator only supports colour in Single Colour mode
if args.colour != None and args.trigger != 'off':
  # sanity check on colour, value should be between 0 and 0xFFFFFF, string passed should start with #, and be 7chars long
  if len(args.colour) != 7 or args.colour[0] !='#': 
    print("Colour passed is invalid - wrong length or does not start with \'#\'. Colour should be specified as #rrggbb")
    exit()
  else:
    try: int(args.colour.replace("#","0x"),0)
    except: print("Colour passed is invalid - contains other than 0-F"); exit()

  if args.trigger == 'limit':
    print("TODO: here we need to check that scheme is set to 1:single colour")
    exit()
  else: # rest of commands support colour
    h = args.colour.lstrip('#')
    (red, green, blue) = tuple(int(h[i:i+2], 16) for i in (0, 2 ,4))
    led_cmd2=('&& echo "set_indicator_value,{led},{trigger},{red_bit},{red_value}" | tee /proc/acpi/nuc_led \\\n'
    '&& echo "set_indicator_value,{led},{trigger},{green_bit},{green_value}" | tee /proc/acpi/nuc_led \\\n'
    '&& echo "set_indicator_value,{led},{trigger},{blue_bit},{blue_value}" | tee /proc/acpi/nuc_led \\\n').format(
    led=led_dict[args.led],trigger=trigger_dict[args.trigger],
    red_bit=active_dict['red'],
    green_bit=active_dict['green'],
    blue_bit=active_dict['blue'],
    red_value = red,
    green_value = green,
    blue_value = blue)

# Type of blinking indication, choices=['solid','breath','pulse','strobe'] and Frequency of blinking indication, in blinks per second [1..10]
# Just power indication (and software, which we aren't implementing yet)
if args.trigger == 'pwr' or args.trigger == 'soft':
  if args.type != None:
    led_cmd3=('&& echo "set_indicator_value,{led},{trigger},{blink_bit},{blink_value}" | tee /proc/acpi/nuc_led \\\n').format(
    led=led_dict[args.led],trigger=trigger_dict[args.trigger],
    blink_bit=active_dict['behavior'],
    blink_value = flash_dict[args.type])
  if args.freq != None:
    led_cmd4=('&& echo "set_indicator_value,{led},{trigger},{freq_bit},{freq_value}" | tee /proc/acpi/nuc_led \\\n').format(
    led=led_dict[args.led],trigger=trigger_dict[args.trigger],
    freq_bit=active_dict['frequency'],
    freq_value = args.freq)

# Now the special cases... 
# limit has a scheme
if args.trigger == 'limit' and args.scheme != None:
    led_cmd5=('&& echo "set_indicator_value,{led},{trigger},{scheme_bit},{scheme_value}" | tee /proc/acpi/nuc_led \\\n').format(
    led=led_dict[args.led],trigger=trigger_dict[args.trigger],
    scheme_bit=active_dict['scheme'],
    scheme_value = args.scheme)

# eth has options of one, other or both
if args.trigger == 'eth' and args.lan != None:
    led_cmd5=('&& echo "set_indicator_value,{led},{trigger},{lan_bit},{lan_value}" | tee /proc/acpi/nuc_led \\\n').format(
    led=led_dict[args.led],trigger=trigger_dict[args.trigger],
    lan_bit=active_dict['lan'],
    lan_value = args.lan)
  

final_cmd=led_cmd0 + led_cmd1 + led_cmd2 + led_cmd3 + led_cmd4 + led_cmd5 + led_cmd6 + '&& echo "Done!"'
process = subprocess.call(final_cmd, shell=True)

exit()



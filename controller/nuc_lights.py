#!/usr/bin/env python3
import subprocess
import time

dictIndicator = {
    'power':0,
    'hddio':1,
    'netio':2,
    'wifi':3,
    'power_limit':5,
    'off':6
}

dictLed = {
    'button':0,
    'skull':2,
    'eyes':3,
    'f1':4,
    'f2':5,
    'f3':6,
}

dictPowerStateIndicator = {
    'brightness':0,
    'behavior':1,
    'frequency':2,
    'red':3,
    'green':4,
    'blue':5    
}

dictHDDIndicator = {
    'brightness':0,
    'red':1,
    'green':2,
    'blue':3,
    'behavior':4 
}

dictNetworkIndicator = {
    'behavior':0,
    'brightness':1,
    'red':2,
    'green':3,
    'blue':4 
}

dictWIFIIndicator = {
    'brightness':0,
    'red':1,
    'green':2,
    'blue':3
}

def setLEDIndicatorColor(led, indicator, brightness, hexCode):
    h = hexCode.lstrip('#')
    (red, green, blue) = tuple(int(h[i:i+2], 16) for i in (0, 2 ,4))

    if indicator == 'power': dictToUse = dictPowerStateIndicator
    elif indicator == 'hddio': dictToUse = dictHDDIndicator
    elif indicator == 'netio': dictToUse = dictNetworkIndicator
    elif indicator == 'wifi': dictToUse = dictWIFIIndicator
    else: return

    cmd = (
        'echo "set_indicator_value,{led},{indicator},{brightBit},{brightness}" | tee /proc/acpi/nuc_led && '
        'echo "set_indicator_value,{led},{indicator},{redBit},{red}" | tee /proc/acpi/nuc_led && '
        'echo "set_indicator_value,{led},{indicator},{greenBit},{green}" | tee /proc/acpi/nuc_led && '
        'echo "set_indicator_value,{led},{indicator},{blueBit},{blue}" | tee /proc/acpi/nuc_led ').format(
                led = dictLed[led],
                indicator=dictIndicator[indicator],
                brightness = brightness,
                brightBit=dictToUse['brightness'],
                redBit=dictToUse['red'],
                greenBit=dictToUse['green'],
                blueBit=dictToUse['blue'],
                red = red,
                green = green,
                blue = blue)
   

    process = subprocess.call(cmd, shell=True)
    return


def setLEDSource(led, indicator):
    cmd = "echo \"set_indicator,%s,%s\" | tee /proc/acpi/nuc_led" % (dictLed[led], dictIndicator[indicator])
    process = subprocess.call(cmd, shell=True)
    return

def setLEDColorAndSource(led, indicator, brightness, hexCode):
    setLEDSource(led,indicator)
    setLEDIndicatorColor(led, indicator,brightness,hexCode)
    return

            
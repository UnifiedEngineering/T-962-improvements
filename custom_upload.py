Import("env")
import os
import platform
from platformio.builder.tools.pioupload import AutodetectUploadPort

config = env.GetProjectConfig()
mcu_clock = config.get("upload_settings", "MCU_CLOCK")
flash_baud = config.get("upload_settings", "FLASH_BAUD")
#print("MCU_CLOCK: %s" % str(mcu_clock))

# Python callback
def on_upload(source, target, env):
    AutodetectUploadPort(env)
    #print(source, target)
    firmware_path = str(source[0])
    # replace .bin with .hex. Upload needs .hex not .bin
    firmware_path = firmware_path[:-3] + "hex"
    print("Firmware path: %s" % firmware_path)
    # find out what executable to call
    uploader = "./lpc21isp_linux"
    if platform.system() == "Windows":
        uploader = "lpc21isp_win.exe"
    # do something
    env.Execute("%s \"%s\" %s %s %s" % (uploader, firmware_path, env.subst("$UPLOAD_PORT"), flash_baud, mcu_clock))

env.Replace(UPLOADCMD=on_upload)
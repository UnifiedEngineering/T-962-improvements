import subprocess
import os
Import("env")

def create_version_c(*args, **kwargs):
    print("Creating version.c file from git describe..")
    try:
        git_tag_cmd = "git describe --tag --always --dirty"
        git_tag = subprocess.check_output(git_tag_cmd).decode('utf-8').strip()
        print("Got tag: %s" % git_tag)
        file_content = 'const char* Version_GetGitVersion(void) { return "%s"; }' % git_tag
        with open("src/version.c", 'w') as out_file:
            out_file.write(file_content)
        print("Writing version.c okay.")
    except Exception as exc:
        print("Exception during version.c occured: " % str(exc))
        os.remove("src/version.c")

create_version_c()

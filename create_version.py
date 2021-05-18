import subprocess
import os
import traceback
import shutil
Import("env")

def create_version_c(*args, **kwargs):
    print("Creating version.c file from git describe..")
    # sanity check: was this downlaoded via git? 
    if not os.path.isdir(".git"):
        print("Aborting creation of version.c since this project was not cloned via `git`...")
        return
    # sanity check: do we have git?
    if shutil.which("git") is None:
        print("Command `git` is not available, aborting creation of src/version.c..")
        return
    try:
        git_tag_cmd = ["git", "describe", "--tag", "--always", "--dirty"]
        git_tag = subprocess.check_output(git_tag_cmd).decode('utf-8').strip()
        print("Got tag: %s" % git_tag)
        file_content = 'const char* Version_GetGitVersion(void) { return "%s"; }' % git_tag
        with open("src/version.c", 'w') as out_file:
            out_file.write(file_content)
        print("Writing version.c okay.")
    except Exception as exc:
        print("Exception during creation of version.c occured: %s" % str(exc))
        print(traceback.format_exc())
        os.remove("src/version.c")

create_version_c()

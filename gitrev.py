import subprocess
import time

# Allow configuration of the release branch in forked repositories which may
# not want to release from 'main' (which should track upstream directly) and
# instead have a separate 'release' branch containing their merged changes.
RELEASE_BRANCH='main'

# get tag/base version
version = tag = subprocess.check_output("git describe --tags --abbrev=0", shell=True).decode().strip()

# get current revision hash
commit = subprocess.check_output("git log --pretty=format:%h -n 1", shell=True).decode().strip()

# get branch name
branch = subprocess.check_output("git rev-parse --abbrev-ref HEAD", shell=True).decode().strip()
# if not release branch append branch name
if branch != RELEASE_BRANCH:
  version += "-[" + branch + "]"

# check if clean
clean = subprocess.check_output("git status -uno --porcelain", shell=True).decode().strip()
ts = time.strftime('%Y%m%d%H%M%S')
# if not clean append timestamp
if clean != "":
  version += "-" + ts

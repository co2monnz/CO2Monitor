import gitrev

# print build flags
print("'-DAPP_TAG=\"{0}\"'".format(gitrev.tag))
print("'-DAPP_VERSION=\"{0}\"'".format(gitrev.version))
print("'-DSRC_REVISION=\"{0}\"'".format(gitrev.commit))
print("'-DSRC_BRANCH=\"{0}\"'".format(gitrev.branch))
print("'-DBUILD_TIMESTAMP=\"{0}\"'".format(gitrev.ts))

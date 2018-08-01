import subprocess

tag = subprocess.check_output(["git", "describe", "--tags", "--long"]).strip().split("-")

version_tag = "%s-%s" % (tag[0], tag[1]) 
revision_tag = tag[2]

print "-DAPP_VERSION=%s -DAPP_REVISION=%s" % (version_tag, revision_tag)
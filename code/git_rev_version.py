import subprocess
Import("env")

tag = subprocess.check_output(["git", "describe", "--tags", "--long"]).strip().split("-")

version_tag = "%s-%s" % (tag[0], tag[1]) 
revision_tag = tag[2]

flag1 = "'-DAPP_VERSION=\"%s\"'" % version_tag[1:] #[1:] removes the 'v' (1st char)
flag2 = "'-DAPP_REVISION=\"%s\"'" % revision_tag

print "Adding Build Flags: %s %s" % (flag1, flag2)

env.Append(
  BUILD_FLAGS=[
      flag1,
      flag2
  ]
)
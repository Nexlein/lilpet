# ESP-IDF used to build the firmware, locally and in CI (mise run firmware:build): the latest release, pinned here
# only, by its tag and by the digest of the image the tag named then, since a tag can be moved. Dependabot updates both
# in a pull request when a new release is out, and CI checks it before it lands. From a three-part tag such as v6.1.1
# it proposes the bugfix releases (v6.1.2) and the next minor one (v6.2). From a two-part tag such as v6.2 it proposes
# the next minor release only: when its first bugfix release is out (v6.2.1), pin it by hand.
FROM espressif/idf:v6.1.1@sha256:2b4f966c37eaedc19542b0637a5f850b2eff9478d8e858192aa0073d92c2dc91
